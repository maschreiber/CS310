#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "thread.h"
#include "interrupt.h"


using namespace std;

int THREAD_COUNT = 0; //total number of thread

struct TCB{
	// name/status (TCB) -> stack , ucontext_t
	int tid; // thread identifier
	char* stack; //stack pointer 
	int status; //state of the thread: running ready waiting start done
	ucontext_t* ucontext; // ucontext
	
}

int thread_create(thread_startfunc_t func, void *arg){
	//allocate thread control block
	//allocate stack
	//build stack frame for base of stack (stub)
	//put func, args on stack
	//goes in ready queue
	interrupt_disable();








	interrupt_enable();
}
