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

    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Constants and macros */
#define WSIZE 4 /* Word & header/footer size in Byte */
#define DSIZE 8 /* Double words size in Byte */
#define CHUNKSIZE (1<<12) /* Extend heap size (4K Byte) when failed to find a proper block */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* PACK size & allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* read & write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

/* get size and allocated bit of a block */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* compute address of header and footer from a given pointer of a block */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* compute address of next and previous block from a given pointer of a block */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)- DSIZE)))

static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    /* Create initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += DSIZE;

    /* Extend heap by CHUNKSIZE */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    char* bp;

    /* ignore spurious request */
    if (size == 0)
        return NULL;
    
    /* adjust size for alignment */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = (((size + DSIZE) + (DSIZE - 1))/ DSIZE) * DSIZE; /* csapp p73. (x + (1<<k) - 1) >> k */
    
    /* search free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* expand heap if no fit found */
    size_t extend_size;
    extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
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
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    size = GET_SIZE(HDRP(oldptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words) {

    char *bp;
    size_t size;
    size = (words % 2)? (words + 1) * WSIZE: words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    PUT(HDRP(bp), PACK(size, 0)); /* Header */
    PUT(FTRP(bp), PACK(size, 0)); /* Footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    return coalesce(bp);
}

static void *coalesce(void *bp) {

    size_t prev_alloc, next_alloc;
    size_t size;

    prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {          /* case 1 */
        return bp;
    }
    else if (prev_alloc && (!next_alloc)) {    /* case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    }
    else if ((!prev_alloc) && next_alloc) {    /* case 3 */
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
    } 
    else {                                /* case 4 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    return bp;
}

static void *find_fit(size_t asize) {

    size_t blk_size;
    void *bp = heap_listp;

    for (; (blk_size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
        if ((GET_ALLOC(HDRP(bp)) == 0) && (blk_size >= asize))
            return bp;
    }

    return NULL;
}

static void place(void *bp, size_t asize) {

    size_t blk_size = GET_SIZE(HDRP(bp));
    size_t split_size = blk_size - asize;

    if (split_size < 16) {
        PUT(HDRP(bp), PACK(blk_size, 1));
        PUT(FTRP(bp), PACK(blk_size, 1));
    }
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(split_size, 0));
        PUT(FTRP(bp), PACK(split_size, 0));
    }
}