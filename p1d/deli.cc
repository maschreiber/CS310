#include <cstdlib>
#include <ucontext.h>
#include <deque>
#include <queue>
#include <iterator>
#include <map>
#include <iostream>
#include <fstream>
#include "interrupt.h"
#include "thread.h"
#include <string>
#include <vector>
#include <sstream>
using namespace std;
//using deque instead of queue so i can initiate it
// using vector instead of map so can ensure insertion order of sandwiches on the corkboard when 2 sandwiches have the same id

/* 
Sandwich: id 0~999
one sandwich maker
cashiers (variable numer): post orders on the board
cork board: max_order, order 
orders: 1) sandwich id 2) cashier id
wait_queue: cashier waiting if board is full
GLOBAL int CASHIER_COUNT;


1) create a specified number of cashier threads to read in sandwich order from a file
FIFO: cashier thread receive order in input file, wait until sandwich maker finished the last request
cashier thread finishs after all sandwiches in input file has been prepared

2) create one thead for the sandwich maker
NOT FIFO: chooses the next sandwich that has closer id
The maker is initialized with her last sandwich as -1. 
maker thread should only handle a request when the cork board has the largest possible number of orders


*/

//a struct for a sandwich order which contains the cashier id and the sanwich id
struct SANDWICH_ORDER{
  int sid; //sandwich id
  int cid; //cashier id
};

//An arraylist of index for keeping insersion order of sandwich orders
vector<SANDWICH_ORDER*> CORK_BOARD; //int = order_number, 

//A Map of cashier id to the status of this cashier if its previous order has been done yet
map<int, int> CASHIER_MAP; //key = cashier_id, value = 0 if can push order, 1 if cannot, 2 if done

//the max_order the cork board can hold, min(max_order, cashier_count) is used when numbers start changing
int max_order;

//the total number of alive cashiers
int cashier_count;

//the start int for calculating the closest id of the next sandwich the maker should grab from cork board
int previousSandwich = -1;

//a map of all orders that need to be done by master based on each cashier
map<int, queue<SANDWICH_ORDER*> > ORDER_RECEIVED; //int = cashier id, queue is a queue of sandwich orders: order_q


int main(int argc, char *argv[]);
void* startf(void* arg);
void* maker();
void* cashier(void* a);





int main(int argc, char *argv[]){
  //Grab input from terminal and store them into global variables, call libinit
  max_order = atoi(argv[1]);
  cashier_count = argc - 2; // - deli - board_max_order
  if (cashier_count < max_order){
    max_order = cashier_count;
  }
  //cout << "\n max order is: " << max_order;

  //read in the cashier id and sandwhich orders in order received
  for (int i = 2; i < argc; i++){ // check use of ifstream  http://www.cplusplus.com/doc/tutorial/files/
    //for each input file = for each cashier
    ifstream sw_input;
    int new_cid = i - 2; // cashier id
    string new_sid_s; // sandwich id in string
    queue<SANDWICH_ORDER*> new_order_q; // queue of order for this cashier

    sw_input.open(argv[i]);

    //for each sandwich order in cashier #
    if (sw_input.is_open()) 
    {
      while (getline(sw_input, new_sid_s)){
        int new_sid = atoi(new_sid_s.c_str()); //sandwich id in string
        SANDWICH_ORDER* neworder = new SANDWICH_ORDER;
        neworder->sid = new_sid;
        neworder->cid = new_cid;
        new_order_q.push(neworder); // push new order onto order queue for this cashier
        //cout << "\ngot input" << neworder->sid << " " << neworder->cid;
      }
      sw_input.close();
      //where all inputs of cashier and their orders are stored
      ORDER_RECEIVED.insert(pair<int, queue<SANDWICH_ORDER*> >(new_cid, new_order_q));
    }else{
      //cout << "\nsw input not open";
    }
  }

  //Let's start!

  thread_libinit( (thread_startfunc_t) startf, (void*) 100);
  return 0;

}


void* startf(void* arg){
  //create one thread for maker
  thread_create((thread_startfunc_t) maker, (void*) 100);

  //create a variable number of cashiers based on terminal input
  for (int cashier_id = 0; cashier_id < cashier_count; cashier_id++) {
    CASHIER_MAP.insert(pair<int, int>(cashier_id, 0));

    thread_create((thread_startfunc_t) cashier, (void*) cashier_id);
    //cout << "\n thread created";
  }
  
}




