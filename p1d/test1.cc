#include "thread.h"
#include <fstream>
#include <stdlib.h>
#include <iostream>

using namespace std;

void one(void* arg) {                                                            
    thread_lock(0);                                                                     
    cout <<"1 wait...\n";
    thread_wait(0,0);
    cout <<"1 awake...\n";
    thread_unlock(0);
    cout <<"1 done...\n";
    cout <<"Ended program\n";
}

void two(void* arg) {
    thread_lock(0);
    cout <<"2 yields...\n";
    thread_yield();
    cout <<"2 return\n";
    thread_unlock(0);
    cout <<"2 done\n";
    return;
}

void three(void* arg) {
    thread_signal(0,0);
    cout <<"3 signals...\n";
    thread_lock(0);
    cout <<"3 locks\n";
    thread_unlock(0);
    cout <<"3 done\n";
}

void all_thread_creation (void* arg) {
    thread_create((thread_startfunc_t) one, arg);
    thread_create((thread_startfunc_t) two, arg);
    thread_create((thread_startfunc_t) three, arg);
    cout <<"created all threads\n";
}

int main(int argc, char *argv[]) {
    int valBack =  thread_libinit((thread_startfunc_t) all_thread_creation, (void*) NULL);
    exit(0);
    return 1;
}