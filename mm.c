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

/* Given block ptr bp, check to see if it is last block by seeing if the next header is the epilogue */
#define LAST_BLOCK(bp) ((!(GET_SIZE(HDRP(NEXT_BLKP(bp))))) && (GET_ALLOC(HDRP(NEXT_BLKP(bp)))) ? 1 : 0) 

void *heap_listp;

/* 
 * mm_init - initialize the malloc package.
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

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


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
return bp; 

}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

 
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t asize; 
    size_t extendsize; 
    char *bp;

    // size > aggreagate free memory, extend heap
    // need aggregate free memory counter, need heap size tracker

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
    return bp;
}


static void *find_fit(size_t asize)
{
    void *bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
	    if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
		    return bp;
	    }
    }
    return NULL;
}

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
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
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

    // declaring ints after first two cases, to save memory
    size_t asize, csize, nsize; 
    int nalloc;

    asize = ALIGN(size); // rounding up size to nearest multiple of 8
    csize = GET_SIZE(HDRP(bp)); // size of current block

    if(asize <= csize){
        return bp;
    }

    nsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
    nalloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    // if the next block is free 
    if(!nalloc){
        // if bp is the last block
        if(LAST_BLOCK(bp)){ // if bp points to the last block
            if(extend_heap(asize/ WSIZE) == NULL) // extend the heap by asize/wsize blocks
                return NULL;
        }
        // if block next to bp is last block, but still not sufficient for realloc size request
        else if(nsize + csize < asize && LAST_BLOCK(NEXT_BLKP(bp))){ // 
            if(extend_heap(((asize - nsize + csize)/ WSIZE) == NULL)
                return NULL; // extend heap by the difference of how many bytes are needed
        }
        // if bp has a free block in front of it, but cannot be coalesced to satisfy
        // request, must break and look for new location in heap
        else if(nsize + csize < asize && !LAST_BLOCK(NEXT_BLKP(bp))){
            break;
        }
        // manually merging the blocks with new heap provided based on conditions above
        PUT(HDRP(bp), PACK(asize), 1)); 
        PUT(FTRP(bp), PACK(asize), 1));
        return bp; 
    }






   
}