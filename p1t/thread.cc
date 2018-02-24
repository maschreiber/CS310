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


void STUB(thread_startfunc_t func, void* arg);
int thread_libinit(thread_startfunc_t func, void *arg); //want to exit
int thread_create(thread_startfunc_t func, void *arg); //want to exit
int thread_yield(void); // call switch
int thread_lock(unsigned int lock); //call switch
int thread_unlock(unsigned int lock);
int thread_wait(unsigned int lock, unsigned int cond); //call switch
int thread_signal(unsigned int lock, unsigned int cond);
int thread_broadcast(unsigned int lock, unsigned int cond);
void print_ready_queue();
void print_lock_queue();
void print_cv_queue();
int thread_unlock_free(unsigned int lock);

static void delete_thread();

// Structure which represents thread.
struct TCB {
  int tid; // Thread identifier number.
  int status; //Thread state: 0 is ready, 1 is blocked for lock, 2 is waiting for CV, 3 is finished.
  ucontext_t* ucontext; // Thread context.
};

static int switch_thread(TCB* a, TCB* b);

static queue<TCB*> READY_QUEUE;
//map of queues for multiple locks/cvs
static map<unsigned int, queue<TCB*> > LOCK_QUEUE_MAP;
static map<pair<unsigned int, unsigned int>, queue<TCB*> > CV_QUEUE_MAP;
static map<unsigned int, TCB*> LOCK_OWNER_MAP; //maps the lock id to its owner

static int THREAD_COUNT = 0; //total number of thread

static TCB* RUNNING_THREAD;
static TCB* PREVIOUS_THREAD;
static TCB* DELETE_THREAD;
static bool t_init = false;

static bool INTERRUPT = true;

void interrupt_enable2() {
  //if (INTERRUPT == false){ interrupt_enable(); INTERRUPT = true; }
}

void interrupt_disable2() {
  //if (INTERRUPT == true){ interrupt_disable(); INTERRUPT = false; }
}

/**
* Method thread_libinit initializes the thread library. A user program should call thread_libinit exactly
* once (before calling any other thread functions). Library must not be initialized more than once.
* Creates and runs first thread, which calls the starting function it points to and this method will never
* return to its caller, and control with transfer completely to the starting function of the main application
* thread.
*/
int thread_libinit(thread_startfunc_t func, void *arg) {
  // If the library has already been initialized, do not reinitialize.
  if (t_init) {
    printf("Thread library already initialized. Do not attempt initialization again.");
    return -1;
  } else {
    t_init = true;
  }

  // Create a new thread.
  thread_create(func, arg);

  // Get the newly created thread. Set the current running thread to it.
  RUNNING_THREAD = READY_QUEUE.front();
  READY_QUEUE.pop();

  DELETE_THREAD = new TCB;
  char *stack = new char[STACK_SIZE];
  ucontext_t *context;
  try{
    context = new ucontext_t;
    getcontext(context);
    context->uc_stack.ss_sp = stack;
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = NULL;
    DELETE_THREAD->ucontext = context;
    DELETE_THREAD->status = 0;
    DELETE_THREAD->tid = THREAD_COUNT;
    THREAD_COUNT++;
  } catch(bad_alloc b) {
    context->uc_stack.ss_sp = NULL;
    delete DELETE_THREAD->ucontext;
    delete DELETE_THREAD;
    return -1;
  }

  interrupt_disable2();
  switch_thread(DELETE_THREAD, RUNNING_THREAD);
  //interrupt_enable2();

  while (!READY_QUEUE.empty()) {
    if (RUNNING_THREAD->status == 3) {
      delete_thread();
    }
    RUNNING_THREAD = READY_QUEUE.front();
    READY_QUEUE.pop();
    switch_thread(DELETE_THREAD, RUNNING_THREAD);
  }

  while (RUNNING_THREAD != NULL) {
    delete_thread();
  }

  //When there are no runnable threads in the system (e.g. all threads have
  //finished, or all threads are deadlocked)
  //interrupt_enable2();
  cout << "Thread library exiting.\n";
  exit(0);
}


void STUB(thread_startfunc_t func, void* arg){
  // We enable interrupts because this is the method which runs after any swapcontext.
  // This method is the core of the thread's execution, and thus interrupts must be allowed, like
  // forced yielding.
  //interrupt_enable2(); BADENABLE
  // Run the function.
  func(arg);
  // After execution, we disable interrupts to clean up and exit the thread and to ensure it happens.
  interrupt_disable2();
  // The thread is finished. We can exit it.
  RUNNING_THREAD->status = 3;
  switch_thread(RUNNING_THREAD, DELETE_THREAD);
}


static int switch_thread(TCB* a, TCB* b) {
  swapcontext(a->ucontext, b->ucontext);
}

static void delete_thread() {
  delete (char*) RUNNING_THREAD->ucontext->uc_stack.ss_sp;
  RUNNING_THREAD->ucontext->uc_stack.ss_sp = NULL;
  RUNNING_THREAD->ucontext->uc_stack.ss_size = 0;
  RUNNING_THREAD->ucontext->uc_stack.ss_flags = 0;
  RUNNING_THREAD->ucontext->uc_link = NULL;
  delete RUNNING_THREAD->ucontext;
  delete RUNNING_THREAD;
  RUNNING_THREAD = NULL;
}


