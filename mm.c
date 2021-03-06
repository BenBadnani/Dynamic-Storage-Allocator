/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
#include <stdbool.h> 

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* bu username : eg. jappavoo */
    "bbadnani",
    /* full name : eg. jonathan appavoo */
    "Ben Badnani",
    /* email address : jappavoo@bu.edu */
    "bbadnani@bu.edu",
    "",
    ""
};


// static function declarations
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *recoalesce(void* bp);
static void* mm_brute_realloc(void *bp, size_t size);
static bool should_recoalesce(void *bp, size_t size);
//static int mm_check(void);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) 
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp,compute address of its header and footer */
#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp,compute address of next and previous blocks */
#define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


// global heap_list pointer 
void *heap_listp;

void *last_free_block_found; 

/* 
 * mm_init - initialize the malloc package.
 * Code from pg 831 CSAPP
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2*WSIZE);
    last_free_block_found = heap_listp; 

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
    extend_heap - if more heap is required for a given 
    allocator request, add more free blocks of memory
    via a mem_sbrk call 
    pg 831 CSAPP
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
    coalesce - search to see if there are any contiguous
    free blocks of heap, and merge them
    pg 833 CSAPP
*/
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { 
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 
        PUT(HDRP(bp), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
        PUT(FTRP(bp), PACK(size, 0)); 
        PUT(HDRP(PREV_BLKP(bp)), 
        PACK(size, 0)); 
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); 
        bp = PREV_BLKP(bp);
    }

    if ((last_free_block_found > bp) && ((char *)last_free_block_found < NEXT_BLKP(bp))) 
	    last_free_block_found = bp;
    return bp; 
}

/* 
 *     mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *     Code from pg 834 CSAPP 
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t asize; 
    size_t extendsize; 
    char *bp;

    // make size round up to nearest 8 multiple
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);

    //mm_check();
    return bp;
}

/*
    find_free - find the first fit for a given size among
    the free blocks in the heap
    pg 856 CSAPP
*/ 
static void *find_fit(size_t size)
{
    void* point_in_the_list = last_free_block_found; 

    // from the last free block to the end of the list
    for(; GET_SIZE(HDRP(last_free_block_found)) > 0; last_free_block_found = NEXT_BLKP(last_free_block_found)){
	    if(!GET_ALLOC(HDRP(last_free_block_found)) && (size <= GET_SIZE(HDRP(last_free_block_found)))) {
		    return last_free_block_found;
	    }
    }

    // if not found in second half of the heap, start from the beginning

    for(last_free_block_found = heap_listp; last_free_block_found < point_in_the_list; last_free_block_found = NEXT_BLKP(last_free_block_found)){
	    if(!GET_ALLOC(HDRP(last_free_block_found)) && (size <= GET_SIZE(HDRP(last_free_block_found)))) {
		    return last_free_block_found;
	    }
    }
    return NULL;
}

/*  place - find a block of free memory in heap
    allocate the passed in asize, and 
    either allow for interanl or external fragmentation
    pg 856 CSAPP
*/
static void place(void *bp, size_t asize){

    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2*DSIZE)){
	    PUT(HDRP(bp), PACK(asize, 1));
	    PUT(FTRP(bp), PACK(asize, 1));
	    bp = NEXT_BLKP(bp);
	    PUT(HDRP(bp), PACK(csize - asize, 0));
	    PUT(FTRP(bp), PACK(csize - asize, 0));
	}

    else{
	    PUT(HDRP(bp), PACK(csize, 1));
	    PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 *  mm_free - Change header and footer of 
 *  respective block by changing header and footer alloc to 0 
 *  Code from pg 833 CSAPP
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);

    //mm_check();
}

/*
 * mm_realloc - given a pointer previously called by malloc or realloc,
 * adjust size of respective block of memory to match requirements of passed in size
   or find new block in heap to allocate the size.
 */
void *mm_realloc(void *bp, size_t size)
{
    // if bp points to null,
    // return mm_malloc(size)
    if(bp == NULL)
        return mm_malloc(size);
    
    // if size is zero, simply free the block
    if(size == 0){
        mm_free(bp);
        return NULL;
    }

    size_t asize, current_size; 

    current_size = GET_SIZE(HDRP(bp));

     if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);


    if(asize <= current_size){
        return bp; 
    }

     if(should_recoalesce(bp, asize)){
        void *bp_prev = bp;
        bp = recoalesce(bp);
        memcpy(bp, bp_prev, asize);
        return bp;
    }
    
    return mm_brute_realloc(bp, size);
}

