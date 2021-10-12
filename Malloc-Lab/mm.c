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
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* get size and allocated bit of a block */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* compute address of header and footer from a given pointer of a block */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* compute address of next and previous block from a given pointer of a block */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)- DSIZE)))

/* compute address of next and previous free block from a given pointer of a free block */
#define NEXT_PTR(bp) ((char *)(bp) + WSIZE)
#define PREV_PTR(bp) ((char *)(bp))
#define GET_NEXT(bp) (*(char **)(NEXT_PTR(bp)))
#define GET_PREV(bp) (*(char **)(bp))
#define PUT_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void *seg_listp[13];
static void insert_free_blk(void *bp);
static void remove_free_blk(void *bp);

static int get_list_idx(size_t asize);

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
    char *bp;
    for (int i = 0; i < 13; i++)
        seg_listp[i] = NULL;
        
    if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;

    /* Set Null pointers*/
    PUT_PTR(PREV_PTR(bp), NULL);
    PUT_PTR(NEXT_PTR(bp), NULL);

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
    PUT_PTR(PREV_PTR(bp), NULL);
    PUT_PTR(NEXT_PTR(bp), NULL);

    return coalesce(bp);
}

static void *coalesce(void *bp) {

    size_t prev_alloc, next_alloc;
    size_t size;

    void *next_blk = NEXT_BLKP(bp);
    void *prev_blk = PREV_BLKP(bp);

    prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {             /* case 1 */
        insert_free_blk(bp);
        return bp;
    }
    else if (prev_alloc && (!next_alloc)) {     /* case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_free_blk(next_blk);
    }
    else if ((!prev_alloc) && next_alloc) {     /* case 3 */
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        remove_free_blk(prev_blk);
        bp = prev_blk;
    } 
    else {                                      /* case 4 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        remove_free_blk(next_blk);
        remove_free_blk(prev_blk);
        bp = prev_blk;
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_free_blk(bp);
    return bp;
}

static void *find_fit(size_t asize) {

    void *bp;
    int idx = get_list_idx(asize);
    for (; idx < 13; idx++) {
        bp = seg_listp[idx];
        for (; bp != NULL; bp = GET_NEXT(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize) {

    size_t blk_size = GET_SIZE(HDRP(bp));
    size_t split_size = blk_size - asize;

    remove_free_blk(bp);
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
        insert_free_blk(bp);
    }
}

static void remove_free_blk(void *bp) {

    void *prev_blk = GET_PREV(bp);
    void *next_blk = GET_NEXT(bp);

    size_t size = GET_SIZE(HDRP(bp));
    int idx = get_list_idx(size);

    if (!prev_blk && !next_blk)     /* empty linked-list */
        seg_listp[idx] = NULL;
    else if (!prev_blk && next_blk) /* first node */
    {
        PUT_PTR(PREV_PTR(next_blk), NULL);
        seg_listp[idx] = next_blk;
    }
    else if (prev_blk && !next_blk) /* last node */
    {
        PUT_PTR(NEXT_PTR(prev_blk), NULL);
    } else
    {
        PUT_PTR(PREV_PTR(next_blk), prev_blk);
        PUT_PTR(NEXT_PTR(prev_blk), next_blk);
    }
}

/* insert free block into segragated free list in order of block size */
static void insert_free_blk(void *bp) {
    
    size_t size = GET_SIZE(HDRP(bp));
    int idx = get_list_idx(size);
    void *root = seg_listp[idx];
    void *prev = NULL;
    void *next = root;
    while (next) {
         if (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(next))) break;
         prev = next;
         next = GET_NEXT(next);
    }

    if (next == root)
    {
        PUT_PTR(PREV_PTR(bp), NULL);
        PUT_PTR(NEXT_PTR(bp), next);
        if (next)
            PUT_PTR(PREV_PTR(next), bp);
        seg_listp[idx] = bp;
    } else
    {
        PUT_PTR(PREV_PTR(bp), prev);
        PUT_PTR(NEXT_PTR(bp), next);
        PUT_PTR(NEXT_PTR(prev), bp);
        if (next)
            PUT_PTR(PREV_PTR(next), bp);
    }
}

static int get_list_idx(size_t asize) {
    int idx = 0;
    while (asize > 1 && idx < 12) {
        asize >>= 1;
        idx++;
    }
    return idx;
}