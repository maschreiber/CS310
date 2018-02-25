#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;
//complicated version of test7.cc dont know if should work or not. tests if broadcast does not sent every thread to ready queue

//put all ready thread in the cv queue
//broadcast: put everything on the cv wait queue to the tail of the ready queue
//make each thread run after broadcast and couts when thread starts from broadcast


int lock1 = 1;
int cond1 = 1;

//correct:
//thread1, thread2, thread3, thread4 in ready queue
//thread1 runs on block, grabs lock1, calls wait, relinguish lock and gets put in cv wait queue
//thread2 runs in block, grabs lock1, calls wait, relinguish lock and gets put in cv wait queue
//thread3 runs in child, grabs lock1, broadcasts, thread1 and 2 leaves cv wait queue to go inside ready queue
//thread3 waits in cv wait, no lock, and thread 1 grabs lock and runs and unlock and exit
//thread2 runs and grabs lock, unlock and exit
//thread3 runs in child, grabs lock and unlock and exit



void broadcast(void* arg){
	cout << "thread in broadcast\n";
	if (thread_lock(lock1) < 0) { 
    	cout << "thread lock1 failed.\n";
  	}else{
  		cout << "thread lock1 successful.\n";
  	}
  	cout << "thread tries to broadcast.\n";
	if (thread_broadcast(lock1, cond1) < 0){
		cout << "thread broadcast failed.\n";
	} else{
		cout << "thread broadcast suceeded.\n";
	}
	cout << "thread tries to wait.\n";

  	if (thread_wait(lock1, cond1) < 0){ 
  		cout << "thread wait failed.\n";
  	}else{
  		cout << "thread wait suceeded.\n";
  	}
  	cout << "thread comes back from wait in broadcast.\n";

	if (thread_unlock(lock1) < 0) { 
    	cout << "thread unlock1 failed.\n";
  	}else{
  		cout << "thread unlock1 successful.\n";
  	}
  	cout << "broadcast ends.\n";
  	exit(0);
}

void block(void* arg){
	cout << "thread enters block.\n";
	if (thread_lock(lock1) < 0) { 
    	cout << "block lock1 failed.\n";
  	}else{
  		cout << "block lock1 successful.\n";
  	}
  	cout << "block tries to wait.\n";

  	if (thread_wait(lock1, cond1) < 0){ 
  		cout << "block wait failed.\n";
  	}else{
  		cout << "block wait suceeded.\n";
  	}
  	cout << "block comes back from wait in block.\n";

  	if (thread_unlock(lock1) < 0) { 
    	cout << "block unlock1 failed.\n";
  	}else{
  		cout << "block unlock1 successful.\n";
  	}
  	cout << "block tries to broadcast.\n";
  	if (thread_broadcast(lock1, cond1) < 0){
		cout << "block broadcast failed.\n";
	} else{
		cout << "block broadcast suceeded.\n";
	}
	cout << "block tries to wait.\n";

  	if (thread_wait(lock1, cond1) < 0){ 
  		cout << "block wait failed.\n";
  	}else{
  		cout << "block wait suceeded.\n";
  	}
  	cout << "func block ends.\n";

}






void parent(void* arg){
	cout << "thread enters parent\n";
  if (thread_create((thread_startfunc_t) block, (void*) 100) < 0){
    cout << "thread 1 failed\n";
    exit(1);
  }else{
    cout << "thread 1 created\n";
  }

  if (thread_create((thread_startfunc_t) block, (void*) 100) < 0){
    cout << "thread 2 failed\n";
    exit(1);
  }else{
    cout << "thread 2 created\n";
  }

  if (thread_create((thread_startfunc_t) broadcast, (void*) 100) < 0){
    cout << "thread 3 failed\n";
    exit(1);
  }else{
    cout << "thread 3 created\n";
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