/*
    should_recoalesce - checks to see 
    if recaolesce function necessary 
    for mm_realloc calls
*/
static bool should_recoalesce(void *bp, size_t size)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp))); 
    size_t prev_size = GET_SIZE(FTRP(PREV_BLKP(bp))); 
    size_t size_bp = GET_SIZE(HDRP(bp));

    bool ret = false; 

    // CASE 1 - next blkp is free and its size >= bp's size - size
    if(next_alloc == 0 && (next_size + size_bp >= size)){
        ret = true;
    }
    // CASE 2 - prev blkp is free and its size >= bp's size - size
    else if(prev_alloc == 0 && (prev_size + size_bp >= size)){
        ret = true;
    }
    // CASE 3 - prev and next block are free, and their combined size >= bp's size - size
    else if(next_alloc == 0 && prev_alloc == 0 && 
        ((next_size + prev_size + size_bp) >= size)){
        ret = true;
    }
    // CASE 4 - neither are free, or neither alloc is free, return 0
   return ret; 
}

/*
    mm_brute_realloc - brute force realloc,
    finds new space in heap to allocate block of size passed in.
    frees block previously pointed to by bp
*/
static void* mm_brute_realloc(void *bp, size_t size){

    // allocate new memory for realloc call
    void* new_ptr = mm_malloc(size);
    // check if malloc works
    if(new_ptr == NULL){
        return NULL;
    }
    
    memcpy(new_ptr, bp, size);

    // free the old block of memory that has now been reallocated
    mm_free(bp);

    return new_ptr; 
}

/*
    recoalesce - coalesces contiguous blocks
    that can be used to prospectively satisfy 
    realloc requests
*/
static void *recoalesce(void* bp){

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { 
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 
        PUT(HDRP(bp), PACK(size, 1)); 
        PUT(FTRP(bp), PACK(size,1));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
        PUT(FTRP(bp), PACK(size, 1)); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1)); 
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1)); 
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 1)); 
        bp = PREV_BLKP(bp);
    }

    if ((last_free_block_found > bp) && ((char *)last_free_block_found < NEXT_BLKP(bp))) 
	    last_free_block_found = bp;

    return bp; 

}

/*
    mm_current_and_next_free - takes in pointer bp, 
    returns true if bp and block after bp are free,
    false otherwise

static bool mm_current_and_next_free(bp){

    return (!(GET_ALLOC(HDRP(bp))) && !(GET_ALLOC(HDRP(NEXT_BLKP(bp))))); 
     
}
*/
/* mm_check - checks to make sure allocated and free blocks
   are within heap boundaries, that there are no contiguous free blocks
   that have not been coalesced, and that no allocated blocks overlap.

static int mm_check(void){

    void* start = mem_heap_lo(); 
    void* end = mem_heap_hi();
    void* bp; 
    int ret = 1;

    // check to see if address is valid heap address

    // scans entire heap
    for(bp = start; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        // check to see if any pointers exceed heap high and low space
        if(bp < start || bp > end){
            printf("Error: %p does not point to a valid heap block\n", bp);
            // if bp is outside heap, contiguous blocks are likely outside as well, terminate mm_check
            break; 
        }
        // check for any contiguous free blocks
        if((void *)NEXT_BLKP(bp) < end){ // to avoid seg faults
            if(mm_current_and_next_free(bp)){
                printf("Error: %p and %p have not been coalesced\n", bp, NEXT_BLKP(bp));
                break; 
            }
            if((!!(GET_SIZE(HDRP(bp)) ^ GET_SIZE(FTRP(bp)))) 
                & GET_ALLOC(HDRP(bp)) 
                &  GET_ALLOC(HDRP(NEXT_BLKP(bp)))){ // and if both blocks are allocated
                printf("Error: The allocated blocks, %p and %p, overlap\n", bp, NEXT_BLKP(bp));
                break;
            }
        }
        
    }
    // condition only met if bp points to epilogue block
    if(GET_SIZE(bp) == 0 && GET_ALLOC(bp)){
        ret = 0; 
    }
    return ret;
}
*/