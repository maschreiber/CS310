// If thread doesn't hold lock, thread_wait doesn't do anything.
#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>

using namespace std;

void methodA(void *a) {
	cout << "Step A\n";
	thread_wait(1,1);
	cout << "Step B\n";
}

void methodB(void *b) {
	thread_lock(1);
	cout << "Step C\n";
	thread_unlock(1);
} 

void parent(void* a) {
  thread_create( (thread_startfunc_t) methodA, a);
  thread_create( (thread_startfunc_t) methodB, a);
}

// Copied from app.cc
int main() {
  if (thread_libinit( (thread_startfunc_t) parent, (void *) 100)) {
    cout << "thread_libinit failed\n";
    exit(1);
  }
}