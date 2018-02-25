// Deadlock threads. Taken from 310 Spring 16 Midterm 1 P3.
#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>

static int sodacount = 0;

void consume(void* a) {
	thread_lock(1);
	while (empty) {
		thread_yield();
		thread_wait(1, 2);
	}
	sodacount = sodacount - 1;
	thread_signal(1, 2);
}


void parent(void* a) {
  thread_create( (thread_startfunc_t) consume, a);
  thread_create( (thread_startfunc_t) consume, a);
  thread_create( (thread_startfunc_t) produce, a);
  thread_yield();
  produce();
}

// Copied from app.cc
int main() {
  if (thread_libinit( (thread_startfunc_t) parent, (void *) 100)) {
    cout << "thread_libinit failed\n";
    exit(1);
  }
}