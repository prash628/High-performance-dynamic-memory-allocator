


/*
 * mm.c
 * Name:Prashant Srinivasan
 * Andrew Id: psriniv1
 * Solution Description:
 *
 *  Method Used: Segregated Free Lists(using first fits within the lists)
 *
 *
 *  Description of the segregated free lists and the heap structure:
 *  Heap Structure:
        --Assume that the heap starts at x. Starting from this point on,
            the pointers for all the segregated lists are stored.

        --No of segregated lists are chosen to be 28(i.e. 0 to 27 positions).

        --From point x, each list gets 8 bytes worth of heap memory to store
            the address of the first node in that segList. Initially, they
            store NULL. Afterwards, they may store the address of the first
            node of a certain size, if present

        --Each segregated list only has elements of a particular size range.
            In particular, except the 0th seg list, each ith seg list store
            all nodes which fall within the range size 2^(i+5) to 2^(i+6)
            The 0th seg list stores all sizes until the size of 32.
            --32 is chosen to be the MINSEGLISTSIZE; not to be confused with
               the MINBLOCKSIZE. MINSEGLISTSIZE just says that the first seg
               list store data from sizes 0 until 2^5,i.e. 32 inclusive

        --After these addresses for all 28 Segregated lists are stored, the
          prologue header and the prologue footer is stored(worth 4bytes each)
          Then, epilogue header is stored. This would help for one to know the
          end of heap
          -- Between the epilogue and prologue the real data-i.e. data that is
            stored by user after calling malloc is stored. It need not just be
            data. It could also be free blocks.

        The above statements are condensed as follows:
        Heap in order:
        1. First (28*8) bytes: One 8 byte block for each segregated list
        They contain pointers to each segregated list. NULL if none are there.
        2. Prologue Header- 4 bytes
        3. Prologue Footer- 4 bytes
        4. Malloc'd data: This could range from 0 to the max range
        5. Epilogue: 4 bytes


 *  Desciption about blocks:
        --The minimum size of any block that a user could get is 24.

       --Any block that is allocated or free contains 24 bytes

        --The structure of a block that is allocated is as follows:
            --1. The first 4 bytes(Header):
                    Contain the size and allocated/not-allocated info
            --2. The next x bytes:
                    2.1. If the size of asked block is a multiple of 8
                        and is greater than 16- then it is all of that
                    2.2. If the size of asked block is a multiple of 8
                        and is less than 16- then 16 is given to the user,
                        but the rest of it could be considered as padding.
                    2.3. If the size of asked block is a not a multiple of 8
                        and is less than 16- then 16 is given to the user,
                        but the rest of it could be considered as padding.
                    2.4. If the size of asked block is a not a multiple of 8
                        and is greater than 16- then that size is given,
                        and the rest of it(which is given to make it a
                        multiple of 8 could be considered as padding.)
            --3. The last 4 bytes: Same as header or the first 4 bytes


        --The structure of a block that is not allocated/free is as follows:
            --1. The first 4 bytes(Header):
                    Contain the size and allocated/not-allocated info
            --2. The next 8 bytes: NEXT Pointer
                    Contain the address of the next free list block
            --3. The next 8 bytes: PREV Pointer
                    Contain the address of the previous free list block
            --4. In-between bytes: Some garbage data
            --5. The last 4 bytes: Same as header or the first 4 bytes
 *
 * Other design Decisions recap:
   -- 28 different segregated lists are chosen with increasing power of 2.
      Reason: Easier to code.
        It is acknowledged that it could be tweaked.
   -- The first fit is chosen within every segregated list. This obviously
        improves performance, but it certainly does cause some fragmentation
        Whenever there is no fit within the list, to find a fit, the next
        bigger list is chosen and the process occurs iteratively.
   --Blocks are coalesced according to class lecture slides/text book.
        The only difference is that they're freed and coalesced into
        correct/ specific segregated lists when compared to the
        explicit lists discussed in class.
   --Freeing blocks:
        The freed blocks are coalesced and put into appropriate segregated
        list
   --Heap Extension:Whenever there is block of size x that a user needs is
        not already present:
            -- First it is checked if there is a free block at the end. If
                yes, only the rest of it is extended to improve util %.If
                no, it extends heap by CHUNKSIZE:  (1<<8) bytes
            Reason for CHUNKSIZE being (1<<8) bytes: If higher, it means that
            the utilization decreases. If less, it means that the kernel has
            to be asked for memory all the time, which reduces throughput.
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
/* Reference: CSAPP 3e textbook*/
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<8)  /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 24     /* For explicit list, this is the min block size*/
#define MINSEGLISTSIZE 32     /* For seg list, this is the min block size*/
#define SEGLISTS 27         /* This contains the number of segmented lists
* ranging from 2^4 to 2^32 with increments by power of 2*/


