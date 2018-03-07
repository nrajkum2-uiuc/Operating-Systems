/**
* Malloc Lab
* CS 241 - Spring 2018
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* the meta data that every free block will hold */
typedef struct metadata{
    size_t size;
    void * prev;
    void * next;
} meta;

/* This is the minimum that we're allowing people to malloc. This is lower bounded by the basic meta data we need as well as the end tag*/
static const size_t MIN_BLOCK_SIZE = sizeof(meta) + sizeof(size_t);

/* serves as the beginning of the free list */
static meta * head = NULL;

/* serves as the boundary to the beginning of the heap */
static void * BEGIN_HEAP = NULL;

/* removeBlock helper function -- returns block that is no longer in the free list*/

void removeBlock(meta *in){
    meta * prev = in->prev;
    meta * next = in->next;
    if(prev)
        prev->next = next;
    if(next)
        next->prev = prev;
    if(in == head)
        head = head->next;
}

/* coalesce function */

void coalesce() {
  //  fprintf(stderr, "\n coa start ");
    /*TODO: know how to split into the three cases of possible coalescing
	- this involves knowing the boundaries of the heap so you don't coalesce to anything outside of the heap for some reason
	- CASE I: coalescing to block before it
		- requires one deletion
	- CASE II: coalescing to block after it
		- requires one deletion
	- CASE III: coalescing to block before it and after it
		- requires two deletions
*/
    void * END_HEAP = sbrk(0);
    void * coa_before = (void*)( ((char*)head) - sizeof(size_t) );
    void * coa_after = (void*)( ((char*)head) + head->size );
    /* first check the boudaries of the heap */
    int case_1 = !(BEGIN_HEAP > coa_before);
    int case_2 = !(END_HEAP <= coa_after);
    size_t coa_before_size = 0;
    size_t coa_after_size = 0;
    /* next check the size of the block
	- if the size is divisible by MIN_BLOCK_SIZE, then you are dealing with a free block
	- else the adjacent block is malloc'd and you should not coalesce with it
    */
    if(case_1) {
	coa_before_size = *(size_t*)(coa_before);
	if(coa_before_size % MIN_BLOCK_SIZE != 0) {
	    case_1 = 0;
	} else {
            coa_before = (void*)( ((char*)coa_before) + sizeof(size_t) - coa_before_size);
	}
    }

    if(case_2) {
	coa_after_size = *(size_t*)(coa_after);
	if(coa_after_size % MIN_BLOCK_SIZE != 0) {
	    case_2 = 0;
        }
    }

//    fprintf(stderr, "before coa head->size: %zx ", head->size);
    /*TODO: CASE I
	- assign head to coa_before
	- assign the head->next to coa_before <- ???
	- delete (remove) coa_before from the list ( the first thing you need to do )
	- assign the head->prev to coa_before ( really just a NULL )
        - change the boundary tags of the whole coalesced block
    */
    if(case_1 && !case_2) {
	removeBlock((meta*)coa_before);
        meta * coa_block= (meta*)coa_before;
        coa_block->size = coa_block->size + head->size;
        coa_block->prev = head->prev;
	coa_block->next = head->next;
	if(head->next)
	    ((meta*)head->next)->prev = coa_block;
	size_t * end_tag = (size_t*)( ((char*)coa_block) + coa_block->size - sizeof(size_t) );
	*end_tag = coa_block->size;
        head = coa_block;
  //      fprintf(stderr, "\nCASEI\n");
    }
    /*TODO: CASE II
	- remove coa_after from the list (first thing you need to do)
	- change the boundary tags of the whole coalesced block
    */
    if(!case_1 && case_2) {
	removeBlock((meta*)coa_after);
	head->size = head->size + ((meta*)coa_after)->size;
	size_t * end_tag = (size_t*)( ((char*)head) + head->size - sizeof(size_t) );
	*end_tag = head->size;
//	fprintf(stderr, "\nCASEII\n");
    }
    /*TODO: CASE III
	- remove coa_before and coa_after fromt the list (first thing you need to do)
	- assign head to coa_before
    */
    if(case_1 && case_2) {
	removeBlock((meta*)coa_after);
	removeBlock((meta*)coa_before);
        meta * coa_block = (meta*)coa_before;
	coa_block->size = coa_block->size + head->size + ((meta*)coa_after)->size;
	coa_block->prev = head->prev;
	coa_block->next = head->next;
	if(head->next)
	    ((meta*)head->next)->prev = coa_block;
	size_t * end_tag = (size_t*)( ((char*)coa_block) + coa_block->size - sizeof(size_t) );
        *end_tag = coa_block->size;
        head = coa_block;
     //   fprintf(stderr, "\n----\nCASEIII\n");
    }
  //  fprintf(stderr, " after coa head->size: %zx ", head->size);
  //  fprintf(stderr, "coa end\n");
}

/* insertBlock helper function -- takes in argument and inserts it as the head of the free list */

