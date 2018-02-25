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


