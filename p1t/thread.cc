#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <queue>
#include <map>
#include <iostream>

using namespace std;
using std::queue;
using std::pair;
using std::map;


void STUB(thread_startfunc_t func, void* arg);

int thread_libinit(thread_startfunc_t func, void *arg); //want to exit
int thread_create(thread_startfunc_t func, void *arg); //want to exit
int thread_yield(void); // call switch
int thread_lock(unsigned int lock); //call switch
int thread_unlock(unsigned int lock); 
int thread_wait(unsigned int lock, unsigned int cond); //call switch
int thread_signal(unsigned int lock, unsigned int cond);
int thread_broadcast(unsigned int lock, unsigned int cond);
static int switch_thread();
static int exit_thread();
static void pop_thread();
static int delete_thread();


// Structure which represents thread.
struct TCB {
    int tid; // Thread identifier number.
    int status; //Thread state: 0 is ready, 1 is blocked for lock, 2 is waiting for CV, 3 is finished.
    ucontext_t* ucontext; // Thread context.
};

static queue<TCB*> READY_QUEUE;
//map of queues for multiple locks/cvs
static map<unsigned int, queue<TCB*> > LOCK_QUEUE_MAP;
static map<pair<unsigned int, unsigned int>, queue<TCB*> > CV_QUEUE_MAP;
static map<unsigned int, TCB*> LOCK_OWNER_MAP; //maps the lock id to its owner

static int THREAD_COUNT = 0; //total number of thread

static TCB* RUNNING_THREAD;
static TCB* PREVIOUS_THREAD;
static TCB* LIBINIT_THREAD;
static TCB* DELETE_THREAD;
static bool t_init = false;

/**
 * Method thread_libinit initializes the thread library. A user program should call thread_libinit exactly
 * once (before calling any other thread functions). Library must not be initialized more than once. 
 * Creates and runs first thread, which calls the starting function it points to and this method will never
 * return to its caller, and control with transfer completely to the starting function of the main application 
 * thread.
 */ 
int thread_libinit(thread_startfunc_t func, void *arg) {
    interrupt_disable();
    interrupt_enable();
    // If the library has already been initialized, do not reinitialize.
    if (t_init) {
        printf("Thread library already initialized. Do not attempt initialization again.");
        //interrupt_enable();
        return -1;
    } else {
        t_init = true;
    }
    
    // Re-enable interrupts before new thread creation.
    //interrupt_enable();

    // Create libinit thread.
    LIBINIT_THREAD = new TCB;
    char *stack1 = new char[STACK_SIZE];
    ucontext_t *context1 = new ucontext_t;
    getcontext(context1);
    context1->uc_stack.ss_sp = stack1;
    context1->uc_stack.ss_size = STACK_SIZE;
    context1->uc_stack.ss_flags = 0;
    context1->uc_link = NULL;
    makecontext(context1,(void(*)()) func, 1, arg);
    LIBINIT_THREAD->ucontext = context1;


    // create delete thread
    DELETE_THREAD = new TCB;
    char *stack = new char[STACK_SIZE];
    ucontext_t *context = new ucontext_t;
    getcontext(context);
    context->uc_stack.ss_sp = stack;
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = NULL;
    makecontext(context, (void (*)()) delete_thread, 0);
    DELETE_THREAD->ucontext = context;

    RUNNING_THREAD = LIBINIT_THREAD;

    //interrupt_enable();
    func(arg);
    //interrupt_disable();

    exit_thread();
}


void STUB(thread_startfunc_t func, void* arg){
    // We enable interrupts because this is the method which runs after any swapcontext.
    // This method is the core of the thread's execution, and thus interrupts must be allowed, like
    // forced yielding.
    //interrupt_enable();

    // Run the function.
    func(arg);

    // After execution, we disable interrupts to clean up and exit the thread and to ensure it happens.
    //interrupt_disable();

    // The thread is finished. We can exit it.
    RUNNING_THREAD->status = 3;
    exit_thread(); //question? what is thread_exit?
}


