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


struct TCB {
  ucontext_t* ucontext;
  int status; //3 for ready to clean up
  char* stack;
};

static TCB* RUNNING_THREAD;
static ucontext_t* DELETE_THREAD;

//map of queues for multiple locks/cvs
static queue<TCB*> READY_QUEUE;
static map<unsigned int, queue<TCB*> > LOCK_QUEUE_MAP;
static map<pair<unsigned int, unsigned int>, queue<TCB*> > CV_QUEUE_MAP;
static map<unsigned int, TCB*> LOCK_OWNER_MAP; //maps the lock id to its owner
static bool islib = false;

void interrupt_enable2() {
  interrupt_enable();
}

void interrupt_disable2() {
  interrupt_disable();
}

static void cleanup(){
  if (RUNNING_THREAD->status == 3 || RUNNING_THREAD == NULL){
    RUNNING_THREAD->ucontext->uc_stack.ss_sp = NULL;
    RUNNING_THREAD->ucontext->uc_stack.ss_size = 0;
    RUNNING_THREAD->ucontext->uc_stack.ss_flags = 0;
    RUNNING_THREAD->ucontext->uc_link = NULL;
    delete RUNNING_THREAD->ucontext;
    delete RUNNING_THREAD->stack;
    delete RUNNING_THREAD;
    RUNNING_THREAD = NULL;
  }
}

static void check_ready_queue(){
  while (!READY_QUEUE.empty()) {
    cleanup();
    RUNNING_THREAD = READY_QUEUE.front();
    READY_QUEUE.pop();
    swapcontext(DELETE_THREAD, RUNNING_THREAD->ucontext);
  }
}

int thread_libinit(thread_startfunc_t func, void *arg) {
  if (islib) return -1;
  islib = true;

  try {
    DELETE_THREAD = new ucontext_t;
    getcontext(DELETE_THREAD); //order changed by J
  } catch (bad_alloc b) {
    delete DELETE_THREAD;
    return -1;
  }

  if (thread_create(func, arg) < 0){
    return -1;
  }

  RUNNING_THREAD = READY_QUEUE.front();
  READY_QUEUE.pop();

  //switch to RUNNING_THREAD thread
  interrupt_disable2();
  swapcontext(DELETE_THREAD, RUNNING_THREAD->ucontext);

  check_ready_queue();

  cleanup();

  //When there are no runnable threads in the system (e.g. all threads have
  //status, or all threads are deadlocked)
  cout << "Thread library exiting.\n";
  exit(0);
}

static void STUB(thread_startfunc_t func, void *arg) {

  interrupt_enable2();
  func(arg);
  interrupt_disable2();

  RUNNING_THREAD->status = 3;
  swapcontext(RUNNING_THREAD->ucontext, DELETE_THREAD);
}

int thread_create(thread_startfunc_t func, void *arg) {
  if (!islib) return -1;

  interrupt_disable2();
  TCB* newThread;
  try {
    newThread = new TCB;
    newThread->stack = new char [STACK_SIZE];
    newThread->status = 0;

    newThread->ucontext = new ucontext_t;
    getcontext(newThread->ucontext);
    newThread->ucontext->uc_stack.ss_sp = newThread->stack;
    newThread->ucontext->uc_stack.ss_size = STACK_SIZE;
    newThread->ucontext->uc_stack.ss_flags = 0;
    newThread->ucontext->uc_link = NULL;
    makecontext(newThread->ucontext, (void (*)())STUB, 2, func, arg);

    READY_QUEUE.push(newThread);
  }
  catch (bad_alloc b) {
    delete newThread->stack;
    delete newThread->ucontext;
    delete newThread;
    interrupt_enable2();
    return -1;
  }
  interrupt_enable2();
  return 0;
}

int thread_yield(void) {
  if (!islib) return -1;

  interrupt_disable2();
  //cout << "Adding a thread back to READY_QUEUE queue " << RUNNING_THREAD << "\n";
  READY_QUEUE.push(RUNNING_THREAD);
  swapcontext(RUNNING_THREAD->ucontext, DELETE_THREAD);
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
    swapcontext(RUNNING_THREAD->ucontext, DELETE_THREAD); // Switch thread to run the head of the ready queue.
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
  swapcontext(RUNNING_THREAD->ucontext, DELETE_THREAD);
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
