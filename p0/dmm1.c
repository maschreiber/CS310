

#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include "segment.h"
#include <stdio.h>
#include <limits.h>

// Heap blocks are required to be aligned to 8-byte boundary
#define ALIGNMENT 8
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
#define DUMMY_PAYLOAD_SZ 20// global variable to track end of heap
void* end_of_heap;

// global array to keep track of the segregrated free lists
void* buckets[10] = {NULL};

/* This very efficient bitwise round of sz up to nearest multiple of mult adds mult-1 to sz, 
* then masks off the bottom bits to compute least multiple of mult that is greater/equal than 
* sz. This value is returned if larger than MIN_PAYLOAD. Otherwise, MIN_PAYLOAD is returned.
*/
static size_t roundup(size_t sz, size_t mult){
    size_t rounded_size = (sz + mult-1) & ~(mult-1);
    if(rounded_size < MIN_PAYLOAD) return MIN_PAYLOAD;
    else return rounded_size;
}

// Given a pointer to start of payload, simply back up to access its block header
static inline size_t* hdr_for_payload(void *payload){
    return (size_t *)((char *)payload - sizeof(size_t));
}

// Given a pointer to block header, advance past header to access start of payload
static inline void* payload_for_hdr(void *header){
    return (char*)header + sizeof(size_t);
}

// Given a pointer to free block header, advance past header to start of prev pointer
static inline void* get_prev_ptr(void* block) {
    return (char*)block + sizeof(size_t);
}

// Given a pointer to free block header, adva/nce past header and prev pointer to start of next pointer
static inline void* get_next_ptr(void* block) {
    return (char*)block + sizeof(size_t) + sizeof(void*);
}

// Given a pointer to block header, read payload size
static inline size_t get_payload_size(void* block) {
    return *(size_t*)block & ~PAYLOAD_SIZE_MASK;
}

// Given a pointer to block header, return pointer to block footer
static inline void* get_footer_addr(void* header) {
    return (char*)header + sizeof(size_t) + get_payload_size(header);
}

//Given a pointer to a block header or footer and a paylaod size, update the header/footer with the new payload size
static inline void set_payload_size(void* hdr_or_ftr, size_t newsz) {
        *(size_t*)hdr_or_ftr = newsz;
}

//Given a void* pointer to a block's prev or next pointer, update it's value to pointee
static inline void set_ptr(void* ptr, void* pointee) {
    *(void**)ptr = pointee;
}

//Given a pointer to block header or footer, read state of block (allocated=0 or free=4)
static inline size_t get_block_state(void* block) {
    return *(size_t *)block & CURR_BLOCK_MASK;
}

/* Function: hash
* ----------------------
* The hash function hashes a payload size into one of the segregreated free lists. If the payload size
* is greater than the smallest payload size stored in the last bucket, then the method returns LAST_BUCKET_INDEX.
* Otherwise, the method used __builtinclzl to compute the array index the current block should be stored in.
*/
int hash(size_t payloadsz) {
    if(payloadsz >= LAST_BUCKET_SIZE) return LAST_BUCKET_INDEX;
    return BITS_IN_SIZET - __builtin_clzl((unsigned long)payloadsz) - 1 - SHIFT;
}

/* Function: update_dummy_state
* ----------------------
* The update_dummy_state function updates the state of the dummy header/footer
*/
void update_dummy_state(void* dummy_hdr_or_ftr) {
        *(size_t*)dummy_hdr_or_ftr = (*(size_t*)dummy_hdr_or_ftr & ~CURR_BLOCK_MASK);
}

/* Function: update_block_state
* ----------------------
* The update_block_state function updates a block's state to either ALLOCATED or FREED. To mark a block as
* allocated, the method turns off the 4's bit by &ing the header and footer with the negated CURR_BLOCK_MASK.
* To mark a block as free, the method turns on the 4's bit by |ing the header and footer with CURR_BLOCK_MASK.
*/
void update_block_state(void* header, int state) {
    void* footer = get_footer_addr(header);
    if(state == ALLOCATED) {
        *(size_t*)header = (*(size_t*)header & ~CURR_BLOCK_MASK);
        *(size_t*)footer = (*(size_t*)footer & ~CURR_BLOCK_MASK);
    } else {
        *(size_t*)header = (*(size_t*)header | CURR_BLOCK_MASK);
        *(size_t*)footer = (*(size_t*)footer | CURR_BLOCK_MASK);
    }
}

