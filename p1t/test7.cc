#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;
//tests if broadcast does not sent every thread to ready queue

//put all ready thread in the cv queue
//broadcast: put everything on the cv wait queue to the tail of the ready queue
//make each thread run after broadcast and couts when thread starts from broadcast


int lock1 = 1;
int cond1 = 1;





void block(void* arg){
	if (thread_lock(lock1) < 0) { 
    	cout << "thread lock1 failed.\n";
  	}else{
  		cout << "thread lock1 successful.\n";
  	}

  	thread_yield();

  	if (thread_unlock(lock1) < 0) { 
    	cout << "thread unlock1 failed.\n";
  	}else{
  		cout << "thread unlock1 successful.\n";
  	}


}

void broadcast(void* arg){
	
}




void parent(void* arg){
	cout << "thread enters parent\n";
  if (thread_create((thread_startfunc_t) broadcast, (void*) 100) < 0){
    cout << "thread 1 failed\n";
    exit(1);
  }else{
    cout << "thread 1 created\n";
  }

  if (thread_create((thread_startfunc_t) broadcast, (void*) 100) < 0){
    cout << "thread 2 failed\n";
    exit(1);
  }else{
    cout << "thread 2 created\n";
  }
}


int main() {
  if (thread_libinit( (thread_startfunc_t) parent, (void *) 100)) {
    cout << "thread_libinit failed\n";
    exit(1);
  }else{
    cout << "thread_libinit suceeded\n";
  }
}