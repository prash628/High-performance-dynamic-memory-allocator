


/*
 * mm.c
 * Name:Prashant Srinivasan
 *  Andrew Id: psriniv1
 * Explicit Free List Implementation
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"



/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst
#define MINBLOCKSIZE 24     /* For explicit list, this is the min block size*/

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put


/* Read and write an address(double word) at address p */
#define GET2W(p)       ((char *) *(unsigned int **)(p))           //line:vm:mm:get
#define PUT2W(p, val)  (*(unsigned int **)(p) = (void *)(val))    //line:vm:mm:put



/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute where its next and previous free ptrs are*/
#define NXTFP(bp)       ((char **)(bp))                      //line:vm:mm:hdrp
#define PRVFP(bp)       (((char **)(bp)) + DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute location of its next and previous free ptrs */
#define NXTFREE_BLKP(bp)       ((char**)bp)                     //line:vm:mm:hdrp
#define PRVFREE_BLKP(bp)       ((char**)(bp + DSIZE))  //line:vm:mm:ftrp


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((void *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((void *)(bp) - DSIZE))) //line:vm:mm:prevblkp
/* $end mallocmacros */
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static char *heap_freelistp = 0;  /* Pointer to first free block */


/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
void mm_checkheap(int verbose);
static void printblock(void *bp);

//static int aligned(const void *p);

/*
 * mm_init - Initialize the memory manager
 */
/*
 * Initialize: return -1 on error, 0 on success.
 */
/* $begin mminit */
int mm_init(void)
{
    heap_listp = 0;
    heap_freelistp = 0;
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);                     //line:vm:mm:endinit
    /* $end mminit */


    /* $begin mminit */
    //mm_checkheap(10);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}
/* $end mminit */
/*
 * malloc
 */
/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    void *bp;


    //mm_checkheap(10);

    /* $end mmmalloc */
    if (heap_listp == 0)
    {
        mm_init();
    }

    /* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= (2*DSIZE))                                          //line:vm:mm:sizeadjust1
        asize = 3*DSIZE;                                        //line:vm:mm:sizeadjust2
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //line:vm:mm:sizeadjust3

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)    //line:vm:mm:findfitcall
    {

        place(bp, asize);                  //line:vm:mm:findfitplace

        return bp;
    }


    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;                                  //line:vm:mm:growheap2

    //printf("\n extend found done!");

    place(bp, asize);                                 //line:vm:mm:growheap3
    return bp;
}
/* $end mmmalloc */


/*
 * free
 */

/*
 * mm_free - Free a block
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    /* $end mmfree */
    if (bp == 0)
        return;

    /* $begin mmfree */
    size_t size = GET_SIZE(HDRP(bp));
    /* $end mmfree */
    if (heap_listp == 0)
    {
        mm_init();
    }
    /* $begin mmfree */

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
    //mm_checkheap(10);
}

/* $end mmfree */




