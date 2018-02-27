#include <cstdlib>
#include <ucontext.h>
#include <queue>
#include <iterator>
#include <map>
#include <iostream>
#include "interrupt.h"
#include "thread.h"
using namespace std;


static void STUB(thread_startfunc_t func, void* arg);
int thread_libinit(thread_startfunc_t func, void *arg); //want to exit
int thread_create(thread_startfunc_t func, void *arg); //want to exit
int thread_yield(void); // call switch
int thread_lock(unsigned int lock); //call switch
int thread_unlock(unsigned int lock);
int thread_wait(unsigned int lock, unsigned int cond); //call switch
int thread_signal(unsigned int lock, unsigned int cond);
int thread_broadcast(unsigned int lock, unsigned int cond);
static void cleanup();
static void process(thread_startfunc_t func, void* arg);
static void swapToSwitchThread();
static void switchtorunningthread();

// TCB contains user context and thread status.
struct TCB {
  ucontext_t* ucontext; // Contains stack pointer to simulate thread switching.
  int status; // 0 for not finished, 3 for cleanup (1 and 2 were supposed to be for lock/CV block respectivelty but not implemented)
};

// Global variable keeps tracking of the currently running thread.
static TCB* RUNNING_THREAD;

// Thread which handles switching of threads. Also handles cleanup upon completion of execution.
static TCB* SWITCH_THREAD;

// Ready queue holds all threads which are ready.
static queue<TCB*> READY_QUEUE;

// Lock queue map is a structure to hold a queue of threads waiting for a specific lock
static map<unsigned int, queue<TCB*> > LOCK_QUEUE_MAP;

// CV wait queue map is a structure to hold a queue of threads waiting for a signal of a lock, condiition variable pair.
static map<pair<unsigned int, unsigned int>, queue<TCB*> > CV_QUEUE_MAP;

// Lock owner map tells us which thread owns a specific lock.
static map<unsigned int, TCB*> LOCK_OWNER_MAP;

// No thread library calls can be made without initializing the library first through thread_libint(...);
static bool islib = false;

// Created new functions for interrupts so we could globally disable and enable for testing by commenting out the method body.
void interrupt_enable2() {
  interrupt_enable();
}

void interrupt_disable2() {
  interrupt_disable();
}

// Shorter call to swap to the switch thread from the running thread.
static void swapToSwitchThread(){
  swapcontext(RUNNING_THREAD->ucontext, SWITCH_THREAD->ucontext);
}

// Shorter call to swap to the running thread from the switch thread.
static void swapToRunningThread(){
  swapcontext(SWITCH_THREAD->ucontext, RUNNING_THREAD->ucontext);
}

// Cleans up the thread if it is not null by deleting the stack, ucontext, and thread itself.
static void cleanup() {
  if (RUNNING_THREAD == NULL) {
    return;
  }
  if (RUNNING_THREAD->status == 3){
    delete (char*) RUNNING_THREAD->ucontext->uc_stack.ss_sp;
    RUNNING_THREAD->ucontext->uc_stack.ss_sp = NULL;
    RUNNING_THREAD->ucontext->uc_stack.ss_size = 0;
    RUNNING_THREAD->ucontext->uc_stack.ss_flags = 0;
    RUNNING_THREAD->ucontext->uc_link = NULL;
    delete RUNNING_THREAD->ucontext;
    delete RUNNING_THREAD;
    RUNNING_THREAD = NULL;
  }
}


static void process(thread_startfunc_t func, void *arg) {

  //switch to RUNNING_THREAD thread to start func
  //interrupt_disable2();
  //switchtorunningthread();

  // While the ready queue still has threads to run.
  while (!READY_QUEUE.empty()) {
    // Always try to delete thread if it is done.
    cleanup();
    // RUNNING_THREAD is set to be the head of next ready queue.
    RUNNING_THREAD = READY_QUEUE.front();
    READY_QUEUE.pop();

    // swapcontext into next thread from this thread
    swapToRunningThread();
  }
  // At this point, ALL threads are done running or deadlocked.
  // Do one last cleanup call.
  cleanup();
  // Exit.
  cout << "Thread library exiting.\n";
  exit(0);
}

// Initializes the thread library.
int thread_libinit(thread_startfunc_t func, void *arg) {
  if (islib) {
    // printf("Thread library cannot be reinitialized.");
    return -1;
  }

  islib = true;

  // Code from specification to set up a new thread. We will initialize the SWITCH_THREAD first.
  try {
    // Initialize switch thread struct variables.
    SWITCH_THREAD = new TCB;
    SWITCH_THREAD->status = 0;

    // Set up the ucontext.
    SWITCH_THREAD->ucontext = new ucontext_t;
    getcontext(SWITCH_THREAD->ucontext);
    SWITCH_THREAD->ucontext->uc_stack.ss_sp = new char [STACK_SIZE];
    SWITCH_THREAD->ucontext->uc_stack.ss_size = STACK_SIZE;
    SWITCH_THREAD->ucontext->uc_stack.ss_flags = 0;
    SWITCH_THREAD->ucontext->uc_link = NULL;
    makecontext(SWITCH_THREAD->ucontext, (void (*)()) process, 2, func, arg);
  }
  catch (bad_alloc b) {
    delete (char*) SWITCH_THREAD->ucontext->uc_stack.ss_sp;
    delete SWITCH_THREAD->ucontext;
    delete SWITCH_THREAD;
    return -1;
  }

  // Create the thread which runs the parent method given to libinit.
  if (thread_create(func, arg) == -1) {
    return -1;
  }

  // Since thread_create(...) added the thread to the ready queue, we should go ahead and pop it off to run it.
  RUNNING_THREAD = READY_QUEUE.front();
  READY_QUEUE.pop();

  // Call the function manually.
  func(arg);
  interrupt_disable2();

  // Once the function has been called and executed, we swap to switch thread for cleanup and so that we may run our
  // next thread if there are any. We set the first thread status to complete.
  RUNNING_THREAD->status = 3;
  swapToSwitchThread();
}

