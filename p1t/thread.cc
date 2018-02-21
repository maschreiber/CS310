#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <queue>


using namespace std;

queue<TCB*> READY_QUEUE;
//map of queues for multiple locks/cvs
map<unsigned int, queue<TCB*>> LOCK_QUEUE_MAP;
map<pair<unsigned int, unsigned int>, queue<TCB*>> CV_QUEUE_MAP;
map<unsigned int, TCB*> LOCK_OWNER_MAP; //maps the lock id to its owner

int THREAD_COUNT = 0; //total number of thread

TCB* current_thread;

// Structure which represents thread.
struct TCB {
	int tid; // Thread identifier number.
	int status; //Thread state: 0 is ready, 1 is blocked for lock, 2 is waiting for CV, 3 is finished.
	ucontext_t* ucontext; // Thread context.
}


void STUB(thread_startfunc_t func, void* arg){
	//Call (*func)(args), Call thread_exit()
	interrupt_enable();
	func(arg);
	interrupt_disable();
	thread_exit(); //question? what is thread_exit?
}

boolean t_init = false;

/**
 * Method thread_libinit initializes the thread library. A user program should call thread_libinit exactly
 * once (before calling any other thread functions). Library must not be initialized more than once. 
 * Creates and runs first thread, which calls the starting function it points to and this method will never
 * return to its caller, and control with transfer completely to the starting function of the main application 
 * thread.
 */ 
int thread_libinit(thread_startfunc_t func, void *arg) {
	interrupt_disable();
	
	// If the library has already been initialized, do not reinitialize.
	if (t_init) {
		printf("Thread library already initialized. Do not attempt initialization again.");
		interrupt_enable();
		return -1;
	} else {
		t_init = true;
	}
	
	// Re-enable interrupts before new thread creation.
	interrupt_enable();

	// Create new thread.
	int main_status = thread_create(func, arg);

	// If thread creation failed.
	if (main_status < 0) {

	}
}

int thread_create(thread_startfunc_t func, void *arg) {
	interrupt_disable();

	if (!init) {
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
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
	makecontext(context, (void (*)()) start, 2, func, arg);

	// Set thread context.
	thread->ucontext = context;

	// Set thread status at ready.
	thread->status = 0;

	// Set thread identification number.
	thread->tid = THREAD_COUNT;
	THREAD_COUNT++;

	// Put thread on ready queue.
	READY_QUEUE.push(thread);

	interrupt_enable();
	return 0;
}

int thread_yield(void){
	//caller goes to the tail of ready queue another thread from the front of readyq runs
	interrupt_disable();
	if (t_init == false){
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
		return -1;
	}
	TCB* next_thread = READY_QUEUE.front();
	if (next_thread == NULL){
		interrupt_enable();
		return 0;
	}
	READY_QUEUE.push(current_thread);
	swapcontext(current_thread->ucontext,next_thread->ucontext)
	interrupt_enable();
	return 0
}

int thread_lock(unsigned int lock){
	//NEED MONITORS!
	TCB* owner = LOCK_OWNER_MAP[lock]; //if key lock is not existant in lock_owner_map, NULL is set as its value
	if (t_init == false || owner == current_thread){
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
		return -1;
	}
	//check if there is a queue for the lock in the lock queue map if not add one
	if (LOCK_QUEUE_MAP.find(lock) == LOCK_QUEUE_MAP.end()){
		// key not found in the map, initialize a queue for this lock
		LOCK_OWNER_MAP[lock] = NULL; //owner is NULL, initiated under the while loop
		queue<TCB*> new_LOCK_QUEUE;
		LOCK_QUEUE_MAP.insert({lock, new_LOCK_QUEUE});
	} 
	//while this monitor is not free put my TCB on this monitor lock list and switch
	while (owner != NULL){
		LOCK_QUEUE_MAP[lock].push(current_thread);
		if (READY_QUEUE.empty()){
			printf("The ready queue is empty when trying to push the front of the ready queue to run after a block from lock");
			//all threads are deadlocked need to exit the thread library
			return -1;
		}
		TCB* next_thread = READY_QUEUE.front(); //put it to sleep if T attempts to acquire a lock that is busy
		READY_QUEUE.pop();
		swapcontext(current_thread->ucontext,next_thread->ucontext);
	}
	//if this monitor is free set current thread as owner of monitor
	LOCK_OWNER_MAP[lock] = current_thread;
	return 0;
}



int thread_unlock(unsigned int lock){
	//NEED MONITORS
	//caller releases lock and continues running
	//If the lock queue is not empty, then wake up a thread by moving it from the head of the lock queue to the tail of the ready queue
	interrupt_disable();
	if (t_init == false){
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
		return -1;
	}
	LOCK_OWNER_MAP[lock] = NULL; //releases owner
	if (!LOCK_QUEUE.empty()){ //take the front of the lock queue to the end of the ready queue
		READY_QUEUE.push(LOCK_QUEUE_MAP[lock].front());
		LOCK_QUEUE_MAP[lock].pop();
	}
	interrupt_enable();
	return 0;
}



int thread_wait(unsigned int lock, unsigned int cond){
	//NEED MONITORS
	if (t_init == false){
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
		return -1;
	}
	//caller unlocks the lock
	thread_unlock(lock);
	//if CV queue not intialized, initialize it
	if (CV_QUEUE_MAP.find(make_pair(lock,cond)) == CV_QUEUE_MAP.end()){
		queue<TCB*> NEW_CV_QUEUE;
		CV_QUEUE_MAP[make_pair(lock,cond)] = NEW_CV_QUEUE;
	}
	//move current thread to tail of CV queue of this lock
	CV_QUEUE_MAP[make_pair(lock,cond)].push(current_thread);
	//thread from the front of ready queue runs
	switch_thread();
	thread_lock(lock);
	return 0;
}


int thread_signal(unsigned int lock, unsigned int cond){
	//if CV queue not empty, head of cv queue goes to the tail of ready queue
	if (t_init == false){
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
		return -1;
	}
	//get waiter TCB from CV queue
	if (!CV_QUEUE_MAP[make_pair(lock,cond)].empty()){
		READY_QUEUE.push(CV_QUEUE_MAP[make_pair(lock,cond)].front());
		CV_QUEUE_MAP[make_pair(lock,cond)].pop();
	}
	return 0;
}


int thread_broadcast(unsigned int lock, unsigned int cond){
	if (t_init == false){
		printf("Thread library must be initialized first. Call thread_libinit first.");
		interrupt_enable();
		return -1;
	}
	//get all the waiter in CV queue and put it in ready queue
	while (!CV_QUEUE_MAP[make_pair(lock,cond)].empty()){
		READY_QUEUE.push(CV_QUEUE_MAP[make_pair(lock,cond)].front());
		CV_QUEUE_MAP[make_pair(lock,cond)].pop();
	}
	return 0;
}



