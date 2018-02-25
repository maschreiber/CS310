#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;
//broadcast does not sent every thread to ready queue
//put all ready thread in the 
//broadcast: put everything on the cv wait queue to the tail of the ready queue
//make each thread run after broadcast and couts when thread starts from broadcast


int lock = 1;
int cond = 1;



void parent(void* arg){
	cout << "thread enters parent\n";
  if (thread_create((thread_startfunc_t) one, (void*) 100) < 0){
    cout << "thread 1 failed\n";
    exit(0);
  }else{
    cout << "thread 1 created\n";
  }

  if (thread_create((thread_startfunc_t) two, (void*) 100) < 0){
    cout << "thread 2 failed\n";
    exit(0);
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