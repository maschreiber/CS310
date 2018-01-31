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
// Minimum payload size for the last bucket. Anything larger than this will be stored in last bucket
#define LAST_BUCKET_SIZE 8192
// Index of the last bucket (there are 10 bucekts)
#define LAST_BUCKET_INDEX 9
// Number of bits in a size_t (8 bytes = 64 bits)
#define BITS_IN_SIZET 64
// Shift used in hash to map payload sizes to the proper bucket
#define SHIFT 4
// Number of buckets storing free lists
#define NBUCKETS 10
// Constant returned by get_block_state if block is free (since allocated/free bit is the 4's bit)
#define BLOCK_IS_FREE 4
// Payload size used in the dummy header and footer
#define DUMMY_PAYLOAD_SZ 20


// global variable to track end of heap
//void* end_of_heap;



// void* heap_start;
// void* heap_end;

// global array to keep track of the segregrated free lists
//void* buckets[10] = {NULL};

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

// /* Function: delete_from_free_list
// * ----------------------
// * The delete_from_free_list function removes void* block from its designated free list. There are 4 cases the
// * method handles. The first case is when the block is the only element in the free list. In this case, the bucket
// * is set to NULL. If the block is the first element of the free list (but not the only element), the bucket is set
// * to point to the block following the current block. If the block is the last element in the free list (but not the only
// * element), the previous block's next pointer is set to NULL. Finally, if the block is somewhere in the middle of the
// * free list, we set the previous block's next pointer equal to the block following the current block and the next block's
// * previous pointer equal to the block preceding the current block.
// */
// void delete_from_free_list(metadata_t* block, int bucket_index) {
//     metadata_t* prev_block = *(metadata_t**)prev_ptr;
//     metadata_t* next_block = *(metadata_t**)next_ptr;
//     if(prev_block == NULL && next_block == NULL) buckets[bucket_index] = NULL;
//     else if(prev_block == NULL) {
//         buckets[bucket_index] = next_block;
//         metadata_t* next_block_prev_ptr = get_prev_ptr(next_block);
//         set_ptr(next_block_prev_ptr, NULL);
//     } else if(next_block == NULL) {
//         void* prev_block_next_ptr = get_next_ptr(prev_block);
//         set_ptr(prev_block_next_ptr, NULL);
//     } else {
//         void* prev_block_next_ptr = get_next_ptr(prev_block);
//         set_ptr(prev_block_next_ptr, next_block);
//         void* next_block_prev_ptr = get_prev_ptr(next_block);
//         set_ptr(next_block_prev_ptr, prev_block);
//     }
// }

// /* Function: add_to_free_list
// * ----------------------
// * The add_to_free_list function adds curr_block to the front of the free list stored at the bucket_indexth bucket
// * of the global array. The method first sets the block's previous pointer to NULL (since it will be the first element
// * in the list) and sets the block's next pointer to the element that is currently at the front of the list. Then, the
// * front of that particular free list is set to point to curr_block. If the free list was not empty before the addition
// * curr_block, next_block's previous pointer is set to curr_block.
// */
// void add_to_free_list(metadata_t* curr_block, int bucket_index) {
//     *(metadata_t**)curr_block->prev = NULL;
//     *(metadata_t**)curr_block->next = buckets[bucket_index];
//     buckets[bucket_index] = curr_block;
//     metadata_t* next_block = *(metadata_t**)curr_block->next;
//     if(next_block != NULL) {
//         metadata_t* next_block_prev_ptr = next_block->prev;
//         *(metadata_t**)next_block_prev_ptr = curr_block;
//     }    
// }



/* Function: add_to_list
 * ---------------------
 * Takes in a freeListNode and a list index that corresponds to
 * the list it is in.
 * Appends the node to the begging of the list 
 */
