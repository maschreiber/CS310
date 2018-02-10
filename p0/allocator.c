/*
 * File: allocator.c
 * Author: Mark Schreiber
 * ----------------------
 * An allocator using doubly linked lists to store the free chunks.
 * more informtaion in readme
 */

#include <stdio.h>

#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include "segment.h"

// Heap blocks are required to be aligned to 8-byte boundary
#define ALIGNMENT 8

// Number of free linked lists (one for each power of two from 2^3 to 2^30
#define NUM_LISTS 28

// if a heap chunk uses ACCEPTABLE_FRACTION or less of its available space,
// the chunk is split into two parts
#define ACCEPTABLE_FRACTION .5

// The smalles payload that we can split so that the payload of the smaller resulting
// chunk is 8 bytes large
#define SMALLEST_CUTTABLE_CHUNK 32

// Number of extra pages that are pulled each time a new page is needed to minimize calls to extend heap
#define EXTRA_PAGES 3

//extra space given when something is realloced because the same thing is often realloced multiple times
#define REALLOC_MULTIPLIER 3

/* struct: freeListNode
 * --------------------
 * A node for a doubly linked list that keeps track of 
 * free hep chunks.
 * 
 * Stored in the first 8 bytes of each free heap chunk, 
 * right after the header.
 */
typedef struct freeListNode{
  struct freeListNode *next;
  struct freeListNode *prev;
} freeListNode;

/* struct: headerT
 * ---------------
 * A header that comes before each chunk of memory that
 * contains the size of the memory and whether or not
 * that memory is free.
 */
typedef struct {
  size_t payloadsz;
  bool free;
  char padding1; // padding to make the header 8 bytes
  char padding2;
  char padding3;
} headerT;


// Array of pointers to the begginings of the free lists
static freeListNode *list_arr[NUM_LISTS];

// Very efficient bitwise round of sz up to nearest multiple of mult
// does this by adding mult-1 to sz, then masking off the
// the bottom bits to compute least multiple of mult that is
// greater/equal than sz, this value is returned
// NOTE: mult has to be power of 2 for the bitwise trick to work!
static inline size_t roundup(size_t sz, int mult)
{
  return (sz + mult-1) & ~(mult-1);
}


// Given a pointer to start of payload, simply back up
// to access its block header
static inline headerT *hdr_for_payload(void *payload)
{
  return (headerT *)((char *)payload - sizeof(headerT));
}

// Given a pointer to block header, advance past
// header to access start of payload
static inline void *payload_for_hdr(headerT *header)
{
  return (char *)header + sizeof(headerT);
}

/* Function: get_right_list
 * ------------------------
 * Finds and returns the index of the list where a 
 * free node of size "size" would be stored
 */
static inline int get_right_list(size_t size){
  //Number of binary digits up to the msb that is on. Offset by four because list one starts at 2^3
  return(32 - __builtin_clz(size) - 4);
}

/* Function: add_to_list
 * ---------------------
 * Takes in a freeListNode and a list index that corresponds to
 * the list it is in.
 * Appends the node to the begging of the list 
 */
static void add_to_list(freeListNode *node, int listIndex){
  node->next = list_arr[listIndex];
  node->prev = NULL;
  if(list_arr[listIndex]) list_arr[listIndex]->prev = node;
  list_arr[listIndex] = node;
}

/* Function: remove_from_list
 * --------------------------
 * Takes in a freeListNode and a list index that corresponds to
 * the list it is in.
 * Removes the node from its list.
 */
static void remove_from_list(freeListNode *node, int listIndex){
  if(!node->prev && !node->next) list_arr[listIndex] = NULL; // if it is the only one
  if(node->prev) node->prev->next = node->next;
  else list_arr[listIndex] = node->next; // if node is the first element, make the second element the start of the list
  if(node->next) node->next->prev = node->prev;
}

/* The responsibility of the myinit function is to set up the heap to
 * its initial, empty ready-to-go state. This will be called before
 * any allocation requests are made. The myinit function may also
 * be called later in program and is expected to wipe out the current
 * heap contents and start over fresh. This "reset" option is specificcally
 * needed by the test harness to run a sequence of scripts, one after another,
 * without restarting program from scratch.
 */
bool myinit()
{
  init_heap_segment(0); // reset heap segment to empty, no pages allocated
  for(int i = 0; i < NUM_LISTS; i ++){
    list_arr[i] = NULL;
  }
  return true;
}

/* Function: mymalloc
 * ------------------
 * Takes in a requested size and returns a pointer to a chunk of 
 * heap allocated memory that is big enough to contain at least 
 * that many bytes
 * 
 * if the size requested is 0 or less, returns NULL
 */