int thread_create(thread_startfunc_t func, void *arg) {
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
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
  ucontext_t *context;
  try{
    context = new ucontext_t;
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
    READY_QUEUE.push(thread);
    //cout << "Created thread and pushed to end of ready queue: " << thread << "\n";
  } catch(bad_alloc b){
    context->uc_stack.ss_sp = NULL;
    delete thread->ucontext;
    delete thread;
    delete stack;
    interrupt_enable2();
    return -1;
  }

  // Put thread on ready queue.
  interrupt_enable2(); //BADENABLE
  return 0;
}

int thread_yield(void) {
  // Disable interrupts to allow proper switching of threads.
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
    return -1;
  }
  // Push the running thread to the end of the ready queue.
  READY_QUEUE.push(RUNNING_THREAD);
  // Switch threads (switch_thread will swap to the first thread on ready queue).
  switch_thread(RUNNING_THREAD, DELETE_THREAD);
  // Re-enable interrupts when we return to this thread for safety.
  interrupt_enable2(); // BADENABLE
  return 0;
}

int thread_lock(unsigned int lock){
  // Thread lock must not be interrupted because two threads might end up holding lock.
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
    return -1;
  }
  // Calling for the lock while holding it is an error.
  if (LOCK_OWNER_MAP[lock] == RUNNING_THREAD) {
    interrupt_enable2();
    return -1;
  }

  // Check if there is a queue for the lock in the lock queue map, and if not -- add one.
  if (LOCK_QUEUE_MAP.count(lock) == 0) {
    LOCK_OWNER_MAP[lock] = NULL; // Lock has no owner yet.
    queue<TCB*> NEW_LOCK_QUEUE; // Create empty queue.
    LOCK_QUEUE_MAP.insert(pair<unsigned int, queue<TCB*> >(lock, NEW_LOCK_QUEUE) ); // Insert.
  }
  // If the lock is owned by another thread.
  if (LOCK_OWNER_MAP[lock] != NULL) {
    LOCK_QUEUE_MAP[lock].push(RUNNING_THREAD); // Push current thread to end of ready queue.
    switch_thread(RUNNING_THREAD, DELETE_THREAD); // Switch thread to run the head of the ready queue.
  } else {
    LOCK_OWNER_MAP[lock] = RUNNING_THREAD; // Give lock to this thread.
  }

  // We can re-enable interrupts for forced yields.
  interrupt_enable2();
  return 0;
}

int thread_unlock_free(unsigned int lock){
  // If lock isn't found, is null, or isn't owned by the running thread, we cannot unlock.
  if (LOCK_OWNER_MAP.count(lock) == 0 || LOCK_OWNER_MAP[lock] == NULL || LOCK_OWNER_MAP[lock] != RUNNING_THREAD) {
    interrupt_enable2();
    return -1;
  }

  // Releases the lock owner.
  LOCK_OWNER_MAP[lock] = NULL;

  // If the lock queue is not empty:
  if (!LOCK_QUEUE_MAP[lock].empty()) {
    READY_QUEUE.push(LOCK_QUEUE_MAP[lock].front()); // Pushed blocked thread to ready queue.
    LOCK_OWNER_MAP[lock] = LOCK_QUEUE_MAP[lock].front(); // Hand-off lock: Piazza @439
    LOCK_QUEUE_MAP[lock].pop(); // Remove thread from blocked queue.
  }
}

int thread_unlock(unsigned int lock){
  // Disable interrupts for successful unlocks.
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
    return -1;
  }
  thread_unlock_free(lock);
  // We can re-enable interrupts for forced yields.
  interrupt_enable2();
  return 0;
}

int thread_wait(unsigned int lock, unsigned int cond) {
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
    return -1;
  }

  // We must have a successful unlock.
  if (thread_unlock_free(lock) == -1) {
    interrupt_enable2();
    return -1;
  }
  // If CV waiting queue is not initialized, we initialize it.
  pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
  if (CV_QUEUE_MAP.find(lock_cond_pair) == CV_QUEUE_MAP.end()){
    queue<TCB*> NEW_CV_QUEUE;
    CV_QUEUE_MAP[lock_cond_pair] = NEW_CV_QUEUE;
  }
  // Push thread to tail of CV waiting queue.
  CV_QUEUE_MAP[lock_cond_pair].push(RUNNING_THREAD);

  // Switch thread so that thread from the front of ready queue runs.
  switch_thread(RUNNING_THREAD, DELETE_THREAD);
  interrupt_enable2();
  // After returning from swapcontext and being awoken, we must first reacquire the lock.
  return thread_lock(lock);
}

int thread_signal(unsigned int lock, unsigned int cond){
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
    return -1;
  }
  // Take first waiter from CV wait queue and push to end of ready queue.
  pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
  if (!CV_QUEUE_MAP[lock_cond_pair].empty()){
    READY_QUEUE.push(CV_QUEUE_MAP[lock_cond_pair].front());
    CV_QUEUE_MAP[lock_cond_pair].pop();
  };
  interrupt_enable2(); // BADENABLE
  return 0;
}

int thread_broadcast(unsigned int lock, unsigned int cond) {
  interrupt_disable2();
  if (!t_init) {
    printf("Thread library must be initialized first. Call thread_libinit first.");
    interrupt_enable2();
    return -1;
  }
  // Take all waiters from CV wait queue and push to end of ready queue.
  pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
  while (!CV_QUEUE_MAP[lock_cond_pair].empty()){
    READY_QUEUE.push(CV_QUEUE_MAP[lock_cond_pair].front());
    CV_QUEUE_MAP[lock_cond_pair].pop();
  }
  interrupt_enable2();
  return 0;
}