void insertBlock(meta *in) {
//    fprintf(stderr, " insertBlock->size: %zx ", in->size);
    if(head){
	head->prev = in;
	in->next = head;
    }
    head = in;
    //fprintf(stderr, "head size: %zu\n", head->size);
    coalesce();
}

/**/
/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    size_t totalSize = num * size;
    void * ret = malloc(totalSize);
    memset(ret,0,totalSize);
    return ret;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */

void *malloc(size_t input_size) {
   // fprintf(stderr, "malloc start   ");
    //TODO: round input_size to a block size that can be accepted by the free list

    size_t alloc_size = ((input_size + 2*sizeof(size_t)) / MIN_BLOCK_SIZE) * MIN_BLOCK_SIZE + ( (input_size + 2*sizeof(size_t)) % MIN_BLOCK_SIZE != 0 ? MIN_BLOCK_SIZE : 0);

    /*TODO: search through the free list for a block that can accomodate the alloc_size
	    - use either first fit or worst fit ( probably first fit honestly)
    */

    meta * trav = head;
    //fprintf(stderr,"alloc_size: %zx ", alloc_size);
    /*TODO: if you find a block in the free list that can accomodate it, then you should
	    1) remove it from the free list entirely
		- if you can salvage some memory space from this block that we don't need (block_size - alloc_size) > 0
			- then remove it and place this at the head of the list ( insertBlock ) -- coalescing is optional ( you should probably do it)
	    else
		you should allocate some memory from the heap
    */
    int split_block = 0;
    void * malloc_block = NULL;
    while(trav){
        if(trav->size >= alloc_size) {

	    removeBlock(trav);
	    malloc_block = trav;

	    if(trav->size > alloc_size){
		trav->next = NULL;
	        trav->prev = NULL;
		trav->size = trav->size - alloc_size;
		size_t * endTag = (size_t*) ( ((char*)trav) + trav->size - sizeof(size_t) );
		*endTag = trav->size;
		//insertBlock(trav);
	        split_block = 1;
		malloc_block = (void*)( ((char*)malloc_block) + trav->size );
	    }
	    break;
	}
        trav = trav->next;
    }

    if(!malloc_block){
	if(!BEGIN_HEAP)
	    BEGIN_HEAP = sbrk(0);
        malloc_block = sbrk(alloc_size);
    }
    /*TODO: add the tags to the block you are choosing to return, the tags will just be the size at the beginning and end of this memory -- add +1 to indicate taken*/

    size_t * beg_tag = (size_t*)malloc_block;
    size_t * end_tag = (size_t*)( ((char*)malloc_block) + alloc_size - sizeof(size_t) );
    *beg_tag = alloc_size + 1;
    *end_tag = alloc_size + 1;
    if(split_block)
	insertBlock(trav);
    /*TODO: return a return_block that's sizeof(size_t) + the block's base address */
    //fprintf(stderr, " malloc end\n");
    return (void*)( ((char*)malloc_block) + sizeof(size_t) );
}




/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    //fprintf(stderr, "free start");
  //TODO: offset the ptr to return the base address of the block

    meta * free_block = (meta*)( ((char*)ptr) - sizeof(size_t) );

  /*TODO: you have to convert this previous malloced block into a free block
 	- this requires you to change the size from  its +1 state into its regular state
	- you have to set the metadata up.
	- ->next = NULL ->prev = NULL
  */

    free_block->next = NULL;
    free_block->prev = NULL;
    free_block->size = free_block->size - 1;
    size_t * end_tag = (size_t *)( ((char*)free_block) + free_block->size - sizeof(size_t) );
    *end_tag = free_block->size;
   // fprintf(stderr, "   free_block->size: %zx\n", free_block->size);

  /*TODO: insert the converted block into your free list*/
    insertBlock(free_block);
    //fprintf(stderr, " free end\n");
}


/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */

void *realloc(void *ptr, size_t size) {

    if(!ptr) {
	return malloc(size);
    }
    if(!size){
	free(ptr);
	return NULL;
    }
    //fprintf(stderr, "realloc begin  ");
    void * ret = malloc(size);
    size_t size_before = *(size_t*)( ((char*)ptr) - sizeof(size_t) );
    size_t size_reall = *(size_t*)( ((char*)ret) - sizeof(size_t));
    //fprintf(stderr, "size_before: %zx and size_reall: %zx ", size_before, size_reall);
    size_t cpy_size = size_before < size_reall ? size_before : size_reall;
    /* subtract 1 because both ahve the one raised to show alloc */
    cpy_size--;
    /* subtract 2 * sizeof(size_t) because you need to not overlap the tags */
    cpy_size -= 2 * sizeof(size_t);
   // fprintf(stderr," cpy_size: %zx", cpy_size);
    memcpy(ret,ptr,cpy_size);
    free(ptr);
    //fprintf(stderr, " realloc end\n");
    return ret;
}