void* cashier(void* a) {
  //the cashier thread which handles whether the cashier can push its order to master or should wait
  
  int cashier_id = (long int) a;

  //while the cashier still have orders to complete
  while (!ORDER_RECEIVED[cashier_id].empty()) {
    //cout << "\ncork board size = " << CORK_BOARD.size() << " and max order = " << max_order << " and cashiermap = " << CASHIER_MAP[cashier_id];
    thread_lock(1);
    // Wait while we can't submit an order for a variety of reasons
      //1. CORK_BOARD is full.
      //2. CORK_BOARD already has a non-null order for this thread
    while (CORK_BOARD.size() == max_order || CASHIER_MAP[cashier_id] == 1) {
      
      if (CASHIER_MAP[cashier_id] == 1){
        //waiting on this cashier thread previous order to finish
      //cout << "*putting in prev wait:  cashier id: " << cashier_id << " sandwich id: " << ORDER_RECEIVED[cashier_id].front()->sid << "\n";
        thread_wait(1, 2);
      }
      
      if (CORK_BOARD.size() == max_order) {
        // Make thread wait until maker will allow more orders. Maker will signal upon completion.
      //cout << "*putting in full wait:  cashier id: " << cashier_id << " sandwich id: " << ORDER_RECEIVED[cashier_id].front()->sid << "\n";
        thread_wait(1, 1);
      }
      
    }
    //cout << "\nDid you reach here = " << cashier_id;
    if (!ORDER_RECEIVED[cashier_id].empty()) {
      CORK_BOARD.push_back(ORDER_RECEIVED[cashier_id].front());
      CASHIER_MAP[cashier_id] = 1;
      cout << "POSTED: cashier " << cashier_id << " sandwich " << ORDER_RECEIVED[cashier_id].front()->sid << endl; 
      ORDER_RECEIVED[cashier_id].pop();
    }
    
    if (CORK_BOARD.size() == min(cashier_count, max_order)) {
      // Let the maker know to start processing orders if the board is now full.
      thread_signal(1, 3);
    }
    
    thread_unlock(1);
  }

  //cashier is done
  CASHIER_MAP[cashier_id] = 2;

}


int getClosestSandwich() {
  //get the index of the next sandwich on the corkboard whose id is closest to the current sandwich, start from -1
  int min_distance_index = 0;
  for (int i = 0; i < CORK_BOARD.size(); i++) {
    if ((abs((CORK_BOARD.at(i)->sid) - previousSandwich)) < (abs((CORK_BOARD.at(min_distance_index)->sid) - previousSandwich))) {
      min_distance_index = i;
    } 
  }
  return min_distance_index;
}

void* maker(){
  //the sandwich maker thread that takes orders from the cork board, wait if corkboard not full
  // While we are not finished or there is something left to do:
  // We keep looping till both the cashier count and board size are 0.
  while (cashier_count > 0 || CORK_BOARD.size() > 0) {
    thread_lock(1);
    // Wait while we can't start operating. That would happen if the board wasn't full.
    while (CORK_BOARD.size() < min(cashier_count, max_order)) {
      
      thread_wait(1, 3);
    }

    int idx = getClosestSandwich(); //vector keeps order of insertion
    SANDWICH_ORDER* this_sandwich_order = CORK_BOARD[idx];
    CORK_BOARD.erase(CORK_BOARD.begin() + idx);

    //get cashier id, changes cashier status
    CASHIER_MAP[this_sandwich_order->cid] = 0; //cashier is free

    //if cashier is empty delete it
    if (ORDER_RECEIVED[this_sandwich_order->cid].size() == 0){
      cashier_count--;
    }

    //update the id of sandwich to compare for next sandwich 
    previousSandwich = this_sandwich_order->sid;

    cout << "READY: cashier " << this_sandwich_order->cid << " sandwich " << this_sandwich_order->sid << endl; 

    //broadcast to who are waiting for board to have a spot
    thread_broadcast(1, 1);
    //boradcast to who are waiting for their previous order to be done
    thread_broadcast(1, 2);
    

    thread_unlock(1);
  }
}

