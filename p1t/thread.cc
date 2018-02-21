#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <queue>


using namespace std;

queue<TCB*> READY_QUEUE;
queue<TCB*> LOCK_QUEUE; 
//map of queues for multiple locks/cvs
queue<TCB*> CV_QUEUE;

int THREAD_COUNT = 0; //total number of thread

TCB* RUNNING_THREAD;

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
		printf("Thread library must be initialized first. Call thread_libinit first.")
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
	TCB* next_thread = READY_QUEUE.front();
	if (next_thread == NULL){
		interrupt_enable();
		return 0;
	}
	swapcontext(current_thread->ucontext,next_thread->ucontext)
	interrupt_enable();
	return 0
}

 

 









