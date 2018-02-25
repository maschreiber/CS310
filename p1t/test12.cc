#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;


//broadcast before anything on waitlist



void one(void* arg){
	cout << "enter one.\n";
	if (thread_broadcast(1,1) < 0){
		cout << "1";
		
	} else{
		cout << "2";
	}

	if (thread_signal(1,1) < 0){
		cout << "3";
	} else{
		cout << "4";
	}
	exit(0);

}


void parent(void* arg){


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
    //exit(1);
	}else{
		cout << "thread_libinit susceeded\n";
	}

}