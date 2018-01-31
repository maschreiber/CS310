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
metadata_t *freelist = NULL;
metadata_t *lastVisited = NULL;
void *bp0 = NULL;

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

// /* Function: get_right_list
//  * ------------------------
//  * Finds and returns the index of the list where a 
//  * free node of size "size" would be stored
//  */
// static inline int get_right_list(size_t size){
//   //Number of binary digits up to the msb that is on. Offset by four because list one starts at 2^3
//   return(32 - __builtin_clz(size) - 4);
// }

// /* Function: add_to_list
//  * ---------------------
//  * Takes in a metadata_t and a list index that corresponds to
//  * the list it is in.
//  * Appends the node to the begging of the list 
//  */
// static void add_to_list(metadata_t *node, int listIndex){
//   node->next = list_arr[listIndex];
//   node->prev = NULL;
//   if(list_arr[listIndex]) list_arr[listIndex]->prev = node;
//   list_arr[listIndex] = node;
// }

// /* Function: remove_from_list
//  * --------------------------
//  * Takes in a metadata_t and a list index that corresponds to
//  * the list it is in.
//  * Removes the node from its list.
//  */
// static void remove_from_list(metadata_t *node, int listIndex){
//   if(!node->prev && !node->next) list_arr[listIndex] = NULL; // if it is the only one
//   if(node->prev) node->prev->next = node->next;
//   else list_arr[listIndex] = node->next; // if node is the first element, make the second element the start of the list
//   if(node->next) node->next->prev = node->prev;
// }

metadata_t* find_fit(metadata_t *header, size_t size)
{
  metadata_t* ptr = header;
  
  while(ptr != NULL)
  {
    if(ptr->size >= (size + sizeof(metadata_t)) && ptr->available == 1)
    {
      return ptr;
    }
    lastVisited = ptr;
    ptr = ptr->next;
  }  
  return ptr;  
}

void splitChunk(metadata_t* ptr, unsigned int numbytes)
{
  metadata_t *newChunk = NULL; 
  
  newChunk = (metadata_t *)ptr->end + numbytes;
  newChunk->size = ptr->size - numbytes- sizeof(metadata_t);
  newChunk->available = 1;
  newChunk->next = ptr->next;
  newChunk->prev = ptr;
  
   if((newChunk->next) != NULL)
   {      
      (newChunk->next)->prev = newChunk;
   }
  
  ptr->size = numbytes;
  ptr->available = 0;
  ptr->next = newChunk;
}



bool dmalloc_init() {
  

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
  /* returns heap_region, which is initialized to freelist */
  

  bp0 = sbrk(0);
  void *bp1;

  /* Q: Why casting is used? i.e., why (void*)-1?  WHY? */
  if (sbrk(max_bytes)== (void *) - 1)
      return false;
 //Create the first chunk with size equals all memory available in the heap after setting the new breakpoint
  bp1 = sbrk(0);
  freelist = bp0;
  freelist->size = max_bytes-METADATA_T_ALIGNED;
  freelist->available = 0;
  freelist->next = NULL;
  freelist->prev = NULL;
 
  return true;
}



void *dmalloc(size_t numbytes) {

   /* initialize through sbrk call first time */
  if(freelist == NULL) {      
    if(!dmalloc_init())
      return NULL;
  }

  assert(numbytes > 0);

  
  printf("debugg\n");
  metadata_t *ptr;
  

  //Split the chunk into two: one with size request by user, other with the remainder.
  ptr = freelist;
    

  // find the first free heap chunk that is big enough
  metadata_t *smallest_chunk = NULL; //first chunk encountered that is big enough
  smallest_chunk = find_fit(freelist, numbytes);

  if(smallest_chunk != NULL){

    // cut the chunk down to a reasonable size
    if(smallest_chunk->size > numbytes){
      splitChunk(smallest_chunk, numbytes);
    }
  }
  return smallest_chunk;
}

void mergeChunkPrev(metadata_t *freed)
{ 
  metadata_t *prev;
  prev = freed->prev;
  
  if(prev != NULL && prev->available == 1)
  {
    prev->size = prev->size + freed->size + sizeof(metadata_t);
    prev->next = freed->next;
    if( (freed->next) != NULL )
      freed->next->prev = prev;
  }
}

/* mergeChunkNext: merge one freed chunk with the following chunk (in case it is free as well)
     chunkStatus* freed: pointer to the block of memory to be freed.
     retval: void, the function modifies the list
*/
void mergeChunkNext(metadata_t *freed)
{  
  metadata_t *next;
  next = freed->next;
  
  if(next != NULL && next->available == 1)
  {
    freed->size = freed->size + sizeof(metadata_t) + next->size;
    freed->next = next->next;
    if( (next->next) != NULL )
      (next->next)->prev = freed;
  }
}
void dfree(void *ptr) {

  metadata_t *toFree = NULL;
  toFree = ptr - sizeof(metadata_t);
  
  if(toFree >= freelist && toFree <= (metadata_t *)bp0)
  {
    toFree->available = 1;  
    mergeChunkNext(toFree);
    mergeChunkPrev(toFree);
  
  }
}


//   if(!ptr) return;
//   metadata_t *header = hdr_for_payload(ptr);
//   size_t size = header->size;
//   metadata_t *node = (metadata_t*) ptr;

//   //coalescing
//   metadata_t *next_header = (metadata_t*)((char*)ptr + size);
//   // while the addr of the next header is free and within the current heap segment
//   while(next_header->free){
//     metadata_t *nextNode = payload_for_hdr(next_header);
//     int listIndex = get_right_list(next_header->size);
//     remove_from_list(nextNode, listIndex);
//     header->size += next_header->size + sizeof(metadata_t);
//     next_header = (metadata_t*) ((char*)ptr + header->size);
//   }

//   int listIndex = get_right_list(header->size);
//   header->free = true;
//   add_to_list(node, listIndex);
// }



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

 

/* for debugging; can be turned off through -NDEBUG flag*/
void print_freelist() {
  metadata_t *freelist_head = freelist;
  while(freelist_head != NULL) {
    DEBUG("\tfreelist Size:%zd, Head:%p, Prev:%p, Next:%p\t",
	  freelist_head->size,
	  freelist_head,
	  freelist_head->prev,
	  freelist_head->next);
    freelist_head = freelist_head->next;
  }
  DEBUG("\n");
}
