#include "thread.h"
#include <fstream>
#include <stdlib.h>
#include <iostream>

using namespace std;

class METADATA {
public:
    METADATA(void* list_of_order_list_input, void* list_of_num_orders_input) {
        list_of_order_list = list_of_order_list_input;
        list_of_num_orders = list_of_num_orders_input;
    }
    
    void* list_of_order_list;
    void* list_of_num_orders;
};

static int max_orders;
static int num_orders_on_cork_board = 0;
static int num_files_cashiers;
static int num_cashier_created = 0;
static int num_cashier_alive = 0;
static int last_sandwitch = -1;
static int* cork_board_orders;
static int* cork_board_cashier;
static bool* posted_order_ready;

static unsigned int lock1 = 50;
static unsigned int cork_board_full = 100;
static unsigned int all_cashiers_created = 1000;
static unsigned int cashier_post_orders = 500;

static bool board_is_not_full () {
    return num_orders_on_cork_board < std::min(max_orders, num_cashier_alive);
}

static void remove_element (int* array, int target_sandwitch_index) {
    for (int i = target_sandwitch_index; i < num_orders_on_cork_board-1; i++) {
        array[i] = array[i+1];
    }
    
    array[num_orders_on_cork_board-1] = 0;
}

static int abs (int x, int y) {
    return x>=y ? x-y : y-x;
}

static void cashier (void* metadata_pointer) {
    thread_lock(lock1);
    num_cashier_created += 1;
    int cashier_index = num_cashier_created - 1;
    // printf("-----------------------Cashier %d created!\n", cashier_index);
    int order_index = 0;
    posted_order_ready[cashier_index] = true;
    
    num_cashier_alive += 1;
    
    void* order_list = ((void**)(((METADATA*) metadata_pointer)->list_of_order_list))[cashier_index];
    int num_order_cashier = ((int*)(((METADATA*) metadata_pointer)->list_of_num_orders))[cashier_index];
    
    if (num_cashier_created == num_files_cashiers) {
        // printf("------------------------------------------thread_signal: all_cashiers_created\n");
        thread_signal(lock1, all_cashiers_created);
    }
    
    while (order_index < num_order_cashier) {
        if (order_index == 0 || num_cashier_alive > max_orders) {
            // printf("---------------cashier %d waiting-----------------\n", cashier_index);
            thread_wait(lock1, cashier_post_orders);
        }
        
        if (board_is_not_full() && posted_order_ready[cashier_index]) {
        
            cork_board_orders[num_orders_on_cork_board] = (int) ((int*)order_list)[order_index];
            cork_board_cashier[num_orders_on_cork_board] = cashier_index;
            
            cout << "POSTED: cashier " << cashier_index << " sandwich " << cork_board_orders[num_orders_on_cork_board] << endl;
            
            order_index++;
            num_orders_on_cork_board++;
            
            posted_order_ready[cashier_index] = false;
            
            if (board_is_not_full()) {
                thread_signal(lock1, cashier_post_orders);
            }
            
            else {
                thread_signal(lock1, cork_board_full);
            }
            
            thread_wait(lock1, cashier_index);
        }
    }
    
    num_cashier_alive--;
    
    if (!board_is_not_full()) {
        thread_signal(lock1, cork_board_full);
    }
    
    thread_unlock(lock1);
    return;
}

static void SANDWITCH_MAKER (void* metadata_pointer)  {
    // printf("-------------------------SANDWITCH_MAKER CREATED!\n");
    thread_lock(lock1);
    thread_wait(lock1, all_cashiers_created);
    
    // printf("ready!\n");
    for (int i = 0; i < std::min(max_orders, num_cashier_alive); i++)
    {
        thread_signal(lock1, cashier_post_orders);
    }
    
    while (true) {
        // printf("-------------------------------------dddddd-----------------\n");
        thread_wait(lock1, cork_board_full);
        
        if (num_cashier_alive == 0) {
            break;
        }
        
        int target_sandwitch_index = 0;
        
        if (num_orders_on_cork_board > 1) {
            for (int i = 1; i < num_orders_on_cork_board; i++) {
                if (abs(cork_board_orders[i], last_sandwitch) < abs(cork_board_orders[target_sandwitch_index], last_sandwitch)) {
                    target_sandwitch_index = i;
                }
            }
        }
        
        cout << "READY: cashier " << cork_board_cashier[target_sandwitch_index] << " sandwich " << cork_board_orders[target_sandwitch_index] << endl;
        
        last_sandwitch = cork_board_orders[target_sandwitch_index];
        
        posted_order_ready[cork_board_cashier[target_sandwitch_index]] = true;
        thread_signal(lock1, cork_board_cashier[target_sandwitch_index]);
        
        remove_element(cork_board_orders, target_sandwitch_index);
        remove_element(cork_board_cashier, target_sandwitch_index);
        num_orders_on_cork_board--;
        thread_signal(lock1, cashier_post_orders);
    }
    
    thread_unlock(lock1);
    return;
}

void all_thread_creation (void* metadata_pointer) {
    start_preemptions(true, true, 1);
    
    if (thread_create((thread_startfunc_t) SANDWITCH_MAKER, metadata_pointer)) {
        printf("thread_create failing\n");
        exit(0);
    }
    
    for (int i = 0; i < num_files_cashiers; i++) {
        if (thread_create((thread_startfunc_t) cashier, metadata_pointer)) {
            printf("thread_create failing\n");
            exit(0);
        }
    }
}

int main(int argc, char *argv[]) {
    max_orders = atoi(argv[1]);
    num_files_cashiers = argc -2;
    
    int num_orders_in_total = 0;
    
    void* list_of_order_list [num_files_cashiers];
    int list_of_num_orders [num_files_cashiers];
    
    for (int i = 0; i < argc-2; i++) {
    
        std::ifstream file (argv[i+2]);
        
        int num_orders_per_file = 0;
        void* buffer;
        
        while (file.good()) {
            file >> buffer;
            num_orders_per_file++;
        }
        
        list_of_num_orders[i] = num_orders_per_file - 1;
        
        num_orders_in_total += num_orders_per_file;
    };
    
    cork_board_orders = (int*) malloc (num_orders_in_total * sizeof(int));
    
    cork_board_cashier = (int*) malloc (num_orders_in_total * sizeof(int));
    
    posted_order_ready = (bool*) malloc (num_files_cashiers * sizeof(bool));
    
    for (int i = 0; i< argc-2; i++) {
    
        int* orders_per_file = (int*) malloc ((list_of_num_orders[i]+1)*sizeof(int));
        std::ifstream file (argv[i+2]);
        int num_orders_per_file = 0;
        
        while (file.good()) {
            file >> orders_per_file[num_orders_per_file];
            num_orders_per_file++;
        }
        
        list_of_order_list[i] = (void*) orders_per_file;
    };
    
    
    METADATA metadata(list_of_order_list, (void*) list_of_num_orders);
    
    METADATA* metadata_pointer = &metadata;
    
    if (thread_libinit((thread_startfunc_t) all_thread_creation, (void*) metadata_pointer)) {
        printf("libinit failing\n");
        exit(0);
    }
    
    return 1;
}