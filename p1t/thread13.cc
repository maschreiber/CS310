#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;




void one(void* arg){
  if (thread_lock(1) < 0) { 
    cout << "thread1 loc12 failed. Incorrect.\n";
  }else{
    cout << "thread1 lock2d successful. Correct.\n";
  }

  if (thread_unlock(3) < 0) { 
    cout << "thread1 unlock lock3 failed. InCorrect.\n";
  }else{
    cout << "thread1 unlock2 succedssful. Correct.\n";
  }

  if (thread_unlock(0) < 0) { 
    cout << "thread1 unlock locsk2 failed. InCorrect.\n";
  }else{
    cout << "thread1 unlock2 succdsessful. Correct.\n";
  }

}

void parent(void* arg){
	cout << "thread enters parent\n";
  if (thread_create((thread_startfunc_t) one, (void*) 100) < 0){
    cout << "thread 1 failed\n";
    exit(1);
  }else{
    cout << "thread 1 created\n";
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