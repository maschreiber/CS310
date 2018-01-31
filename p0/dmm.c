#include <stdio.h>  // needed for size_t
#include <unistd.h> // needed for sbrk
#include <assert.h> // needed for asserts
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "dmm.h"




// typedef struct metadata {
//    size_t is the return type of the sizeof operator. Since the size of an
//    * object depends on the architecture and its implementation, size_t is used
//    * to represent the maximum size of any object in the particular
//    * implementation. size contains the size of the data object or the number of
//    * free bytes
   
//   size_t size;
//   size_t state;
//   char padding1; // padding to make the header 8 bytes
//   char padding2;
//   char padding3;
//   struct metadata *next;
//   struct metadata *prev;
// } metadata_t;




/* struct: metadata_t
 * ---------------
 * A header that comes before each chunk of memory that
 * contains the size of the memory and whether or not
 * that memory is free.
 */
typedef struct metadata {
  size_t szn;
  struct metadata_t *next;
  struct metadata_t *prev;
} metadata_t;

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


// Array of pointers to the begginings of the free lists
static metadata_t *list_arr[NUM_LISTS];

//static metadata_t* freelist = NULL; /* THE INITIAL FREE LIST, WITH MAX_HEAP_SIZE AS THE SIZE CLASS*/



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
static inline metadata_t *hdr_for_payload(void *payload)
{
  return (metadata_t *)((char *)payload - sizeof(metadata_t));
}

// Given a pointer to block header, advance past
// header to access start of payload
static inline void *payload_for_hdr(metadata_t *header)
{
  return (char *)header + sizeof(metadata_t);
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
 * Takes in a metadata_t and a list index that corresponds to
 * the list it is in.
 * Appends the node to the begging of the list 
 */
static void add_to_list(metadata_t *node, int listIndex){
  node->next = list_arr[listIndex];
  node->prev = NULL;
  if(list_arr[listIndex]) list_arr[listIndex]->prev = node;
  list_arr[listIndex] = node;
}

/* Function: remove_from_list
 * --------------------------
 * Takes in a metadata_t and a list index that corresponds to
 * the list it is in.
 * Removes the node from its list.
 */
static void remove_from_list(metadata_t *node, int listIndex){
  if(!node->prev && !node->next) list_arr[listIndex] = NULL; // if it is the only one
  if(node->prev) node->prev->next = node->next;
  else list_arr[listIndex] = node->next; // if node is the first element, make the second element the start of the list
  if(node->next) node->next->prev = node->prev;
}


bool dmalloc_init() {
  

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
  /* returns heap_region, which is initialized to freelist */
  freelist = (metadata_t*) sbrk(max_bytes);

  /* Q: Why casting is used? i.e., why (void*)-1?  WHY? */
  if (freelist == (void *) - 1)
      return false;
  freelist->next = NULL;
  freelist->prev = NULL;
  freelist->size = max_bytes-METADATA_T_ALIGNED;
  return true;
}



void *dmalloc(size_t numbytes) {

  assert(numbytes > 0);
  printf("debugg\n");
  metadata_t *smallest_chunk = NULL; //first chunk encountered that is big enough
  metadata_t *header = NULL;

  // find the first free heap chunk that is big enough
  int listIndex = get_right_list(numbytes);

  for(int i = listIndex; i < NUM_LISTS; i++){
    metadata_t *cur = list_arr[i];

    while(cur){
      printf("debugg\n");
      header = hdr_for_payload(cur);

      if(header->szn >= numbytes){
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


  // cut the chunk down to a reasonable size
  if(numbytes <= (header->szn * ACCEPTABLE_FRACTION) && header->szn >= SMALLEST_CUTTABLE_CHUNK){
    size_t original_payload = header->szn;
    size_t new_payload = roundup(numbytes, ALIGNMENT); // cut off just enough for the first block
    size_t free_payload = original_payload - new_payload - sizeof(metadata_t); // remainder goes back in free list

    header->szn = new_payload;
    metadata_t *newHeader = (metadata_t *) ((char*) header + sizeof(metadata_t) + new_payload);
    newheader->szn = free_payload;
    newHeader->free = true;
    listIndex = get_right_list(free_payload);

    metadata_t *new_chunk = payload_for_hdr(newHeader);
    add_to_list(new_chunk, listIndex);
  }
  header->free = false;
  return smallest_chunk;
}


void dfree(void *ptr) {
  if(!ptr) return;
  metadata_t *header = hdr_for_payload(ptr);
  size_t size = header->szn;
  metadata_t *node = (metadata_t*) ptr;

  //coalescing
  metadata_t *next_header = (metadata_t*)((char*)ptr + size);
  // while the addr of the next header is free and within the current heap segment
  while(next_header->free){
    metadata_t *nextNode = payload_for_hdr(next_header);
    int listIndex = get_right_list(next_header->szn);
    remove_from_list(nextNode, listIndex);
    header->szn += next_header->szn + sizeof(metadata_t);
    next_header = (metadata_t*) ((char*)ptr + header->szn);
  }

  int listIndex = get_right_list(header->szn);
  header->free = true;
  add_to_list(node, listIndex);
}



//  */
// static void print_free(){
//   printf("Current State of freed lists: \n");
//   for(int i = 0; i < NUM_LISTS; i ++){
//     printf("[%d]: ", i);
//     metadata_t *cur = list_arr[i];
//     while(cur){
//       printf("%#x -> ", (int)cur);
//       cur = cur->next;
//     }
//     printf("NULL\n");
//   }
//   printf("\n");
// }

 

// /* for debugging; can be turned off through -NDEBUG flag*/
// void print_freelist() {
//   metadata_t *freelist_head = *(metadata_t**)freelist;
//   while(freelist_head != NULL) {
//     DEBUG("\tfreelist Size:%zd, Head:%p, Prev:%p, Next:%p\t",
// 	  freelist_head->size,
// 	  freelist_head,
// 	  freelist_head->prev,
// 	  freelist_head->next);
//     freelist_head = freelist_head->next;
//   }
//   DEBUG("\n");
// }