/* Function: format_block
* ----------------------
* The format_block_function formats a block by updating the stored payload size and block status. Both the header
* and footer's payload sizes are set to payload_sz and the block's state is updated to either ALLOCATED or FREED.
*/
void format_block(size_t payload_sz, void* header, size_t block_state) {
    set_payload_size(header, payload_sz);
    void* footer = get_footer_addr(header);
    set_payload_size(footer, payload_sz);
    update_block_state(header, block_state);
}

/* Function: delete_from_free_list
* ----------------------
* The delete_from_free_list function removes void* block from its designated free list. There are 4 cases the
* method handles. The first case is when the block is the only element in the free list. In this case, the bucket
* is set to NULL. If the block is the first element of the free list (but not the only element), the bucket is set
* to point to the block following the current block. If the block is the last element in the free list (but not the only
* element), the previous block's next pointer is set to NULL. Finally, if the block is somewhere in the middle of the
* free list, we set the previous block's next pointer equal to the block following the current block and the next block's
* previous pointer equal to the block preceding the current block.
*/
void delete_from_free_list(void* block, int bucket_index) {
    void* prev_ptr = get_prev_ptr(block);
    void* next_ptr = get_next_ptr(block);
    void* prev_block = *(void**)prev_ptr;
    void* next_block = *(void**)next_ptr;
    if(prev_block == NULL && next_block == NULL) buckets[bucket_index] = NULL;
    else if(prev_block == NULL) {
        buckets[bucket_index] = next_block;
        void* next_block_prev_ptr = get_prev_ptr(next_block);
        set_ptr(next_block_prev_ptr, NULL);
    } else if(next_block == NULL) {
        void* prev_block_next_ptr = get_next_ptr(prev_block);
        set_ptr(prev_block_next_ptr, NULL);
    } else {
        void* prev_block_next_ptr = get_next_ptr(prev_block);
        set_ptr(prev_block_next_ptr, next_block);
        void* next_block_prev_ptr = get_prev_ptr(next_block);
        set_ptr(next_block_prev_ptr, prev_block);
    }
}