static int switch_thread() {
    //interrupt_disable();
    // If the ready queue is not empty, we should switch thread to the head of the ready queue.
    if (!READY_QUEUE.empty()) {
        pop_thread();
        return swapcontext(PREVIOUS_THREAD->ucontext, RUNNING_THREAD->ucontext);
    }
    // If the ready queue is empty, we have nothing to go to. 
    if (RUNNING_THREAD != NULL) {
        // Delete this thread.
    }
    //interrupt_enable();
    cout << "Thread library exiting.\n";
    exit(0);
    
    return 0;
}

/**
 * Exits a thread, called at the end of thread_libinit if we ever get there, and most definitely
 * after STUB (the function) finishes running, we must mark the thread finished and switch threads.
 */ 
static int exit_thread() {
    //interrupt_disable();
    //If the ready queue is not empty, we should switch thread to the head of the ready queue.
    if (!READY_QUEUE.empty()) {
        pop_thread();
        return swapcontext(PREVIOUS_THREAD->ucontext, DELETE_THREAD->ucontext);
    } else {
        //ininterrupt_enable();
        cout << "Thread library exiting.\n";
        exit(0);
    }
    return 0;
}

static void pop_thread() {
    // The next thread to run is the head of the ready queue.
    //NEXT_THREAD = READY_QUEUE.front();
    // Save the running thread as previous.
    PREVIOUS_THREAD = RUNNING_THREAD;
    // Set the new running thread to be the next thread.
    RUNNING_THREAD = READY_QUEUE.front();
    // Pop it off.
    READY_QUEUE.pop();
}

static int delete_thread() {
    while(1) {
        if (PREVIOUS_THREAD != NULL) {
              PREVIOUS_THREAD->ucontext->uc_stack.ss_sp = NULL;
              PREVIOUS_THREAD->ucontext->uc_stack.ss_size = 0;
              PREVIOUS_THREAD->ucontext->uc_stack.ss_flags = 0;
              PREVIOUS_THREAD->ucontext->uc_link = NULL;
              delete (char*) PREVIOUS_THREAD->ucontext->uc_stack.ss_sp;
              delete PREVIOUS_THREAD->ucontext;
              delete PREVIOUS_THREAD;
        }
        int error = swapcontext(DELETE_THREAD->ucontext, RUNNING_THREAD->ucontext);
        if (error == -1) {
            return error;
        }
    }
    return 0;
}


int thread_create(thread_startfunc_t func, void *arg) {
    //interrupt_disable();

    if (!t_init) {
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }
    /**
     * Follow steps on slide 39 of Recitation 2/5.
     * 1) Allocate thread control block.
     * 2) Allocate stack.
     * 3) Build stack frame for base of stack (stub)
     * 4) Put func, args on stack
     * 5) Put thread on ready queue.
     * 6) Run thread at some point.
     */ 

    // Allocate thread control block.
    TCB* thread = new TCB;

    // Allocate stack.
    char *stack = new char[STACK_SIZE];
    
    // From the lab specification, for creating a ucontext.
    ucontext_t *context = new ucontext_t;
    getcontext(context);
    context->uc_stack.ss_sp = stack;
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = NULL;

    // Put func, args on stack by "doctoring" register values.
    makecontext(context, (void (*)()) STUB, 2, func, arg);

    // Set thread context.
    thread->ucontext = context;

    // Set thread status at ready.
    thread->status = 0;

    // Set thread identification number.
    thread->tid = THREAD_COUNT;
    THREAD_COUNT++;

    // Put thread on ready queue.
    READY_QUEUE.push(thread);

    //interrupt_enable();
    return 0;
}

int thread_yield(void){
    //caller goes to the tail of ready queue another thread from the front of readyq runs
    //interrupt_disable();
    if (t_init == false){
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }
    READY_QUEUE.push(RUNNING_THREAD);
    if (switch_thread() == -1) {
        //interrupt_enable();
        return -1;
    }
    //interrupt_enable();
    return 0;
}

