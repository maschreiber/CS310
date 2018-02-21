#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "thread.h"
#include "interrupt.h"


using namespace std;


struct Thread{
	// name/status (TCB) -> stack , ucontext_t
	char* stack;
	int id;
	int status;
	ucontext_t* ucontext;
}

int thread_create(thread_startfunc_t func, void *arg){
}
