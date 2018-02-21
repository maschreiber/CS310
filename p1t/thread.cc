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

TCB* current_thread;

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

int thread_create(thread_startfunc_t func, void *arg){
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
	READY_QUEUE.push(newthread);
	interrupt_enable();
	//remember to deallocate memory after this
	return 0;
}

int thread_yield(void){
	//caller goes to the tail of ready queue another thread from the front of readyq runs
	TCB* next_thread = READY_QUEUE.front();
}

 