/* Function: add_to_free_list
* ----------------------
* The add_to_free_list function adds curr_block to the front of the free list stored at the bucket_indexth bucket
* of the global array. The method first sets the block's previous pointer to NULL (since it will be the first element
* in the list) and sets the block's next pointer to the element that is currently at the front of the list. Then, the
* front of that particular free list is set to point to curr_block. If the free list was not empty before the addition
* curr_block, next_block's previous pointer is set to curr_block.
*/
void add_to_free_list(void* curr_block, int bucket_index) {
    void* prev_ptr = get_prev_ptr(curr_block);
    set_ptr(prev_ptr, NULL);
    void* next_ptr = get_next_ptr(curr_block);
    set_ptr(next_ptr, buckets[bucket_index]);
    buckets[bucket_index] = curr_block;
    void* next_block = *(void**)next_ptr;
    if(next_block != NULL) {
        void* next_block_prev_ptr = get_prev_ptr(next_block);
        set_ptr(next_block_prev_ptr, curr_block);
    }    
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
    void* start_of_heap = init_heap_segment(1);
    memset(start_of_heap, 0, PAGE_SIZE);
    memset(buckets, 0, sizeof(buckets));
    end_of_heap = (char*)start_of_heap + PAGE_SIZE;
    set_payload_size(start_of_heap, DUMMY_PAYLOAD_SZ);
    update_dummy_state(start_of_heap);
    void* first_block = (char*)start_of_heap + sizeof(size_t);
    size_t first_blk_payloadsz = PAGE_SIZE - 2*HEADER_AND_FOOTER;
    set_ptr(get_prev_ptr(first_block), NULL);
    set_ptr(get_next_ptr(first_block), NULL);
    format_block(first_blk_payloadsz, first_block, FREED);
    void* dummy_header = (char*)get_footer_addr(first_block) + sizeof(size_t);
    set_payload_size(dummy_header, DUMMY_PAYLOAD_SZ);
    update_dummy_state(dummy_header);
    int arr_index = hash(first_blk_payloadsz);
    add_to_free_list(first_block, arr_index);
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
void* first_fit(size_t nbytes, int bucket_index) {
    void* curr_block = buckets[bucket_index];
    while(curr_block != NULL) {
        size_t payload_sz = get_payload_size(curr_block);
        if(payload_sz >= nbytes) {
            delete_from_free_list(curr_block, bucket_index);
            update_block_state(curr_block, ALLOCATED);
            return payload_for_hdr((size_t*)curr_block);
        }    
        void* next_ptr = get_next_ptr(curr_block);
        curr_block = *(void**)next_ptr;
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
void split(void* payload_ptr, size_t nbytes) {
    void* block_header = hdr_for_payload(payload_ptr);
    size_t old_payloadsz = get_payload_size(block_header);
    if(old_payloadsz - nbytes < MIN_BLOCK_SZ) return;
    format_block(nbytes, block_header, ALLOCATED);
    void* second_block_header = (char*)get_footer_addr(block_header) + sizeof(size_t);
    size_t second_payload_sz = old_payloadsz - nbytes - HEADER_AND_FOOTER;
    format_block(second_payload_sz, second_block_header, FREED);
    int bucket_index = hash(second_payload_sz);
    add_to_free_list(second_block_header, bucket_index);
}

/* Function: request_additional_memory
* ----------------------
* The request_additional_memory is a helper function for mymalloc that handles the extension of
* the heap. The method requests npages additional pages of memory, and returns NULL if the extension
* request fails. Assuming the allocation was successful, the new pages are initialized to 0, and the
* end_of_heap is incremented to mark the new end of the heap. The new block is then formatted as allocated
* and split. Our dummy header is then re-written at the end of the heap and set as ALLOCATED before a pointer
* to the payload of the newly-created block is returned.
*/
void* request_additional_memory(size_t npages, size_t payload_sz) {
    void* new_pages = extend_heap_segment(npages);
    if(new_pages == NULL) return NULL;
    memset(new_pages, 0, npages*PAGE_SIZE);
    end_of_heap = (char*)end_of_heap + npages*PAGE_SIZE;
    void* new_block_hdr = (char*)new_pages - sizeof(size_t);
    format_block(npages*PAGE_SIZE - HEADER_AND_FOOTER, new_block_hdr, ALLOCATED);
    split((char*)new_block_hdr + sizeof(size_t), payload_sz);
    void* dummy_header = (char*)end_of_heap - sizeof(size_t);
    set_payload_size(dummy_header, DUMMY_PAYLOAD_SZ);
    update_dummy_state(dummy_header);
    return payload_for_hdr((size_t*)new_block_hdr);
}

/* Function: mymalloc
* ----------------------
* The mymalloc function handles memory allocation requests. If the requested size is equal to 0 or greater
* than INT_MAX, the method returns NULL. Otherwise, the client's requested size is rounded up to a multiple
* of 8. The method searches the free list using first fit, starting at the bucket index corresponding to the
* requested payload size. If first-fit finds a block large enough for the malloc request, we attempt to split
* it to improve utilization and then return a pointer to the block's payload. If first-fit is unable to find
* an appropriately sized block, additional pages of memory are requested.
*/
void *dmalloc(size_t requestedsz) {
    if(requestedsz == 0 || requestedsz > INT_MAX) return NULL;
    size_t nbytes = roundup(requestedsz, ALIGNMENT);
    int arr_index = hash(nbytes);
    for(int i = arr_index; i < NBUCKETS; i++) {
        void* payload_ptr = first_fit(nbytes, i);
        if(payload_ptr == NULL) continue;
        split(payload_ptr, nbytes);
        return payload_ptr;
    }  
    size_t npages = 1 + roundup(requestedsz + 2*sizeof(size_t), PAGE_SIZE)/PAGE_SIZE;
    return request_additional_memory(npages, nbytes);
}

/* Function: coalesce
* ----------------------
* The coalesce function attempts to coalesce a recently freed block with neighboring blocks to reduce
* external fragmentation. The method handles 4 possible cases. In the first case, both neigbhoring blocks
* are allocated, so no coalescing is performed. If the previous block is free and the next block is allocated,
* the previous and current blocks are joined and placed back into the free list as 1 contiguous block. If the
* previous block is allocated and the next block is free, the current and next blocks are joined and placed back
* into the free list as 1 contiguous block. If both the previous and next blocks are free, the previous, current,
* and next blocks are joined and placed back into the free list a 1 contiguous block.
*/
void* coalesce(void* curr_block) {
    void* prev_block_ftr = (char*)curr_block - sizeof(size_t);
    size_t prev_state = get_block_state(prev_block_ftr);
    void* next_block_hdr = (char*)get_footer_addr(curr_block) + sizeof(size_t);
    size_t next_state = get_block_state(next_block_hdr);
    if(prev_state == ALLOCATED && next_state == ALLOCATED) return curr_block;
    else if(prev_state == BLOCK_IS_FREE && next_state == ALLOCATED) {
        void* prev_block = (char*)prev_block_ftr - get_payload_size(prev_block_ftr) - sizeof(size_t);
        int arr_index = hash(get_payload_size(prev_block));
        delete_from_free_list(prev_block, arr_index);
        size_t newsz = get_payload_size(curr_block) + get_payload_size(prev_block) + HEADER_AND_FOOTER;
        format_block(newsz, prev_block, FREED);
        return prev_block;
    } else if(prev_state == ALLOCATED && next_state == BLOCK_IS_FREE) {
        void* next_block = (char*)get_footer_addr(curr_block) + sizeof(size_t);
        int arr_index = hash(get_payload_size(next_block));
        delete_from_free_list(next_block, arr_index);
        size_t newsz = get_payload_size(curr_block) + get_payload_size(next_block) + HEADER_AND_FOOTER;
        format_block(newsz, curr_block, FREED);
        return curr_block;
    } else {
        void* prev_block = (char*)prev_block_ftr - get_payload_size(prev_block_ftr) - sizeof(size_t);
        int arr_index = hash(get_payload_size(prev_block));
        delete_from_free_list(prev_block, arr_index);
        void* next_block = (char*)get_footer_addr(curr_block) + sizeof(size_t);
        arr_index = hash(get_payload_size(next_block));
        delete_from_free_list(next_block, arr_index);
        size_t newsz = get_payload_size(prev_block) + get_payload_size(curr_block) + get_payload_size(next_block) + 2*HEADER_AND_FOOTER;
        format_block(newsz, prev_block, FREED);
        return prev_block;
    }
}

/* Function: myfree
* ----------------------
* The myfree function handles freeing of previously allocated blocks. If the method is passed a NULL pointer,
* it returns, as there is no memory to free. Otherwise, it updates the block's state to free, attempts to
* coalesce with neighboring blocks, and then adds it to the appropriate free list according to the block's
* payload size.
*/
void dfree(void *ptr) {
    if(ptr == NULL) return;
    void* curr_block = hdr_for_payload(ptr);
    update_block_state(curr_block, FREED);
    curr_block = coalesce(curr_block);
    size_t payloadsz = get_payload_size(curr_block);
    int bucket_index = hash(payloadsz);
    add_to_free_list(curr_block, bucket_index);
}



/* Function: search_free_list
* ----------------------
* The search_free_list function traverses the free lists and counts the total number of blocks stored in
* the 10 free lists. The method returns the number of blocks found in the free list.
*/
int search_free_list() {
    void* curr_block;
    int free_blocks = 0;
    for(int i = 0; i < NBUCKETS; i++) {
        curr_block = buckets[i];
        while(curr_block != NULL) {
            free_blocks++;
        }
    }
    return free_blocks;
}

/* Function: search_heap
* ----------------------
* The seach_heap function traverses the heap one block at a time. It counts the
* total number of free blocks and verifies that each block's header and footer
* match. If any blocks's header & footer do not match, the method returns false.
*/
bool search_heap(void * curr_block, int* num_free) {
    int free_blocks = 0;    
    while((char*)curr_block < (char*)end_of_heap - sizeof(size_t)) {
        void * curr_ftr = get_footer_addr(curr_block);
        if(*(size_t *)curr_block != *(size_t *)curr_ftr) return false;
        size_t state = get_block_state(curr_block);
        if(state == BLOCK_IS_FREE) free_blocks++;
        curr_block = (char *)curr_ftr + sizeof(size_t);
    }
    *num_free = free_blocks;
    return true;
}

/* Function: validate_heap
* ----------------------
* The validate_heap function is a debugging routine that verifies the interal structure of
* the heap appears to be what we would expect based on our implementation of heap allocator.
* It compares the number of free blocks found from traversing the heap to the number of free
* blocks contained in the free list to verify that they match. It also verifies that each block's
* header and footer match.
*/
bool validate_heap(){  
	void * curr_block = (char*)heap_segment_start() + sizeof(size_t);
    int num_free_blocks_1 = 0;
    if(!search_heap(curr_block, &num_free_blocks_1)) return false;
    int num_free_blocks_2 = search_free_list();
	return (num_free_blocks_1 == num_free_blocks_2);
}
