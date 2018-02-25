#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
//test ping pong

using namespace std;

int lock1 = 1;
int cond1 = 1;
int lock2 = 2;

void pingpong1(void* arg){
  //ping pong  while something is not done, mx acquire, print, mx release
  if (thread_lock(lock1) < 0) { //thread 1 should get this lock first
    cout << "thread lock1 failed\n";
    exit(0);
  }else{
    cout << "thread lock1 successful\n";
  }

  //thread 1 get this lock, ping and relinguish the lock and waits, in CV queue now
  while(true){
    cout << "ping\n";

    if (thread_signal(lock1, cond1) < 0){ //signal and thread2 is on ready
      cout << "thread1 signal failed\n";
      exit(0);
    } else{
      cout << "thread1 signal succeeded\n";
    }

    
    if (thread_wait(lock1, cond1) < 0){
      cout << "thread1 wait failed\n";
      exit(0);
    }else{
      cout << "thread1 waits\n";
    }
    //returns after thread 2 waits
  }

  if (thread_unlock(lock1) < 0) {
    cout << "thread unlock1 failed\n";
    exit(0);
  }else{
    cout << "thread unlock1 successful\n";
  }
  
}

void pingpong2(void* arg){
  //ping pong  while something is not done, mx acquire, print, mx release

  if (thread_lock(lock1) < 0) { //thread 1 waits, thread2 gets this lock
    cout << "thread lock1 failed\n";
    exit(0);
  }else{
    cout << "thread lock1 successful\n";
  }

  //thread pongs and put herself on wait(), thread 2 in CV queue now
  while (true){
    cout << "pong\n";

    if (thread_signal(lock1, cond1) < 0){ //signal and thread1 is on ready
      cout << "thread2 signal failed\n";
      exit(0);
    } else{
      cout << "thread2 signal succeeded\n";
    }

    if (thread_wait(lock1, cond1) < 0){
      cout << "thread2 wait failed\n";
      exit(0);
    }else{
      cout << "thread2 waits\n";
    }
    
    //returns after thread 1 waits
  }

  if (thread_unlock(lock1) < 0) {
    cout << "thread unlock1 failed\n";
    exit(0);
  }else{
    cout << "thread unlock1 successful\n";
  }
  
}

void parent(){
  cout << "thread enters parent\n";
  if (thread_create((thread_startfunc_t) pingpong1, (void*) 100) < 0){
    cout << "\nthread 1 failed\n";
    exit(0);
  }else{
    cout << "\nthread 1 created\n";
  }
  if (thread_create((thread_startfunc_t) pingpong2, (void*) 100) < 0){
    cout << "\nthread 2 failed\n";
    exit(0);
  }else{
    cout << "\nthread 2 created\n";
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