static void add_to_list(metadata_t *node, int listIndex){
  node->next = freelist[listIndex];
  node->prev = NULL;
  if(freelist[listIndex]) free[listIndex]->prev = node;
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


  // // padding
  // *(size_t *)(heap_start) = 0;

  // // set prologue header and footer
  // *(size_t *)(heap_start + (1*WORD_SIZE)) = WORD_SIZE | 1;
  // *(size_t *)(heap_start + (2*WORD_SIZE)) = WORD_SIZE | 1;

  // // set the size of the freelist
  // freelistSize = max_bytes - 4*WORD_SIZE;

  // // make room for free list header 
  // freelist = heap_start + (3*WORD_SIZE);
 
  // *(size_t *)(freelist + WORD_SIZE) = freelistSize | 0;
  // *(size_t *)(get_footer_addr(freelist + WORD_SIZE)) = freelistSize | 0;/* Free block footer */
  // *(size_t *)(get_next_ptr(freelist + WORD_SIZE)) = 0 | 1;/* New epilogue header */
  // //heap_end= HDRP(get_next_ptr(freelist + WORD_SIZE));
  // heap_start += 2 * WORD_SIZE;




  /* Q: Why casting is used? i.e., why (void*)-1? */
  // if (freelist == (void *)-1)
  //   return false;
  // freelist->next = NULL;
  // freelist->prev = NULL;
  // freelist->size = max_bytes-METADATA_T_ALIGNED;
  // return true;


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


//     metadata_t *smallest_chunk = NULL; //first chunk encountered that is big enough
//     metadata_t *header = NULL;

//     // find the first free heap chunk that is big enough
//     int listIndex = get_right_list(numbytes);
//     for(int i = listIndex; i < NUM_LISTS; i++){
//       metadata_t *cur = freelist[i];
//       while(cur){
//         header = hdr_for_payload(cur);
//         if(header->size >= numbytes){
//           smallest_chunk = cur;
//           break;
//         }
//         cur = cur->next; //move through the list
//       }
//       if(smallest_chunk){
//         remove_from_list(smallest_chunk, i);
//         break; // stop iterating if a chunk has been found
//       }
//     }

//       // cut the chunk down to a reasonable size
//     if(numbytes <= (header->size * ACCEPTABLE_FRACTION) && header->size >= SMALLEST_CUTTABLE_CHUNK){
//       size_t original_payload = *(metadata_t**)header->size;
//       size_t new_payload = roundup(numbytes, ALIGNMENT); // cut off just enough for the first block
//       size_t free_payload = original_payload - new_payload - sizeof(headerT); // remainder goes back in free list

//       header->payloadsz = new_payload;
//       headerT *newHeader = (headerT *) ((char*) header + sizeof(headerT) + new_payload);
//       newHeader->payloadsz = free_payload;
//       newHeader->free = true;
//       listIndex = get_right_list(free_payload);

//       freeListNode *new_chunk = payload_for_hdr(newHeader);
//       add_to_list(new_chunk, listIndex);
//     }
//     header->free = false;
//     return smallest_chunk;
// }

    
    


// /* Function: coalesce
// * ----------------------
// * The coalesce function attempts to coalesce a recently freed block with neighboring blocks to reduce
// * external fragmentation. The method handles 4 possible cases. In the first case, both neigbhoring blocks
// * are allocated, so no coalescing is performed. If the previous block is free and the next block is allocated,
// * the previous and current blocks are joined and placed back into the free list as 1 contiguous block. If the
// * previous block is allocated and the next block is free, the current and next blocks are joined and placed back
// * into the free list as 1 contiguous block. If both the previous and next blocks are free, the previous, current,
// * and next blocks are joined and placed back into the free list a 1 contiguous block.
// */
// metadata_t* coalesce(metadata_t *curr_block) {
//     metadata_t* prev_block_ftr = get_footer_addr(*(metadata_t**)curr_block->prev)
//     size_t prev_state = *(size_t *)curr_block->prev->state;
//     metadata_t *next_header = *(metadata_t**)curr_block+ sizeof(metadata_t);
//     metadata_t* next_block_hdr = (*(metadata_t**)curr_block->next);
//     size_t next_state = *(size_t *)curr_block->next->state;
//     if(prev_state == ALLOCATED && next_state == ALLOCATED) return curr_block;
//     else if(prev_state == BLOCK_IS_FREE && next_state == ALLOCATED) {
//         void* prev_block = *(metadata_t *)prev_block_ftr - get_payload_size(prev_block_ftr) - sizeof(metadata_t);
//         int arr_index = get_right_list(get_payload_size(prev_block));
//         remove_from_list(prev_block, arr_index);
//         size_t newsz = get_payload_size(curr_block) + get_payload_size(prev_block) + HEADER_AND_FOOTER;
//         format_block(newsz, prev_block, FREED);
//         return prev_block;
//     } else if(prev_state == ALLOCATED && next_state == BLOCK_IS_FREE) {
//         metadata_t* next_block = curr_block->next;
//         int arr_index = get_right_list(get_payload_size(next_block));
//         remove_from_list(next_block, arr_index);
//         size_t newsz = get_payload_size(curr_block) + get_payload_size(next_block) + HEADER_AND_FOOTER;
//         format_block(newsz, curr_block, FREED);
//         return curr_block;
//     } else {
//         void* prev_block = (char*)prev_block_ftr - get_payload_size(prev_block_ftr) - sizeof(size_t);
//         int arr_index = get_right_list(get_payload_size(prev_block));
//         delete_from_free_list(prev_block, arr_index);
//         void* next_block = (char*)get_footer_addr(curr_block) + sizeof(size_t);
//         arr_index = hash(get_payload_size(next_block));
//         remove_from_list(next_block, arr_index);
//         size_t newsz = get_payload_size(prev_block) + get_payload_size(curr_block) + get_payload_size(next_block) + 2*HEADER_AND_FOOTER;
//         format_block(newsz, prev_block, FREED);
//         return prev_block;
//     }
// }


// void *coalesce(metadata_t *curr_block){
//   metadata_t *prev_heap_Ptr =  (char*)curr_block - sizeof(metadata_t);
//   metadata_t *next_heap_Ptr = *(metadata_t**)curr_block->next;
//   size_t prev_alloc = *(size_t*)prev_heap_Ptr->state;
//   size_t next_alloc = *(size_t*)next_heap_Ptr->state;
//   size_t size = *(size_t*)curr_block->size;

//   if (prev_alloc && next_alloc) { /*Both previous and next blocks in heap are allocated */
//     return curr_block;
//   }

//   else if (prev_alloc == ALLOCATED && next_alloc == FREED) { /* Next block in heap is free, coalesce with it*/
//     size += get_payload_size(next_heap_Ptr);
//     PUT(HDRP(bp), PACK(size, 0));
//     PUT(FTRP(bp), PACK(size,0));
//     metadata_t *prevNode = payload_for_hdr(next_header);
//     int listIndex = get_right_list(next_header->size);
//     remove_from_list(nextNode, listIndex);
//     header->size += next_header->size + sizeof(headerT);
//     next_header = (headerT*) ((char*)ptr + header->payloadsz);
//   }

//   else if (prev_alloc == FREED && next_alloc == ALLOCATED) { /* Previous block in heap is free so coalesce with it */
//     size += GET_SIZE(HDRP(prevHeapBPtr));
//     PUT(FTRP(bp), PACK(size, 0));
//     PUT(HDRP(prevHeapBPtr), PACK(size, 0));
//     bp = prevHeapBPtr;
//   }

//   else { /*Both previous and next blocks in heap are free so coalesce with them both */
//     size += GET_SIZE(HDRP(prevHeapBPtr)) + GET_SIZE(HDRP(nextHeapBPtr));
//     PUT(HDRP(prevHeapBPtr), PACK(size, 0));
//     PUT(FTRP(nextHeapBPtr), PACK(size, 0));
//     bp = prevHeapBPtr;
//   }
//   return bp;
// }

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
    // if(ptr == NULL) return;
    // metadata_t* curr_block = hdr_for_payload(ptr);
    // update_block_state(curr_block, FREED);
    // curr_block = coalesce(curr_block);
    // size_t sz= *(size_t*)(curr_block->size);
    // int bucket_index = get_right_list(sz);
    // add_to_list(curr_block, bucket_index);





// // Heap blocks are required to be aligned to 8-byte boundary
// #define ALIGNMENT 8

// // Number of free linked lists (one for each power of two from 2^3 to 2^30
// #define NUM_LISTS 28

// // if a heap chunk uses ACCEPTABLE_FRACTION or less of its available space,
// // the chunk is split into two parts
// #define ACCEPTABLE_FRACTION .5

// // The smalles payload that we can split so that the payload of the smaller resulting
// // chunk is 8 bytes large
// #define SMALLEST_CUTTABLE_CHUNK 32

// #define ALLOCATED 0
// // Constant used to hit case in update_block_state to mark block as freed
// #define FREED 1
// // Minimum payload size for the last bucket. Anything larger than this will be stored in last bucket

// // Array of pointers to the begginings of the free lists
// static metadata_t *list_arr[NUM_LISTS];


// /* freelist maintains all the blocks which are not in use; freelist is kept
//  * sorted to improve coalescing efficiency 
//  */

// //static metadata_t* freelist = NULL;//not used;
// static void* heap_start = NULL; //points to the prologue block of the heap
// static metadata_t *freelist = NULL; //points to head of free list which is implicit so points to start of list always
// static void* heap_end = NULL;



// // Very efficient bitwise round of sz up to nearest multiple of mult
// // does this by adding mult-1 to sz, then masking off the
// // the bottom bits to compute least multiple of mult that is
// // greater/equal than sz, this value is returned
// // NOTE: mult has to be power of 2 for the bitwise trick to work!
// static inline size_t roundup(size_t sz, int mult)
// {
//   return (sz + mult-1) & ~(mult-1);
// }


// // Given a pointer to start of payload, simply back up
// // to access its block header
// static inline metadata_t *hdr_for_payload(void *payload)
// {
//   return (metadata_t *)((char *)payload - sizeof(metadata_t));
// }

// // Given a pointer to block header, advance past
// // header to access start of payload
// static inline void *payload_for_hdr(void *header)
// {
//   return (char *)header + sizeof(metadata_t);
// }

// // // Given a pointer to block header, return pointer to block footer
// // static inline metadata_t *get_footer_addr(void* header) {
// //     return (metadata_t *)((char*)header + sizeof(metadata_t) + get_payload_size(header));
// // }


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
//  * Takes in a freelistNode and a list index that corresponds to
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
//  * Takes in a freelistNode and a list index that corresponds to
//  * the list it is in.
//  * Removes the node from its list.
//  */
// static void remove_from_list(metadata_t *node, int listIndex){
//   if(!node->prev && !node->next) list_arr[listIndex] = NULL; // if it is the only one
//   if(node->prev) node->prev->next = node->next;
//   else list_arr[listIndex] = node->next; // if node is the first element, make the second element the start of the list
//   if(node->next) node->next->prev = node->prev;
// }



// void* dmalloc(size_t numbytes) {

//   /* initialize through sbrk call first time */
//   if(freelist == NULL) { 			
//     if(!dmalloc_init())
//       return NULL;
//   }


//   assert(numbytes > 0);

//   metadata_t *smallest_chunk = NULL; //first chunk encountered that is big enough
//   metadata_t *header = NULL;

//   int listIndex = get_right_list(numbytes);
//   for(int i = listIndex; i < NUM_LISTS; i++){
//     metadata_t *cur = list_arr[i];
//     while(cur){
//       //printf("debugging\n");
//       header = hdr_for_payload(cur);
//       if(header->size >= numbytes){
//   smallest_chunk = cur;
//   break;
//       }
//       cur = cur->next; //move through the list
//     }
//     if(smallest_chunk){
//       remove_from_list(smallest_chunk, i);
//       break; // stop iterating if a chunk has been found
//     }
//   }

  
//   size_t original_payload = header->size;
  

//   if(original_payload - numbytes < SMALLEST_CUTTABLE_CHUNK){
//     size_t new_payload = roundup(numbytes, ALIGNMENT); // cut off just enough for the first block
//     size_t free_payload = original_payload - new_payload - sizeof(metadata_t); // remainder goes back in free list
//     printf("debugging\n");
//     header->size = new_payload;
//     metadata_t *newHeader = (metadata_t*) ((char*) header + sizeof(metadata_t) + new_payload);
//     newHeader->size = free_payload;
//     newHeader->free= true;
//     listIndex = get_right_list(free_payload);

//     metadata_t *new_chunk = payload_for_hdr(newHeader);
//     add_to_list(new_chunk, listIndex);
//   }
//   printf("debugging\n");

//   header->free = false;
//   return smallest_chunk;
// }




// void dfree(void* ptr) {
//   if(!ptr) return;
//   metadata_t *cur = hdr_for_payload(ptr);
//   cur->free = true;
//   size_t sz = cur->size;
//   metadata_t *node = (metadata_t*) ptr;

//   //coalescing
//   metadata_t *next_header = (metadata_t*)((char*)ptr + sz);
//   while(next_header->free){
//     metadata_t *nextNode = payload_for_hdr(next_header);
//     int listIndex = get_right_list(next_header->size);
//     remove_from_list(nextNode, listIndex);
//     cur->size += next_header->size + sizeof(metadata_t);
//     next_header = (metadata_t*) ((char*)ptr + cur->size);
//   }

//   int listIndex = get_right_list(cur->size);
 
//   add_to_list(node, listIndex);
// }

// bool dmalloc_init() {

//   /* Two choices: 
//    * 1. Append prologue and epilogue blocks to the start and the
//    * end of the freelist 
//    *
//    * 2. Initialize freelist pointers to NULL
//    *
//    * Note: We provide the code for 2. Using 1 will help you to tackle the 
//    * corner cases succinctly.
//    */

//   size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
//   printf("debugging\n");
//   /* returns heap_region, which is initialized to freelist */
//   freelist = (metadata_t*) sbrk(max_bytes); 

//   for(int i = 0; i < NUM_LISTS; i ++){
//     printf("debugging\n");
//     list_arr[i] = NULL;
//   }
//   /* Q: Why casting is used? i.e., why (void*)-1? */
//   // if (freelist == (void *)-1)
//   //   return false;
//   // freelist->next = NULL;
//   // freelist->prev = NULL;
//   // freelist->size = max_bytes-METADATA_T_ALIGNED;

  

//   return true;
// }

// // bool dmalloc_init() {



// //   size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
// //   freelist = (void*) sbrk(max_bytes); // returns heap_region, which is initialized to freelist

// //   printf("debugging\n");
// //   if (freelist == (void *) - 1)
// //         return false;


// //   printf("debugging\n");
// //   memset(heap_start,0, freelist->size);

 
// //   memset((void *)list_arr, 0, sizeof(list_arr));

// //   heap_end = (char *)heap_start + freelist->size;

 
// //   metadata_t *first_blk = p

// //   printf("debugging\n");
// //   first_blk->next = NULL;

// //   printf("debugging\n");
// //   first_blk->prev = NULL;
// //   first_blk->free = true;

// //   printf("debugging\n");
// //   int listIndex = get_right_list(first_blk->size);

// //   printf("debugging\n");
// //   add_to_list(first_blk, listIndex);
// //   return true;
// //   }

//   // heap_start= freelist;
//   // heap_start->next = NULL;
//   // heap_start->prev = NULL;
//   // heap_start->size = max_bytes;
//   // /*setting heap_end*/
//   // heap_end = (void*) freelist + max_bytes - sizeof(metadata_t)


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