#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))


/* Read and write an address(double word) at address p */
#define GET2W(p)       ((char *) *(unsigned int **)(p))
#define PUT2W(p, val)  (*(unsigned int **)(p) = (unsigned int *)(val))



/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute where its next and previous free ptrs are*/
#define NXTFP(bp)       ((char **)(bp))
#define PRVFP(bp)       (((char **)(bp)) + DSIZE)

/* Given block ptr bp, compute location of its next and previous free ptrs */
#define NXTFREE_BLKP(bp)       ((char**)bp)
#define PRVFREE_BLKP(bp)       ((char**)(bp + DSIZE))

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((void *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((void *)(bp) - DSIZE)))
/* $end mallocmacros */
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static char *heap_freelistp = 0;  /* Pointer to first free block */
static char *segListHeadPtr = 0;
/* segListHeadPtr-Pointer to the set of seglists' starting address */
static char *epilogueAddress = 0;
/* epilogueAddress-Pointer to the set of seglists' starting address */


/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static int find_seg_list(size_t asize);
/* find_seg_list gives the seg list number for a specific size */
static int find_max_range(int blockNum);
/* find_max_range gives the max limit for a seg lists' block number */
static char *find_seg_list_address(void *bp);
/* This gives the seg list address for a specific size */
static void remove_from_seg_list(void *bp);
/* remove_from_seg_list removes the block from appropriate seg list*/
static void add_to_seg_list(void *bp);
/* add_to_seg_list adds the block to the appropriate segList */
static int isLastBlockFree();
/* isLastBlockFree tells if the last block is free or not*/
static int sizeOfLastFreeBlock();
/* sizeOfLastFreeBlock tells the size of the last block if free */



void mm_checkheap(int verbose);
/* The following functions are helper functions for checkheap */
static void printblock(void *bp);
static void header_footer_chk(void *bp);
/* Checks if the header and footer of all the blocks are consistent or not*/
static void seg_list_freeness_consistency(void *bp);
/* Checks if all the blocks in a particular segmented list are free or not*/
static void seg_list_size_consistency(void *bp,int blockNum);
/* Checks if blocks in segmented list follow the size constraint*/
static void print_seg_list();

/*
 * mm_init - Initialize the memory manager
 */
/*
 * Initialize: return -1 on error, 0 on success.
 * Reference: Most part taken from CSAPP textbook implementation
 */