int thread_lock(unsigned int lock){
    // Thread lock must not be interrupted because then perhaps two threads might end up holding
    // the lock.
    //interrupt_disable();
    TCB* owner = LOCK_OWNER_MAP[lock]; //if key lock is not existant in lock_owner_map, NULL is set as its value
    if (t_init == false || owner == RUNNING_THREAD) {
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }
    //check if there is a queue for the lock in the lock queue map if not add one
    if (LOCK_QUEUE_MAP.find(lock) == LOCK_QUEUE_MAP.end()){
        // key not found in the map, initialize a queue for this lock
        LOCK_OWNER_MAP[lock] = NULL; //owner is NULL, initiated under the while loop
        queue<TCB*> new_LOCK_QUEUE;
        LOCK_QUEUE_MAP.insert(pair<unsigned int, queue<TCB*> >(lock, new_LOCK_QUEUE) );
    } 
    //while this monitor is not free put my TCB on this monitor lock list and switch
    while (owner != NULL){
        LOCK_QUEUE_MAP[lock].push(RUNNING_THREAD);
    }
    //if this monitor is free set current thread as owner of monitor
    LOCK_OWNER_MAP[lock] = RUNNING_THREAD;
    return 0;
}



int thread_unlock(unsigned int lock){
    //NEED MONITORS
    //caller releases lock and continues running
    //If the lock queue is not empty, then wake up a thread by moving it from the head of the lock queue to the tail of the ready queue
    //interrupt_disable();

    if (t_init == false){
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }
    LOCK_OWNER_MAP[lock] = NULL; //releases owner
    if (!LOCK_QUEUE_MAP[lock].empty()){
        READY_QUEUE.push(LOCK_QUEUE_MAP[lock].front());
        LOCK_QUEUE_MAP[lock].pop();
    }
    //interrupt_enable();
    return 0;
}


int thread_wait(unsigned int lock, unsigned int cond){
    //NEED MONITORS
    //interrupt_disable();
    if (t_init == false){
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }
    //caller unlocks the lock
    if (thread_unlock(lock) == -1) {
        //interrupt_enable();
        return -1;
    }
    //if CV queue not intialized, initialize it
    if (CV_QUEUE_MAP.find(make_pair(lock,cond)) == CV_QUEUE_MAP.end()){
        queue<TCB*> NEW_CV_QUEUE;
        CV_QUEUE_MAP[make_pair(lock,cond)] = NEW_CV_QUEUE;
    }
    //move current thread to tail of CV queue of this lock
    CV_QUEUE_MAP[make_pair(lock,cond)].push(RUNNING_THREAD);
    //thread from the front of ready queue runs
    if (switch_thread() == -1) {
        //interrupt_enable();
        return -1;
    }
    if (thread_lock(lock) == -1) {
        //interrupt_enable();
        return -1;
    }
    //interrupt_enable();
    return 0;
}


int thread_signal(unsigned int lock, unsigned int cond){
    //if CV queue not empty, head of cv queue goes to the tail of ready queue
    //interrupt_disable();
    if (t_init == false){
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }
    //get waiter TCB from CV queue
    pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
    if (!CV_QUEUE_MAP[lock_cond_pair].empty()){
        READY_QUEUE.push(CV_QUEUE_MAP[lock_cond_pair].front());
        CV_QUEUE_MAP[lock_cond_pair].pop();
    }
    //interrupt_enable();
    return 0;
}


int thread_broadcast(unsigned int lock, unsigned int cond) {
    //interrupt_disable();
    if (t_init == false) {
        printf("Thread library must be initialized first. Call thread_libinit first.");
        //interrupt_enable();
        return -1;
    }    
    pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
    //get all the waiter in CV queue and put it in ready queue
    while (!CV_QUEUE_MAP[lock_cond_pair].empty()){
        READY_QUEUE.push(CV_QUEUE_MAP[lock_cond_pair].front());
        CV_QUEUE_MAP[lock_cond_pair].pop();
    }
    //interrupt_enable();
    return 0;
}



