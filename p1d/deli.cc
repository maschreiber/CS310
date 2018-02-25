#include <cstdlib>
#include <ucontext.h>
#include <deque>
#include <iterator>
#include <map>
#include <iostream>
#include "interrupt.h"
#include "thread.h"
using namespace std;
//using deque instead of queue so i can initiate it


//Sandwich: id 0~999
//one sandwich maker
//cashiers (variable numer): post orders on the board
//cork board: max_orders, order
//orders: 1) sandwich id 2) priority for cashier
//wait_queue: cashier waiting if board is full


//1) create a specified number of cashier threads to read in sandwich order from a file
//2) create one thead for the sandwich maker




struct SANDWICH_ORDER{
    int sid; //sandwich id
    int cid; //cashier id
};


map<int, SANDWICH_ORDER*> CORK_BOARD; //int = cashier id, order
int max_order;
int cashier_count;

map<int, queue<SANDWICH_ORDER*> > ORDER_RECEIVED; //int = cashier id, queue is a queue of sandwich orders: order_q

void* cashier(void* a) {
    int cashier_id = (long int) a;
    while (!ORDER_RECEIVED[cashier_id].empty()) {
        // If the cork board does not have an order from this cashier, we should put one in.
        if (CORK_BOARD.count(cashier_id) == 0 || CORK_BOARD[cashier_id] == NULL) {
            CORK_BOARD.insert(pair<int, SANDWICH_ORDER*>(cashier_id, ORDER_RECEIVED[cashier_id].front());
                              }
                              }
                              cout << " val " << arg << " val2 " <<a;
                              }
                              
                              void* maker(){
                                  
                              }
                              
                              void* startf(void* arg){
                                  
                                  for (int cashier_id = 0; cashier_id < cashier_count; i++){
                                      thread_create((thread_startfunc_t) cashier, (void*) cashier_id);
                                  }
                                  
                                  thread_create((thread_startfunc_t) maker, (void*) 100);
                                  
                              }
                              
                              
                              void* main(int argc, char *argv[]){
                                  max_order = atoi(argv[1]);
                                  cashier_count = argc - 2;
                                  if (cashier_count < max_order){
                                      max_order = cashier_count;
                                  }
                                  
                                  //read in the cashier id and sandwhich orders in order received
                                  for (int i = 2; i < argc; i++){
                                      //for each input file = for each cashier
                                      ifstream sw_input = argv[i];
                                      int new_cid = i - 2; // cashier id
                                      string new_sid_s; // sandwich id in string
                                      queue<SANDWICH_ORDER*> new_order_q; // queue of order for this cashier
                                      
                                      //for each sandwich order in cashier #
                                      if (sw_input.is_open()) // this part we learned how to use ifstream from http://www.cplusplus.com/doc/tutorial/files/
                                      {
                                          while (getline(sw_input, new_sid_s)){
                                              int new_sid = atoi(new_sid_s); //sandwich id in string
                                              SANDWICH_ORDER* neworder = new SANDWICH_ORDER;
                                              neworder->sid = new_sid;
                                              neworder->cid = new_cid;
                                              new_order_q.push(neworder); // push new order onto order queue for this cashier
                                          }
                                          sw_input.close();
                                          ORDER_RECEIVED.insert(pair<int, queue<SANDWICH_ORDER*> >(cid, new_order_q));
                                      }else{
                                          cout << "sw input not open";
                                      }
                                  }
                                  
                                  thread_libinit( (thread_startfunc_t) startf, (void*) 100);
                                  
                              }