void *mymalloc(size_t requestedsz){
  if(requestedsz < 1) return NULL;
  if(requestedsz > (1 << 30)) {
    printf("The requested size is too large and cannot be allocated. \n");
    return NULL;
  }
  freeListNode *smallest_chunk = NULL; //first chunk encountered that is big enough
  headerT *header = NULL;

  // find the first free heap chunk that is big enough
  int listIndex = get_right_list(requestedsz);
  for(int i = listIndex; i < NUM_LISTS; i++){
    freeListNode *cur = list_arr[i];
    while(cur){
      header = hdr_for_payload(cur);
      if(header->payloadsz >= requestedsz){
	       smallest_chunk = cur;
	       break;
      }
      cur = cur->next; //move through the list
    }
    if(smallest_chunk){
      remove_from_list(smallest_chunk, i);
      break; // stop iterating if a chunk has been found
    }
  }

  // if there is not a big enough chunk free, pull another page
  if(!smallest_chunk){
    if (!header) {
      printf("There is not enough heap remaining to accommodate the request. \n");
      return NULL;
    }
    
    smallest_chunk = payload_for_hdr(header);
  }

  // cut the chunk down to a reasonable size
  if(requestedsz <= (header->payloadsz * ACCEPTABLE_FRACTION) && header->payloadsz >= SMALLEST_CUTTABLE_CHUNK){
    size_t original_payload = header->payloadsz;
    size_t new_payload = roundup(requestedsz, ALIGNMENT); // cut off just enough for the first block
    size_t free_payload = original_payload - new_payload - sizeof(headerT); // remainder goes back in free list

    header->payloadsz = new_payload;
    headerT *newHeader = (headerT *) ((char*) header + sizeof(headerT) + new_payload);
    newHeader->payloadsz = free_payload;
    newHeader->free = true;
    listIndex = get_right_list(free_payload);

    freeListNode *new_chunk = payload_for_hdr(newHeader);
    add_to_list(new_chunk, listIndex);
  }
  header->free = false;
  return smallest_chunk;
}

/* Function: myFree
 * ----------------
 * Takes in a pointer to a currently allocated heap chunk. 
 * while the heap chunk that directly follows it in the heap segment is free,
 * the parameter chunk absorbs the next chunk
 *
 * if a null pointer is passed in, it does nothing
 */
void myfree(void *ptr){
  if(!ptr) return;
  headerT *header = hdr_for_payload(ptr);
  size_t size = header->payloadsz;
  freeListNode *node = (freeListNode*) ptr;

  //coalescing
  headerT *next_header = (headerT*)((char*)ptr + size);
  // while the addr of the next header is free and within the current heap segment
  while((uintptr_t) next_header < (uintptr_t) heap_segment_start() + (uintptr_t) heap_segment_size() && next_header->free){
    freeListNode *nextNode = payload_for_hdr(next_header);
    int listIndex = get_right_list(next_header->payloadsz);
    remove_from_list(nextNode, listIndex);
    header->payloadsz += next_header->payloadsz + sizeof(headerT);
    next_header = (headerT*) ((char*)ptr + header->payloadsz);
  }

  int listIndex = get_right_list(header->payloadsz);
  header->free = true;
  add_to_list(node, listIndex);
}

/* Function: myrealloc
 * -------------------
 * Takes in a pointer to the original memory chunk storing client information
 * and a new size needed for that information
 *
 * Returns a pointer to heap allocated memory big enough to hold at 
 * least newsz bytes of information
 *
 * if newsz is less than or equal to the original size of the heap chunk, 
 * it returns the original pointer. Otherwise, it returns REALLOC_MULTIPLIER
 * times the requested new size in anticipation of multiple reallocs of the 
 * same memory
 */
void *myrealloc(void *oldptr, size_t newsz) {
  if (newsz <= hdr_for_payload(oldptr)->payloadsz) return oldptr;
  size_t oldsz = hdr_for_payload(oldptr)->payloadsz;
  void *newptr = mymalloc(newsz * REALLOC_MULTIPLIER);
  memcpy(newptr, oldptr, oldsz);
  myfree(oldptr);
  return newptr;
}

/* Function: print_free
 * --------------------
 * prints the pointers currently within the free lists in a very readable manner
 * next and previous can easily be printed from there in gdb
 */
static void print_free(){
  printf("Current State of freed lists: \n");
  for(int i = 0; i < NUM_LISTS; i ++){
    printf("[%d]: ", i);
    freeListNode *cur = list_arr[i];
    while(cur){
      printf("%#x -> ", (int)cur);
      cur = cur->next;
    }
    printf("NULL\n");
  }
  printf("\n");
}

/* Function: validate_heap
 * -----------------------
 * Prints out the contents of the free lists each time it is called.
 * If it finds something wrong with the heap, it reaises a segmentation
 * fault which allows the user to easily poke around in gdb at the contents of the heap 
 * listed in the free lists
 */
bool validate_heap() {
  for(int i = 0; i < NUM_LISTS; i ++){
    if(list_arr[i]){
      if(list_arr[i]->prev != NULL){
	printf("First element of list %d's previous is not NULL\n", i);
	print_free();
	raise(SIGSEGV);
      }
      headerT *firstHeader = hdr_for_payload(list_arr[i]);
      if(firstHeader->free != true){
	printf("First element of list %d not marked as free\n", i);
	print_free();
	raise(SIGSEGV);
      }
      if(i != get_right_list(firstHeader->payloadsz)){
	printf("Wrong list for first element in list %d", i);
	print_free();
	raise(SIGSEGV);
      }
      freeListNode *cur = list_arr[i]->next;
      while(cur){
	if(cur->prev == NULL){ 
	  printf("Middle element of list %d's previous is NULL\n", i);
	  print_free();
	  raise(SIGSEGV);
	}
	headerT *header = hdr_for_payload(cur);
	if(header->free != true){
	  printf("Middle element of list %d not marked as free\n", i);
	  print_free();
	  raise(SIGSEGV);
	}
	if(i != get_right_list(header->payloadsz)){
	  printf("Wrong list for middle element in list %d", i);
	  print_free();
	  raise(SIGSEGV);
	}
	cur = cur->next;
      }
    }
  }

  return true;
}

