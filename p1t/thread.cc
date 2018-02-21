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
map<unsigned int, queue<TCB*>> CV_QUEUE_MAP;
map<unsigned int, TCB*> LOCK_MAP; //maps the lock id to its owner

int THREAD_COUNT = 0; //total number of thread

TCB* current_thread;

struct TCB{
	// name/status (TCB) -> stack , ucontext_t
	int tid; // thread identifier
	char* stack; //stack pointer 
	int status; //state of the thread: 0 not finished 1 finished
	ucontext_t* ucontext; // ucontext	
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
	//allocate thread control block
	//allocate stack
	//build stack frame for base of stack (stub)
	//put func, args on stack
	//goes in ready queue

	interrupt_disable();
	
	//initialize a context structure
	ucontext_t* ucontext_ptr = new ucontext_t; //do trycatch here?
	getcontext(ucontext_ptr); 
	//direct new thread to use a different stack
	char *stack = new char[STACK_SIZE];
	char *stack = new char [STACK_SIZE];
	ucontext_ptr->uc_stack.ss_sp = stack;
	ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
	ucontext_ptr->uc_stack.ss_flags = 0;
	ucontext_ptr->uc_link = NULL;
	makecontext(ucontext_ptr, (void (*)()) STUB, 2, func, arg);
	//allocate update thread control block
	TCB* newthread = new TCB;
	newthread->tid = THREAD_COUNT+1;
	newthread->stack = stack;
	newthread->status = 0; // 0 for ready
	newthread->ucontext = ucontext_ptr;
	//push to ready queue
	READY_QUEUE.push(newthread);
	THREAD_COUNT ++;
	interrupt_enable();
	//remember to deallocate memory after this
	return 0;
}

int thread_yield(void){
	//caller goes to the tail of ready queue another thread from the front of readyq runs
	interrupt_disable();
	if (t_init == false){
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
	TCB* owner = LOCK_MAP[lock]; //if key lock is not existant in lock_map, NULL is set as its value
	if (t_init == false || owner == current_thread){
		interrupt_enable();
		return -1;
	}
	//check if there is a queue for the lock in the lock queue map if not add one
	if (LOCK_QUEUE_MAP.find(lock) == LOCK_QUEUE_MAP.end()){
		// key not found in the map, initialize a queue for this lock
		LOCK_MAP[lock] = NULL;
		queue<TCB*> new_LOCK_QUEUE;
		LOCK_QUEUE_MAP.insert({lock, new_LOCK_QUEUE});
	} 
	//while this monitor is not free put my TCB on this monitor lock list and switch
	while (owner != NULL){
		queue<TCB*> LOCK_QUEUE = LOCK_QUEUE_MAP[lock];
		LOCK_QUEUE.push(current_thread);
		TCB* next_thread = READY_QUEUE.front();
		READY_QUEUE.pop();
		swapcontext(current_thread->ucontext,next_thread->ucontext);
	}
	//if this monitor is free set current thread as owner of monitor
	LOCK_MAP[lock] = current_thread;
	return 0;
}
 



 









