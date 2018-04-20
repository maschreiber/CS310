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
static void check_ready_queue();
static void switchtodeletethread();
static void switchtorunningthread();


struct TCB {
  ucontext_t* ucontext;
  int status; //3 for ready to clean up
};

static TCB* RUNNING_THREAD;
static TCB* DELETE_THREAD;

//map of queues for multiple locks/cvs
static queue<TCB*> READY_QUEUE;
static map<unsigned int, queue<TCB*> > LOCK_QUEUE_MAP;
static map<pair<unsigned int, unsigned int>, queue<TCB*> > CV_QUEUE_MAP;
static map<unsigned int, TCB*> LOCK_OWNER_MAP; //maps the lock id to its owner
static bool islib = false;

//interrupt
void interrupt_enable2() {
  interrupt_enable();
}

void interrupt_disable2() {
  interrupt_disable();
}

static void switchtodeletethread(){
  swapcontext(RUNNING_THREAD->ucontext, DELETE_THREAD->ucontext);
}

static void switchtorunningthread(){
  swapcontext(DELETE_THREAD->ucontext, RUNNING_THREAD->ucontext);
}

static void cleanup(){
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

static void check_ready_queue(){
  while (!READY_QUEUE.empty()) {
    cleanup();
    RUNNING_THREAD = READY_QUEUE.front();
    READY_QUEUE.pop();
    switchtorunningthread();
  }
}

int thread_libinit(thread_startfunc_t func, void *arg) {
  if (islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
    return -1;
  }

  islib = true;

  try {
    //initiazte delete thread struct variables
    DELETE_THREAD = new TCB;
    DELETE_THREAD->status = 0;

    //set ucontext
    DELETE_THREAD->ucontext = new ucontext_t;
    getcontext(DELETE_THREAD->ucontext);
    DELETE_THREAD->ucontext->uc_stack.ss_sp = new char [STACK_SIZE];
    DELETE_THREAD->ucontext->uc_stack.ss_size = STACK_SIZE;
    DELETE_THREAD->ucontext->uc_stack.ss_flags = 0;
    DELETE_THREAD->ucontext->uc_link = NULL;
  }
  catch (bad_alloc b) {
    delete (char*) DELETE_THREAD->ucontext->uc_stack.ss_sp;
    delete DELETE_THREAD->ucontext;
    delete DELETE_THREAD;
    return -1;
  }

  if (thread_create(func, arg) == -1){
    return -1;
  }

  RUNNING_THREAD = READY_QUEUE.front();
  READY_QUEUE.pop();

  //switch to RUNNING_THREAD thread to start func
  interrupt_disable2();
  switchtorunningthread();

  //return to clean up
  check_ready_queue();

  // no runnable threads in the system, or all threads are deadlocked
  cout << "Thread library exiting.\n";
  exit(0);
}

static void STUB(thread_startfunc_t func, void *arg) {

  interrupt_enable2();
  func(arg);
  interrupt_disable2();

  RUNNING_THREAD->status = 3;
  switchtodeletethread();
}

int thread_create(thread_startfunc_t func, void *arg) {
  interrupt_disable2();

  if (!islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
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
    //intitate struct variables
    newThread = new TCB;
    newThread->status = 0;

    //set context for newthread's ucontext
    newThread->ucontext = new ucontext_t;
    getcontext(newThread->ucontext);
    newThread->ucontext->uc_stack.ss_sp = new char [STACK_SIZE];
    newThread->ucontext->uc_stack.ss_size = STACK_SIZE;
    newThread->ucontext->uc_stack.ss_flags = 0;
    newThread->ucontext->uc_link = NULL;
    makecontext(newThread->ucontext, (void (*)())STUB, 2, func, arg);

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

int thread_yield(void) {
  interrupt_disable2();
  if (!islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
    interrupt_enable2();
    return -1;
  }
  //cout << "Adding a thread back to READY_QUEUE queue " << RUNNING_THREAD << "\n";
  READY_QUEUE.push(RUNNING_THREAD);
  switchtodeletethread();
  interrupt_enable2();
  return 0;
}

int thread_lock(unsigned int lock){
  // Thread lock must not be interrupted because two threads might end up holding lock.
  interrupt_disable2();
  if (!islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
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
    switchtodeletethread(); // Switch thread to run the head of the ready queue.
  } else {
    LOCK_OWNER_MAP[lock] = RUNNING_THREAD; // Give lock to this thread.
  }

  // We can re-enable interrupts for forced yields.
  interrupt_enable2();
  return 0;
}

int thread_unlock(unsigned int lock){
  // Disable interrupts for successful unlocks.
  interrupt_disable2();
  if (!islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
    interrupt_enable2();
    return -1;
  }
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

int thread_wait(unsigned int lock, unsigned int cond) {
  interrupt_disable2();
  if (!islib) {
    ////printf("Thread library must be islibialized first. Call thread_libislib first.");
    interrupt_enable2();
    return -1;
  }

  // We must have a successful unlock
  interrupt_enable2();
  int unlockresult = thread_unlock(lock); //CHANGED HERE BY J
  interrupt_disable2();
  if (unlockresult == -1) {
    interrupt_enable2();
    return -1;
  }
  // If CV waiting queue is not islibialized, we islibialize it.
  pair<unsigned int, unsigned int> lock_cond_pair = make_pair(lock,cond);
  if (CV_QUEUE_MAP.find(lock_cond_pair) == CV_QUEUE_MAP.end()){
    queue<TCB*> NEW_CV_QUEUE;
    CV_QUEUE_MAP[lock_cond_pair] = NEW_CV_QUEUE;
  }
  // Push thread to tail of CV waiting queue.
  CV_QUEUE_MAP[lock_cond_pair].push(RUNNING_THREAD);
  // Switch thread so that thread from the front of ready queue runs.
  switchtodeletethread();
  interrupt_enable2();
  // After returning from swapcontext and being awoken, we must first reacquire the lock.
  return thread_lock(lock);
}

int thread_signal(unsigned int lock, unsigned int cond){
  interrupt_disable2();
  if (!islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
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
  if (!islib) {
    //printf("Thread library must be islibialized first. Call thread_libislib first.");
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
