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
queue<TCB*> CV_QUEUE;

int THREAD_COUNT = 0; //total number of thread

struct TCB{
	// name/status (TCB) -> stack , ucontext_t
	int tid; // thread identifier
	char* stack; //stack pointer 
	int status; //state of the thread: 0ready 1running 2waiting 3done? or just done or not
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
	THREAD_COUNT ++;
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
	newthread->tid = THREAD_COUNT;
	newthread->stack = stack;
	newthread->status = 0; // 0 for ready
	newthread->ucontext = ucontext_ptr;
	//push to ready queue
	READY_QUEUE.push_back(newthread);
	interrupt_enable();
	//remember to deallocate memory after this
	return 0;
}

int thread_yield(void){
	
}











