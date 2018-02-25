// Literal random nonsense.
#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>

using namespace std;

void methodA(void *a) {
	cout << "A";
	thread_wait(1,2);
	cout << "B";
	thread_lock(2);
	cout << "C";
	thread_signal(1, 2);
	cout << "D";
	thread_yield();
	cout << "E";
	thread_unlock(1);
	cout << "F";
	thread_wait(1, 3);
	cout << "G";
	thread_lock(2);
	cout << "H";
	thread_yield();
	cout << "I";
	thread_lock(2);
	cout << "J";
	thread_unlock(2);
	cout << "K";
	thread_unlock(3);
	cout << "L";
	thread_wait(3, 1);
	cout << "M";
	thread_yield();
	cout << "Y";
	thread_lock(1);
	cout << "Z";
	thread_unlock(1);
}

void methodB(void *a) {
	cout << "N";
	thread_lock(2);
	cout << "O";
	thread_wait(1, 2);
	cout << "P";
	thread_broadcast(1, 2);
	cout << "Q";
	thread_unlock(2);
	cout << "R";
	thread_lock(1);
	cout << "S";
	thread_lock(2);
	cout << "T";
	thread_wait(1, 2);
	cout << "U";
	thread_yield();
	cout << "V";
	thread_yield();
	cout << "X";
} 

void methodC(void *a) {
	cout << "R";
	thread_yield();
	cout << "ooo";
}

void parent(void* a) {
  thread_create( (thread_startfunc_t) methodA, a);
  thread_create( (thread_startfunc_t) methodB, a);
  thread_create( (thread_startfunc_t) methodC, a);
}

// Copied from app.cc
int main() {
  if (thread_libinit( (thread_startfunc_t) parent, (void *) 100)) {
    cout << "thread_libinit failed\n";
    exit(1);
  }
}