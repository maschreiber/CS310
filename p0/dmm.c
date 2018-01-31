#include <stdio.h>  // needed for size_t
#include <unistd.h> // needed for sbrk
#include <assert.h> // needed for asserts
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "dmm.h"




typedef struct metadata {
  /* size_t is the return type of the sizeof operator. Since the size of an
   * object depends on the architecture and its implementation, size_t is used
   * to represent the maximum size of any object in the particular
   * implementation. size contains the size of the data object or the number of
   * free bytes
   */
  size_t size;
  size_t state;
  char padding1; // padding to make the header 8 bytes
  char padding2;
  char padding3;
  struct metadata *next;
  struct metadata *prev;
} metadata_t;


static metadata_t* freelist = NULL;

// Heap blocks are required to be aligned to 8-byte boundary
#define ALIGNMENT 8

// Number of free linked lists (one for each power of two from 2^3 to 2^30
#define NUM_LISTS 28

// Smallest block size must include 8-byte header, 8-byte footer, and 16-byte payload to store 2 pointers
#define MIN_BLOCK_SZ 32
// Size of header and footer combined (overhead for each block)
#define HEADER_AND_FOOTER 16
// Bit mask used to specify whether a current block is allocated or free
#define CURR_BLOCK_MASK 0x0000000000000004
// Minimum payload size must be able to contain 2 pointers (8 bytes each)
#define MIN_PAYLOAD 16
// Bit mask used to turn off last 3 bits of header/footer since payload size must be a multiple of 8
#define PAYLOAD_SIZE_MASK 0x7
// Constant used to hit case in update_block_state to mark block as allocated
#define ALLOCATED 0
// Constant used to hit case in update_block_state to mark block as freed
#define FREED 1



/* This very efficient bitwise round of sz up to nearest multiple of mult adds mult-1 to sz, 
* then masks off the bottom bits to compute least multiple of mult that is greater/equal than 
* sz. This value is returned if larger than MIN_PAYLOAD. Otherwise, MIN_PAYLOAD is returned.
*/
static size_t roundup(size_t sz, size_t mult){
    size_t rounded_size = (sz + mult-1) & ~(mult-1);
    if(rounded_size < MIN_PAYLOAD) return MIN_PAYLOAD;
    else return rounded_size;
}

static inline int get_right_list(size_t size){
  //Number of binary digits up to the msb that is on. Offset by four because list one starts at 2^3
  return(32 - __builtin_clz(size) - 4);
}

// Given a pointer to start of payload, simply back up to access its block header
static inline size_t* hdr_for_payload(void *payload){
    return (metadata_t*)((char *)payload - sizeof(metadata_t));
}

// Given a pointer to block header, advance past header to access start of payload
static inline void* payload_for_hdr(metadata_t* header){
    return (char*)header + sizeof(metadata_t);
}

// Given a pointer to free block header, advance past header to start of prev pointer
static inline void* get_prev_ptr(void* block) {
    return (char*)block + sizeof(size_t);
}

// Given a pointer to free block header, adva/nce past header and prev pointer to start of next pointer
static inline void* get_next_ptr(void* block) {
    return (char*)block + sizeof(size_t) + sizeof(metadata_t*);
}

// Given a pointer to block header, read payload size
static inline size_t get_payload_size(metadata_t* header) {
    return *(size_t*)header->size & ~PAYLOAD_SIZE_MASK;
}

// Given a pointer to block header, return pointer to block footer
static inline void* get_footer_addr(metadata_t* header) {
    return *(char*)header + sizeof(metadata_t) + get_payload_size(header);
}

//Given a pointer to a block header or footer and a paylaod size, update the header/footer with the new payload size
static inline void set_payload_size(metadata_t* header, size_t newsz) {
        *(size_t*)header->size = newsz;
}

//Given a void* pointer to a block's prev or next pointer, update it's value to pointee
static inline void set_ptr(void* ptr, void* pointee) {
    *(void**)ptr = pointee;
}

//Given a pointer to block header or footer, read state of block (allocated=0 or free=4)
static inline size_t get_block_state(void* block) {
    return *(size_t *)block & CURR_BLOCK_MASK;
}



/* Function: update_block_state
* ----------------------
* The update_block_state function updates a block's state to either ALLOCATED or FREED. To mark a block as
* allocated, the method turns off the 4's bit by &ing the header and footer with the negated CURR_BLOCK_MASK.
* To mark a block as free, the method turns on the 4's bit by |ing the header and footer with CURR_BLOCK_MASK.
*/
void update_block_state(metadata_t* header, int state) {
    metadata_t* footer = get_footer_addr(header);
    if(state == ALLOCATED) {
        *(size_t*)header->state = (*(size_t*)header->state & ~CURR_BLOCK_MASK);
        *(size_t*)footer->state = (*(size_t*)footer->state & ~CURR_BLOCK_MASK);
    } else {
        *(size_t*)header->state = (*(size_t*)header->state| CURR_BLOCK_MASK);
        *(size_t*)footer->state = (*(size_t*)footer->state | CURR_BLOCK_MASK);
    }
}

/* Function: format_block
* ----------------------
* The format_block_function formats a block by updating the stored payload size and block status. Both the header
* and footer's payload sizes are set to payload_sz and the block's state is updated to either ALLOCATED or FREED.
*/
void format_block(size_t sz, metadata_t* header, size_t block_state) {
    set_payload_size(header, sz);
    metadata_t* footer = get_footer_addr(header);
    set_payload_size(footer, sz);
    update_block_state(header, block_state);
}



/* Function: add_to_list
 * ---------------------
 * Takes in a freeListNode and a list index that corresponds to
 * the list it is in.
 * Appends the node to the begging of the list 
 */
