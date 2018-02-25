#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include <assert.h>
using namespace std;
//detects if other functions is called before libinit is called

int lock = 1;
int cond = 1;

void parent(){
  
}


int main() {
	if (thread_signal(lock, cond) < 0){
		cout << "thread signal fail. Exit correctly.\n";
		exit(0);
	} else {
		cout << "thread signal succeeded. Wrong output.\n";
	}

	if (thread_yield() < 0){
		cout << "thread yield fail. Exit correctly.\n";
	} else {
		cout << "thread yield succeeded. Wrong output.\n";
	}

  if (thread_libinit( (thread_startfunc_t) parent, (void *) 100)) {
    cout << "thread_libinit failed\n";
    exit(1);
  }else{
    cout << "thread_libinit susceeded\n";
  }
}
