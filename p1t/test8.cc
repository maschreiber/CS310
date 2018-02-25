#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;

//call libinit multiple times


void grandchildren(void* arg){
	
}

void child(void* arg){
	if (thread_libinit( (thread_startfunc_t) grandchildren, (void *) 100)) {
		cout << "thread_libinit failed. Correct.\n";
	}else{
		cout << "thread_libinit susceeded. Not correct.\n";
	}
}


void parent(void* arg){

	if (thread_libinit( (thread_startfunc_t) child, (void *) 100)) {
		cout << "thread_libinit failed. Correct.\n";
		exit(0);
	}else{
		cout << "thread_libinit susceeded. Not correct.\n";
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