// "Trampoline method" as described on Piazza and lecture
static void STUB(thread_startfunc_t func, void *arg) {

  // We enable interrupts before since this method begins execution directly after a swapcontext. We always must disable
  // interrupts before swapcontext, and so must enable them to begin.
  interrupt_enable2();
  func(arg);
  interrupt_disable2();

  // The thread is done executing.
  RUNNING_THREAD->status = 3;

  // Switch to the switch thread to clean this finished thread up and get the next one off of the ready queue.
  swapToSwitchThread();
}

// Creates a new thread with the given start function and arguments.
int thread_create(thread_startfunc_t func, void *arg) {
  interrupt_disable2();

  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libinit(...) first.");
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
    * remember to try catch every time we create new ucontext_t
    */

  TCB* newThread;
  try {
    // Initialize struct variables
    newThread = new TCB;
    newThread->status = 0;

    // Setup the ucontext and give it the input function to execute.
    newThread->ucontext = new ucontext_t;
    getcontext(newThread->ucontext);
    newThread->ucontext->uc_stack.ss_sp = new char [STACK_SIZE];
    newThread->ucontext->uc_stack.ss_size = STACK_SIZE;
    newThread->ucontext->uc_stack.ss_flags = 0;
    newThread->ucontext->uc_link = NULL;
    makecontext(newThread->ucontext, (void (*)())STUB, 2, func, arg);

    // Push the thread on to the ready queue, since it is now ready.
    READY_QUEUE.push(newThread);
  }
  catch (bad_alloc b) {
    delete (char*) newThread->ucontext->uc_stack.ss_sp;
    delete newThread->ucontext;
    delete newThread;
    interrupt_enable2();
    return -1;
  }
  interrupt_enable2();
  return 0;
}

// Yields to the next thread on the ready queue, and current thread is placed at the end of ready queue.
int thread_yield(void) {
  interrupt_disable2();
  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libinit(...) first.");
    interrupt_enable2();
    return -1;
  }

  // Push current thread to back of the ready queue.  
  READY_QUEUE.push(RUNNING_THREAD);

  //Switch to the switch thread to get the next one off of the ready queue.
  swapToSwitchThread();
  interrupt_enable2();
  return 0;
}

// Assigns lock to a thread.
int thread_lock(unsigned int lock){
  // Thread lock must not be interrupted because two threads might end up holding lock.
  interrupt_disable2();
  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libinit(...) first.");
    interrupt_enable2();
    return -1;
  }
  // Cannot try to re-lock if lock is already held.
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
    swapToSwitchThread(); // Switch thread to run the head of the ready queue.
  } else {
    LOCK_OWNER_MAP[lock] = RUNNING_THREAD; // Give lock to this thread.
  }

  // We can re-enable interrupts for forced yields.
  interrupt_enable2();
  return 0;
}

// Takes away lock from thread.
int thread_unlock(unsigned int lock){
  // Disable interrupts for successful unlocks.
  interrupt_disable2();

  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libnit(...) first.");
    interrupt_enable2();
    return -1;
  }

  // If no one holds the lock, if the lock owner is null, or if the current thread doesn't hold the lock,
  // attempting to unlock the lock is an error.
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
  
  // We can re-enable interrupts for forced yields.
  interrupt_enable2();
  return 0;
}

// Waits for a lock, condition variable pair to be signaled.
int thread_wait(unsigned int lock, unsigned int cond) {
  interrupt_disable2();
  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libinit(...) first.");
    interrupt_enable2();
    return -1;
  }

  // The code below is copied from unlock, because we have to unlock the held lock.
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
  // If CV waiting queue is not initialized, we initialize it.
  pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
  if (CV_QUEUE_MAP.find(lock_cond_pair) == CV_QUEUE_MAP.end()){
    queue<TCB*> NEW_CV_QUEUE;
    CV_QUEUE_MAP[lock_cond_pair] = NEW_CV_QUEUE;
  }
  // Push thread to tail of CV waiting queue.
  CV_QUEUE_MAP[lock_cond_pair].push(RUNNING_THREAD);
  // Switch thread so that thread from the front of ready queue runs.
  swapToSwitchThread();
  interrupt_enable2();
  // After returning from swapcontext and being awoken, we must first reacquire the lock.
  return thread_lock(lock);
}

// Signals a thread that is waiting for a lock condition variable pair to wake up.
int thread_signal(unsigned int lock, unsigned int cond){
  interrupt_disable2();
  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libinit(...) first.");
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

// Exactly the same signal except it releases the entire queue,
int thread_broadcast(unsigned int lock, unsigned int cond) {
  interrupt_disable2();
  if (!islib) {
    // printf("Thread library must be initialized first. Call thread_libinit(...) first.");
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