#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;
//check if thread fails on locking on a lock it already acquires or is already acquired by someone else
//and if it fails on unlocking a lock it doesnt own or is null


int lock1 = 1;
int lock2 = 2;

void one(void* arg){
	cout << "thread1 running.\n";
	if (thread_lock(lock2) < 0) { 
    	cout << "thread1 lock2 failed. Incorrect.\n";
  	}else{
  		cout << "thread1 lock2 successful. Correct.\n";
  	}

  	cout << "thread1 tries to yield.\n";

  	if (thread_yield() < 0){
  		cout << "thread1 fails yield. Incorrect.\n";
  	} else{
  		cout << "thread1 yields. Correct.\n";
  	}
  	cout << "thread1 running.\n";

  	if (thread_unlock(lock2) < 0) { 
    	cout << "thread1 unlock lock2 failed. InCorrect.\n";
  	}else{
  		cout << "thread1 unlock2 successful. Correct.\n";
  	}


}


void two(void* arg){
	cout << "thread2 running.\n";


	if (thread_lock(lock1) < 0) { 
    	cout << "thread2 lock1 failed. Not correct.\n";
  	}else{
  		cout << "thread2 lock1 successful. Correct.\n";
  	}



  	cout << "thread1 tries to acquire lock1 again.\n";

  	//acquire a lock it already acquire
  	if (thread_lock(lock1) < 0) { 
    	cout << "thread2 lock1 failed. Correct.\n";
  	}else{
  		cout << "thread2 lock1 successful. Not correct.\n";
  	} //should be blocked and thread1 runs


  	if (thread_unlock(lock2) < 0) { //unlock a lock it doesnt own
    	cout << "thread2 unlock lock2 failed. Correct.\n";
  	}else{
  		cout << "thread2 lock2 successful. Not correct.\n";
  	}



  	cout << "thread2 tries to acquire lock2 owned by thread2.\n"; //acquire a lock owned by someone else

  	if (thread_lock(lock2) < 0) { //thread2 blocked here and comes back here and gets the lock
    	cout << "thread2 lock2 failed. Not Correct.\n";
  	}else{
  		cout << "thread2 lock2 successful. Correct.\n";
  	}



  	cout << "thread2 tries to unlock lock2.\n";



  	if (thread_unlock(lock2) < 0) { //unlock a lock it doesnt own
    	cout << "thread2 unlock lock2 failed. Not Correct.\n";
  	}else{
  		cout << "thread2 lock2 successful. Correct.\n";
  	}



  	if (thread_unlock(lock1) < 0) { 
    	cout << "thread2 unlock1 failed. Not correct.\n";
  	}else{
  		cout << "thread2 unlock1 successful. Correct\n";
  	}
  	exit(0);


}


void parent(void* arg){
	cout << "thread enters parent\n";
  if (thread_create((thread_startfunc_t) one, (void*) 100) < 0){
    cout << "thread 1 failed\n";
    exit(1);
  }else{
    cout << "thread 1 created\n";
  }

  if (thread_create((thread_startfunc_t) two, (void*) 100) < 0){
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
    cout << "thread_libinit susceeded\n";
  }
}