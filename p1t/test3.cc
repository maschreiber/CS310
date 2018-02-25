// Deadlock threads. Taken from 310 Spring 16 Midterm 1 P3.
#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>

using namespace std;

static int sodacount = 0;

void produceA(void *a) {
  thread_lock(1);
  cout << "Producer A acquires lock.\n";
  while (sodacount == 1) {
    thread_yield();
    cout << "Producer A returns from first yield.\n";
    thread_wait(1, 2);
    cout << "Producer A returns from wait.\n";
  }
  sodacount = sodacount + 1;
  cout << "Producer A increments soda count.\n";
  thread_signal(1, 2);
  cout << "Producer A signals.\n";
  thread_yield();
  cout << "Producer A returns from second yield.\n";
  thread_unlock(1);
  cout << "Producer A unlocks.\n";
}

void produceB(void *a) {
  thread_lock(1);
  cout << "Producer B acquires lock.\n";
  while (sodacount == 1) {
    thread_yield();
    cout << "Producer B returns from first yield.\n";
    thread_wait(1, 2);
    cout << "Producer B returns from wait.\n";
  }
  sodacount = sodacount + 1;
  cout << "Producer B increments soda count.\n";
  thread_signal(1, 2);
  cout << "Producer B signals.\n";
  thread_yield();
  cout << "Producer B returns from second yield.\n";
  thread_unlock(1);
  cout << "Producer B unlocks.\n";
}

void consumeA(void *a) {
  thread_lock(1);
  cout << "Consumer A acquires lock.\n";
  while (sodacount == 0) {
    thread_yield();
    cout << "Consumer A returns from first yield.\n";
    thread_wait(1, 2);
    cout << "Consumer A returns from wait.\n";
  }
  sodacount = sodacount - 1;
  cout << "Consumer A decrements soda count.\n";
  thread_signal(1, 2);
  cout << "Consumer A signals.\n";
  thread_yield();
  cout << "Consumer A returns from second yield.\n";
  thread_unlock(1);
  cout << "Consumer A unlocks.\n";
}

void consumeB(void *a) {
  thread_lock(1);
  cout << "Consumer B acquires lock.\n";
  while (sodacount == 0) {
    thread_yield();
    cout << "Consumer B returns from first yield.\n";
    thread_wait(1, 2);
    cout << "Consumer B returns from wait.\n";
  }
  sodacount = sodacount - 1;
  cout << "Consumer B decrements soda count.\n";
  thread_signal(1, 2);
  cout << "Consumer B signals.\n";
  thread_yield();
  cout << "Consumer B returns from second yield.\n";
  thread_unlock(1);
  cout << "Consumer B unlocks.\n";
}


void parent(void* a) {
  thread_create( (thread_startfunc_t) consumeA, a);
  thread_create( (thread_startfunc_t) consumeB, a);
  thread_create( (thread_startfunc_t) produceB, a);
  cout << "Producer A yields.\n";
  thread_yield();
  produceA(a);
}

// Copied from app.cc
int main() {
  if (thread_libinit( (thread_startfunc_t) parent, (void *) 100)) {
    cout << "thread_libinit failed\n";
    exit(1);
  }
}