static void add_to_list(metadata_t *node, int listIndex){
  node->next = freelist[listIndex];
  node->prev = NULL;
  if(freelist[listIndex]) freelist[listIndex]->prev = node;
  freelist[listIndex] = node;
  }
}

/* Function: remove_from_list
 * --------------------------
 * Takes in a freeListNode and a list index that corresponds to
 * the list it is in.
 * Removes the node from its list.
 */
static void remove_from_list(metadata_t *node, int listIndex){
  if(!node->prev && !node->next) freelist[listIndex] = NULL; // if it is the only one
  if(node->prev) node->prev->next = node->next;
  else freelist[listIndex] = node->next; // if node is the first element, make the second element the start of the list
  if(node->next) node->next->prev = node->prev;
}

/* Function: myinit 
* ----------------------
* The myinit function is responsible for configuring a new empty heap. It requests 1 page of
* memory, and initializes that page to all 0's. The array of free lists is also initialized to
* all 0's. The end_of_heap pointer is assigned to the end of the 1 allocated page. An allocated dummy 
* footer is initialized at the beginning of the heap to handle the edge case of coalesce where the block
* being freed is the first block in the heap. A similar dummy header is written in the last 8 bytes of the
* heap. The remainder of the block is formatted as a large free block and placed into the free list.f
*/
bool dmalloc_init() {
    
  //size_t freelistSize;
  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
  /* returns heap_region, which is initialized to freelist */
  freelist= (metadata_t*) sbrk(max_bytes); 
  
  if (freelist == (void *)-1)
    return false;
  freelist->next = NULL;
  freelist->prev = NULL;
  freelist->size = max_bytes-METADATA_T_ALIGNED;
  return true;
}

/* Function: first_fit
* ----------------------
* The first_fit function searches through the linked list stored at buckets[bucket_index] and
* returns the first block that is large enough to hold a payload of nbytes. If first_fit finds
* a block, it deletes that block from the free list, sets the block's state to ALLOCATED, and
* returns a pointer to the block's payload. If first_fit does not find a large enough block,
* the method returns NULL.
*/
void* first_fit(size_t numbytes, int listIndex) {
    metadata_t* curr_block = &freelist[listIndex];
    while(curr_block != NULL) {
        if(curr_block->size >= numbytes) {
            remove_from_list(curr_block, listIndex);
            curr_block->state=ALLOCATED;
            return payload_for_hdr((size_t*)curr_block);
        }    
        metadata_t* next_blk = curr_block->next;
        curr_block = *(metadata_t**)next_blk;
    }
    return NULL;
}


/* Function: split
* ----------------------
* The split function splits a large block into one block just large enough to handle the malloc request
* and a second block to store in the free list (assuming that the remaining portion of the free block is
* at least MIN_BLOCK_SIZE). If the block is large enough to split, the first block is formatted to be allocated
* and have a payload size of nbytes. The second block is formatted to be free and have a payload size of old_payload_sz
* - nbytes - HEADER_AND_FOOTER. The second block is then added to the free list.
*/
void split(metadata_t* payload_ptr, size_t numbytes) {
    metadata_t* block_header = hdr_for_payload(payload_ptr);
    size_t old_payloadsz = *(size_t *)block_header->size;
    if(old_payloadsz - numbytes < MIN_BLOCK_SZ) return;
    format_block(numbytes, block_header, ALLOCATED);
    metadata_t* second_block_header = (char*)get_footer_addr(block_header) + sizeof(metadata_t);
    size_t second_payload_sz = old_payloadsz - numbytes - sizeof(metadata_t);
    format_block(second_payload_sz, second_block_header, FREED);
    int bucket_index = get_right_list(second_payload_sz);
    add_to_list(second_block_header, bucket_index);
}

void *dmalloc(size_t numbytes) {

       /* initialize through sbrk call first time */
    if(freelist == NULL) {      
      if(!dmalloc_init())
        return NULL;
    }



    assert(numbytes > 0);
    metadata_t *payload_ptr = NULL;

    int arr_index = get_right_list(numbytes);
    for(int i = arr_index; i < NUM_LISTS; i++) {
        payload_ptr = first_fit(numbytes, i);
        if(payload_ptr == NULL) continue;
        split(payload_ptr, numbytes);
    }
    
    return payload_ptr;
  }


/* Function: myfree
* ----------------------
* The myfree function handles freeing of previously allocated blocks. If the method is passed a NULL pointer,
* it returns, as there is no memory to free. Otherwise, it updates the block's state to free, attempts to
* coalesce with neighboring blocks, and then adds it to the appropriate free list according to the block's
* payload size.
*/
void dfree(void *ptr) {

    if(!ptr) return;
    metadata_t *header = hdr_for_payload(ptr);
    size_t size = header->size;
    metadata_t *node = (metadata_t*) ptr;
      //coalescing
    metadata_t *next_header = (metadata_t*)((char*)ptr + size);
    // while the addr of the next header is free and within the current heap segment
    while(next_header->state == FREED){
      metadata_t *nextNode = payload_for_hdr(next_header);
      int listIndex = get_right_list(next_header->size);
      remove_from_list(nextNode, listIndex);
      header->size += next_header->size + sizeof(metadata_t);
      next_header = (metadata_t*) ((char*)ptr + header->size);
    }

    int listIndex = get_right_list(header->size);
    update_block_state(header, ALLOCATED);
    add_to_list(node, listIndex);
}
 

/* for debugging; can be turned off through -NDEBUG flag*/
void print_freelist() {
  metadata_t *freelist_head = *(metadata_t**)freelist;
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