/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp)
{
    char *nextFreePtr;
    char **rightNextFreePtr;
    char **rightPrevFreePtr;
    char **leftNextFreePtr;
    char **leftPrevFreePtr;
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));


    if (prev_alloc && next_alloc)              /* Case 1 */
    {
        nextFreePtr=heap_freelistp;
        PUT2W(NXTFREE_BLKP(bp),heap_freelistp);
        PUT2W(PRVFREE_BLKP(bp),0);

        if(nextFreePtr!=0)
        {
            PUT2W(PRVFREE_BLKP(nextFreePtr),bp);
            // The first on the previous free list's previous node
        }
        heap_freelistp=bp;
    }

    else if (prev_alloc && !next_alloc)        /* Case 2 */
    {
        rightNextFreePtr=NXTFREE_BLKP(NEXT_BLKP(bp));
        rightPrevFreePtr=PRVFREE_BLKP(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));

        if(GET2W(rightPrevFreePtr)==0)
        {
            heap_freelistp=bp;
            PUT2W(NXTFREE_BLKP(bp),GET2W(rightNextFreePtr));

            if(GET2W(rightNextFreePtr)!=0)
            {

                PUT2W(PRVFREE_BLKP(GET2W(rightNextFreePtr)),bp);
            }
            PUT2W(PRVFREE_BLKP(bp),0);

        }
        else
        {

            PUT2W(NXTFREE_BLKP(bp),heap_freelistp);
            PUT2W(PRVFREE_BLKP(heap_freelistp),bp);
            heap_freelistp=bp;
            PUT2W(NXTFREE_BLKP(GET2W(rightPrevFreePtr)),GET2W(rightNextFreePtr));
            if(GET2W(rightNextFreePtr)!=0)
            {
                PUT2W(PRVFREE_BLKP(GET2W(rightNextFreePtr)),GET2W(rightPrevFreePtr));
            }
            PUT2W(PRVFREE_BLKP(bp),0);
        }
    }

    else if (!prev_alloc && next_alloc)        /* Case 3 */
    {
        leftNextFreePtr=NXTFREE_BLKP(PREV_BLKP(bp));
        leftPrevFreePtr=PRVFREE_BLKP(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        if(GET2W(leftPrevFreePtr)==0)
        {
            bp = PREV_BLKP(bp);
            heap_freelistp=bp;
        }
        else
        {
            if(GET2W(leftNextFreePtr)==0)
            {
                PUT2W(NXTFREE_BLKP(GET2W(leftPrevFreePtr)),0);
                PUT2W(leftPrevFreePtr,0);
                PUT2W(leftNextFreePtr,heap_freelistp);
                PUT2W(PRVFREE_BLKP(heap_freelistp),leftNextFreePtr);
                bp = PREV_BLKP(bp);
                heap_freelistp=bp;
            }
            else
            {
                PUT2W(NXTFREE_BLKP(GET2W(leftPrevFreePtr)),GET2W(leftNextFreePtr));
                PUT2W(PRVFREE_BLKP(GET2W(leftNextFreePtr)),GET2W(leftPrevFreePtr));
                PUT2W(leftPrevFreePtr,0);
                PUT2W(leftNextFreePtr,heap_freelistp);
                PUT2W(PRVFREE_BLKP(heap_freelistp),leftNextFreePtr);
                bp = PREV_BLKP(bp);
                heap_freelistp=bp;
            }
        }

    }

    else                                       /* Case 4 */
    {


        leftNextFreePtr=NXTFREE_BLKP(PREV_BLKP(bp));
        leftPrevFreePtr=PRVFREE_BLKP(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        if(GET2W(leftPrevFreePtr)==0)
        {

            bp = PREV_BLKP(bp);

            heap_freelistp=GET2W(leftNextFreePtr);

            PUT2W(PRVFREE_BLKP(heap_freelistp),0);

        }
        else
        {
            if(GET2W(leftNextFreePtr)==0)
            {


                PUT2W(NXTFREE_BLKP(GET2W(leftPrevFreePtr)),0);
                PUT2W(leftPrevFreePtr,0);
                PUT2W(leftNextFreePtr,heap_freelistp);
                //PUT2W(PRVFREE_BLKP(heap_freelistp),leftNextFreePtr);
                bp = PREV_BLKP(bp);
                //heap_freelistp=bp;



            }
            else
            {
                PUT2W(NXTFREE_BLKP(GET2W(leftPrevFreePtr)),GET2W(leftNextFreePtr));
                PUT2W(PRVFREE_BLKP(GET2W(leftNextFreePtr)),GET2W(leftPrevFreePtr));
                PUT2W(leftPrevFreePtr,0);
                PUT2W(leftNextFreePtr,heap_freelistp);
                //PUT2W(PRVFREE_BLKP(heap_freelistp),leftNextFreePtr);
                bp = PREV_BLKP(bp);
                //heap_freelistp=bp;
            }
        }



        rightNextFreePtr=NXTFREE_BLKP(NEXT_BLKP(bp));
        rightPrevFreePtr=PRVFREE_BLKP(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        if(GET2W(rightPrevFreePtr)==0)
        {
            heap_freelistp=bp;
            PUT2W(NXTFREE_BLKP(bp),GET2W(rightNextFreePtr));

            if(GET2W(rightNextFreePtr)!=0)
            {

                PUT2W(PRVFREE_BLKP(GET2W(rightNextFreePtr)),bp);
            }
            PUT2W(PRVFREE_BLKP(bp),0);

        }
        else
        {

            PUT2W(NXTFREE_BLKP(bp),heap_freelistp);
            PUT2W(PRVFREE_BLKP(heap_freelistp),bp);
            heap_freelistp=bp;
            PUT2W(NXTFREE_BLKP(GET2W(rightPrevFreePtr)),GET2W(rightNextFreePtr));
            if(GET2W(rightNextFreePtr)!=0)
            {
                PUT2W(PRVFREE_BLKP(GET2W(rightNextFreePtr)),GET2W(rightPrevFreePtr));
            }
            PUT2W(PRVFREE_BLKP(bp),0);
        }
    }
    /* $end mmfree */
    /* $begin mmfree */

    return bp;
}
/* $end mmfree */
/*
 * realloc - you may want to look at mm-naive.c
 */
/*
* mm_realloc - Naive implementation of realloc
*/
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0)
    {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL)
    {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr)
    {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}


/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words)
{
    void *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;                                        //line:vm:mm:endextend

    /* Initialize free block header/footer,
    *next Free, previous Free and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          //line:vm:mm:returnblock
}
/* $end mmextendheap */
/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *mm_calloc (size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
static int in_heap(const void *p) {
   return p <= mem_heap_hi() && p >= mem_heap_lo();
}

static int aligned(const void *p) {
   return (size_t)ALIGN(p) == (size_t)p;
}*/

/*
 * mm_checkheap - Check the heap for correctness
 */
void mm_checkheap(int verbose)
{
    int i=1*verbose;
    verbose=i;
    char *bp = heap_listp;


    printf("Heap (%p):\n", heap_listp);




    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {

        printblock(bp);

    }


    printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

/*
 * The remaining routines are internal helper routines
 */


static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;


    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0)
    {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));

}
/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{

    char *nextFreePtr;
    char *prevFreePtr;
    size_t csize = GET_SIZE(HDRP(bp));

    nextFreePtr=GET2W(NXTFREE_BLKP(bp));
    prevFreePtr=GET2W(PRVFREE_BLKP(bp));

    if ((csize - asize) >= (MINBLOCKSIZE))
    {

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));

        PUT2W(NXTFREE_BLKP(bp),nextFreePtr);
        PUT2W(PRVFREE_BLKP(bp),prevFreePtr);
        if(prevFreePtr!=0)
        {
            PUT2W(NXTFREE_BLKP(prevFreePtr),bp);
        }
        else
        {
            heap_freelistp=bp;
        }
        if(nextFreePtr!=0)
        {
            PUT2W(PRVFREE_BLKP(nextFreePtr),bp);
        }

    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        if(prevFreePtr==0)
        {
            heap_freelistp=nextFreePtr;
            if(nextFreePtr!=0)
            {
                PUT2W(PRVFREE_BLKP(nextFreePtr),0);
            }
        }
        else
        {
            PUT2W(NXTFREE_BLKP(prevFreePtr),nextFreePtr);
            if(nextFreePtr!=0)
            {
                PUT2W(PRVFREE_BLKP(nextFreePtr),prevFreePtr);
            }
        }
    }

}
/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit-proto */
{
    /* $end mmfirstfit */

    /* $begin mmfirstfit */
    /* First-fit search */
    void *bp;


    for (bp = heap_freelistp; bp != NULL; bp = GET2W(NXTFREE_BLKP(bp)))
    {
        if ( (asize <= (size_t)GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL; /* No fit */
}
/* $end mmfirstfit */