int mm_init(void)
{
    int i;
    heap_listp = 0;
    heap_freelistp = 0;

    // The initial heap is set-up to hold all the seglist' pointers
    if ((heap_listp = mem_sbrk(4*WSIZE+((1+SEGLISTS)*DSIZE))) == (void *)-1)
        return -1;

    segListHeadPtr=heap_listp;

    /* The following sets up the initial empty heap with all seglist ptrs*/
    for(i=0; i<=SEGLISTS; i++)
    {
        /*Position 0 corresponds to sizes from 0 to 2^5 with 2^5 included
         *Similarly, position i corresponds to 2^(4+i) to 2^(4+i+1)
         */
        PUT2W(segListHeadPtr + (i*DSIZE), 0);
    }
    heap_listp=heap_listp+ ((1+SEGLISTS)*DSIZE);
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp+ (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */


    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    epilogueAddress=heap_listp + (3*WSIZE);
    heap_listp=heap_listp+(2*WSIZE) ;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * malloc
 */
/*
 * mm_malloc - Allocate a block with at least size bytes of payload.
 * Returns the address of the block of specified size.
 * Reference: Most part taken from CSAPP textbook implementation
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    void *bp;
    int sizeOfLastBlock;

    if (heap_listp == 0)
    {
        mm_init();
    }

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= (2*DSIZE))
        asize = 3*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {

        place(bp, asize);
        return bp;
    }

    /*
     *Rationale: To not extend un-neccessarily
     *The last block is checked if free or not
     *If last block is free, then only get the rest that is needed
     */
    if(isLastBlockFree())
    {
        sizeOfLastBlock=sizeOfLastFreeBlock();
        extendsize=asize-sizeOfLastBlock;

    }
    else
    {
        extendsize = MAX(asize,CHUNKSIZE);
    }



    /* No fit found. Get more memory and place the block */
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}


/*
 * mm_free - Free a block
 * Returns nothing.
 * Calls coalesce in the process too.
 * Reference: Taken from CSAPP textbook implementation
 */
void mm_free(void *bp)
{
    if (bp == 0)
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0)
    {
        mm_init();
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}


/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 * Reference: Part taken from CSAPP textbook implementation. So, the
    definition of cases remain true to what was said in the book or class
 */

static void *coalesce(void *bp)
{
    //Coalesce also ensures that free block gets into the correct free list
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)              /* Case 1 */
    {
        add_to_seg_list(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc)        /* Case 2 */
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_from_seg_list(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        add_to_seg_list(bp);
        return(bp);
    }

    else if (!prev_alloc && next_alloc)        /* Case 3 */
    {
        bp=PREV_BLKP(bp);
        size += GET_SIZE(HDRP(bp));
        remove_from_seg_list(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        add_to_seg_list(bp);
        return(bp);

    }

    else                                       /* Case 4 */
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_from_seg_list(NEXT_BLKP(bp));
        bp=PREV_BLKP(bp);
        size += GET_SIZE(HDRP(bp));
        remove_from_seg_list(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        add_to_seg_list(bp);
        return(bp);

    }
    return bp;
}

/*
* mm_realloc - Naive implementation of realloc
* Reference: CSAPP 3e textbook
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
static void *extend_heap(size_t words)
{
    void *bp;
    size_t size;


    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer,
     * next Free, previous Free and the epilogue header */

    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block header */

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    epilogueAddress=HDRP(NEXT_BLKP(bp));

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 *
 * Reference: mm-naive.c
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
 * mm_checkheap - Check the heap for correctness
 The following functions are helper functions for checkheap
static void printblock(void *bp);
static void header_footer_chk(void *bp);
static void seg_list_freeness_consistency(void *bp);
static void seg_list_size_consistency(void *bp,int blockNum);
static void print_seg_list();

    CheckHeap Description:
   To check heap, the following avenues were made:
   1. Printing the blocks:  Printing the blocks were by far, the most useful
      debugging tool. It prints out all the blocks in the heap.
   2. Check if header and footer of each block matches:
            header_footer_chk(void *block) ensures that check
   3. Check that, for a particular segregated list, it only contains the
      free blocks of a certain size: seg_list_size_consistency ensures it
   4. Check that, for a particular segregated list, it only contains the
      free blocks: seg_list_freeness_consistency ensures that constraint.
   5. Loop detected: Not done because manually looking into them by
      printing them was better for me personally.

 */
void mm_checkheap(int verbose)
{
    int blockNum;
    int i=1*verbose;
    verbose=i;
    char *bp = heap_listp;

    print_seg_list();
    // This prints the segregated list pointers along with their locations.

    for (blockNum=0; blockNum <= SEGLISTS; blockNum = blockNum+ 1)
    {
        bp = (segListHeadPtr+(blockNum*8));
        seg_list_freeness_consistency(bp);
        seg_list_size_consistency(bp,blockNum);
    }


    //Now print out the heap blocks
    printf("Heap (%p):\n", heap_listp);


    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        printblock(bp);
        header_footer_chk(bp);

    }

    printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

/*
 * The remaining routines are internal helper routines
 */

/* printblock:
 *   Prints the description of a block-i.e. its header, footer,
 *   allocation information and if free, also its next pointer
 *   previous pointer.
 * Parameter: bp contains the address of a specific block to be printed
 * Returns nothing
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
    if(halloc==0)
    {
        printf("%p: FreeNext: [%p] FreePrev: [%p]\n",
                bp,GET2W(NXTFREE_BLKP(bp)), GET2W(PRVFREE_BLKP(bp)));
    }
}


/* header_footer_chk:
 *   This checks if the header and footer
 *   of a block given as input is consistent
 * Parameter: bp contains the address of a specific block
 * Returns nothing
*/
static void header_footer_chk(void *bp)
{
    //Runs silently unless there is a mismatch between header and footer
    size_t hsize, halloc, fsize, falloc;
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if(hsize!=fsize ||(falloc!=halloc))
    {
        printf("\n Inconsistent header and footer. Recheck block %p\n",bp);
    }
}



/* seg_list_freeness_consistency:
 *   This checks if all the blocks in a particular segmented list
 *   are free
 * Parameter: bp contains the address of a specific segmented list's
 *  first node address
 * Returns nothing
*/
static void seg_list_freeness_consistency(void *bp)
{
    size_t halloc;
    //Check for freeness consistency within a specific seg list
    for (; bp != NULL; bp = GET2W(NXTFREE_BLKP(bp)))
    {
        halloc = GET_ALLOC(HDRP(bp));
        if(halloc!=0)
        {
            printf("\n Error: There is an allocated block ");
            printf("in the segmented free list area-[%p]\n", bp);
        }

    }
}




/* seg_list_size_consistency:
 *   This checks if all the blocks in a particular segmented list
 *   adhere to the size constraint
 * Parameter: bp contains the address of a specific segmented list's
 *  first node address
 * Returns nothing
*/
static void seg_list_size_consistency(void *bp,int blockNum)
{
    size_t hsize;
    size_t maxSegSize; //Max size for a give seg list

    maxSegSize=(size_t)find_max_range(blockNum);

    //Check for size consistency within a specific seg list
    for (; bp != NULL; bp = GET2W(NXTFREE_BLKP(bp)))
    {
        hsize = GET_SIZE(HDRP(bp));
        if(hsize>maxSegSize ||(hsize<(maxSegSize/2)))
        {
            printf("\n Error: There is a block in the wrong");
            printf( "segmented free list-[%p]\n", bp);
        }
    }
}



/* print_seg_list:
 *   This prints all the seg list pointers along with their locations.
 * Parameter: None
 * Returns nothing
*/
static void print_seg_list()
{
    int blockNum;
    char *bp = heap_listp;
    printf("\n---- Printing out the initial seg list blocks!---------");
    for (blockNum=0; blockNum <= SEGLISTS; blockNum = blockNum+ 1)
    {
        printf("The block Number %d which is located at",blockNum);
        printf("%p has the address : [%p] \n",bp,GET2W((bp)));
    }

}


/* isLastBlockFree:
 *   This tells if the last block is free or not
 * Parameter: None
 * Returns 1 if true, 0 if false
*/
static int isLastBlockFree()
{
    char *blockBeforeEpilogue;
    blockBeforeEpilogue=epilogueAddress-WSIZE;
    if(GET_ALLOC(blockBeforeEpilogue)==0)
    {
        return(1);
    }
    else
    {
        return(0);
    }
}



/* sizeOfLastFreeBlock:
 *   This tells the size of the last block if free
 * Parameter: None
 * Returns size of last block
 * Precondition: There must be a last block and it has to be free
*/
static int sizeOfLastFreeBlock()
{
    char *blockBeforeEpilogue;
    blockBeforeEpilogue=epilogueAddress-WSIZE;
    return(GET_SIZE(blockBeforeEpilogue));
}


/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 *
 * Parameter: None
 * Returns Nothing
 */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{

    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (MINBLOCKSIZE))
    {
        /*Here the first part is allocated to the user and
        the latter part is added to the appropriate free list*/

        remove_from_seg_list(bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        add_to_seg_list(bp);
        // Ensure that the rest of the block is put into appropriate SEGLIST
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        remove_from_seg_list(bp);

    }

}


/* sizeOfLastFreeBlock:
 *   This adds the block to the appropriate segList at the front.
 * Parameter: bp, which is the block to be added to list
 * Returns Nothing
 * Precondition: Coalescing is done before
*/
static void add_to_seg_list(void *bp)
{

    char *currentSegListHead;
    char *segListPointsTo;

    PUT2W(PRVFREE_BLKP(bp),0);
    currentSegListHead=find_seg_list_address(bp);
    // This gets the addressof the seg list
    segListPointsTo=GET2W((currentSegListHead));
    // Gets the location where the seg list first points to

    if(segListPointsTo==0)
    {
        // There are no other elements in SEGLIST- THis would be the first one
        PUT2W(NXTFREE_BLKP(bp),0);
    }
    else
    {
        /* There is an node that it points to . Do 2 things:
         * 1. Update next pointer of bp.
         * 2.Update previous pointer of the previous first block
         */
        PUT2W(NXTFREE_BLKP(bp),segListPointsTo);
        PUT2W(PRVFREE_BLKP(segListPointsTo),bp);
    }
    PUT2W(currentSegListHead,bp);
}


/* remove_from_seg_list:
 *   This removes the block from appropriate SEGLIST and ensures consistency
 * Parameter: bp, which is the block to be removed from list
 * Returns Nothing
*/
static void remove_from_seg_list(void *bp)
{
    char *nextFreePtr;
    char *prevFreePtr;
    char *currentSegListHead;

    nextFreePtr=GET2W(NXTFREE_BLKP(bp));
    // Give the address of next free block
    prevFreePtr=GET2W(PRVFREE_BLKP(bp));
    // Gives the address of the previous free block

    if(prevFreePtr==0)
    {
        //Block's specific header is NULL-i.e.this is the 1st element in list
        currentSegListHead=find_seg_list_address(bp);
        PUT2W(currentSegListHead,nextFreePtr);
        if(nextFreePtr!=0)
        {
            //If next isnt NULL, ensure it's previous pointer points to NULL
            PUT2W(PRVFREE_BLKP(nextFreePtr),0);
        }
    }
    else
    {
        // The block's previous pointer is not NULL- there is a node before it
        PUT2W(NXTFREE_BLKP(prevFreePtr),nextFreePtr);
        if(nextFreePtr!=0)
        {
            PUT2W(PRVFREE_BLKP(nextFreePtr),prevFreePtr);
        }
    }
}



/* find_fit:
 *   Find a fit for a block with asize bytes.
 *
 * Parameter: asize- the size for which block has to be found
 * Returns address for block if present. NULL if none could be found.
*/
static void *find_fit(size_t asize)
{
    /*Go through all the free lists in order from the correct size
     *to the largest possible.
     */
    int blockNum;
    void *bp;
    for (blockNum=find_seg_list(asize);
     blockNum <= SEGLISTS; blockNum = blockNum+ 1)
    {
        bp = (segListHeadPtr+(blockNum*8));
        if(GET2W(bp)==0)
        {
            // There are no blocks within the size limit at all

        }
        else
        {
            //Check within a specific seg list
            for (bp = GET2W(bp); bp != NULL; bp = GET2W(NXTFREE_BLKP(bp)))
            {
                if ( (asize <= (size_t)GET_SIZE(HDRP(bp))))
                {
                    return bp;
                }
            }

        }
    }
    return NULL; /* No fit */
}




/* find_seg_list_address:
 *  Gives the seg list address for any random block
 *  For instance, for blocks between sizes 0  and 32(inclusive)
 *  it returns the pointer for the 0th block
 * Parameter: bp
 * Returns block address-seg list address for any random block
*/
static char *find_seg_list_address(void *bp)
{
    int segListNum;
    size_t csize = GET_SIZE(HDRP(bp));
    segListNum=find_seg_list(csize);
    return((char *)segListHeadPtr+(segListNum*DSIZE));
}




/* find_seg_list:
 *  This gives the block number for a specific size
 * Parameter: asize
 * Returns integer that is a block number
*/
static int find_seg_list(size_t asize)
{
    int segListCounter;
    size_t x;
    x=MINSEGLISTSIZE;
    for(segListCounter=0; segListCounter<=SEGLISTS; segListCounter+=1)
    {
        if(asize<=(x))
        {
            return(segListCounter);
        }
        x=x*2;
    }
    return(-1);
}



/* find_max_range:
 *  This gives the max limit for a block number(i.e. specific seg list)
 * Parameter: blockNum
 * Returns integer that is the max limit for a block number
*/
static int find_max_range(int blockNum)
{
    int segListCounter;
    size_t x;
    x=MINSEGLISTSIZE;
    for(segListCounter=0; segListCounter<=blockNum; segListCounter+=1)
    {
        x=x*2;
    }
    return((int)x);
}

