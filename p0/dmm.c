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



void split_chunk(metadata_t *ptr, size_t numbytes)
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


metadata_t* extendH(metadata_t *lastVisitedPtr, size_t size)
{
  bp0 = sbrk(0);
  metadata_t *curBreak = bp0;    //Current breakpoint of the heap
  
  if(sbrk(size + METADATA_T_ALIGNED) == (void*) -1)
  {
    return NULL;
  }
  
  curBreak->size = size + METADATA_T_ALIGNED - sizeof(metadata_t);
  curBreak->available = 0;
  curBreak->next = NULL;
  curBreak->prev = lastVisitedPtr;
  lastVisitedPtr->next = curBreak;
  
  if(curBreak->size > size)
    split_chunk(curBreak, size);
  
  return curBreak;  
}

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

  
  
  metadata_t *ptr;
  //Split the chunk into two: one with size request by user, other with the remainder.
  ptr = freelist;
    

  // find the first free heap chunk that is big enough
  metadata_t *smallest_chunk = NULL; //first chunk encountered that is big enough
  smallest_chunk = find_fit(freelist, numbytes);
 

  if(smallest_chunk == NULL){
      //extend the heap 
      smallest_chunk = extendH(lastVisited, numbytes);
      return smallest_chunk->end;
    }
  else{
    // cut the chunk down to a reasonable size
    if(smallest_chunk->size > numbytes){
      split_chunk(smallest_chunk, numbytes);
      }
    }
  
  return smallest_chunk->end;
}

void coalesce_prev(metadata_t *freed)
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

/* coalesce_next: merge one freed chunk with the following chunk (in case it is free as well)
     chunkStatus* freed: pointer to the block of memory to be freed.
     retval: void, the function modifies the list
*/
void coalesce_next(metadata_t *freed)
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
  
  if(toFree >= freelist && toFree <= (size_t *)bp0)
  {
    toFree->available = 1;  
    coalesce_next(toFree);
    coalesce_prev(toFree);
  
  }
}

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
