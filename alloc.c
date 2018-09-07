/**
 * Machine Problem: Malloc
 * CS 241 - Spring 2017
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>


// ##### print functions #####

void printPointer( void* p )
{
    char buffer[ 1024 ];
    int size = sprintf( buffer, "%p\n", p );
    write( 1, buffer, size );
}

void printInt( int i )
{
    char buffer[ 1024 ];
    int size = sprintf( buffer, "%d\n", i );
    write( 1, buffer, size );
}

void printSizet( size_t i )
{
    char buffer[ 1024 ];
    int size = sprintf( buffer, "%lu\n", i );
    write( 1, buffer, size );
}

void printString( char* str )
{
    write( 1, str, strlen( str ) );
}






// ##### structs #####

typedef struct metadata
{
    __uint32_t next;
    __uint32_t size;
    char data[];
} metadata;



// ##### constants ######

// the size each sbrk() call should allocate
static const size_t chunkSize = 1024;

// minimum block size
static const size_t minBlockSize = sizeof( metadata );





// ##### global vars #####

// free linked list
static metadata* freeHead = NULL;
static metadata* freeTail = NULL;

// start of unused space
static void* extraStart = NULL;

// end of unused space
static void* extraEnd = NULL;




// ##### preprocessor functions #####

#define min( a, b ) ( ( a < b ) ? a : b )

#define max( a, b ) ( ( a < b ) ? b : a )







// ##### helper functions #####

// allocates on the "stack"
void* allocate( size_t size );

// deallocates all data at, and after, data
void deallocate( void* data );



// returns the pointer to block's next block
metadata* getNext( metadata* block );

// sets the next value of block to link to next
void setNext( metadata* block, metadata* next );

// returns a pointer to the metadata of data
metadata* getBlock( void* data );
    metadata* prev;


// returns a pointer to the first block that is at or beyond block
// returns NULL if block is the last block or the list is empty
// if prev != NULL, *prev is set to the block before block
// *prev is set to NULL if block should be added to the end
// or the list is empty
metadata* findBlockByPointer( metadata** prev, metadata* block );

// searches list for the best fit block to contain a blockSize sized block
// returns a pointer to the best block and sets *prev to the block before it,
// if prev != NULL
metadata* findBlockBySize( metadata** prev, __uint32_t blockSize );

// inserts block into the free list. if prev != NULL, inserts block right
// after prev (do not call unless prev < block < prev->next)
void insertBlock( metadata* block, metadata* prev );

// removes the block after prev. if prev == NULL, removes the first block
// from the free list
void eraseBlock( metadata* prev );





// returns the appropriate bucket size to contain data of size == size
size_t getBlockSize( size_t size );

// returns true if a and b are adjacent and a < b
int adjacent( metadata* a, metadata* b );

// returns true if block is the last block
int lastBlock( metadata* block );

// merges a and b (assumes a < b and adjacent( a, b ))
void mergeBlocks( metadata* a, metadata* b );

// splits block into 2 smaller blocks, where block->size = newSize and
// the newly created block is inserted to the free list
// if block doesn't have enough space to be split, nothing happens
void splitBlock( metadata* block, __uint32_t newSize );

// tries to expand block with the block infront of it so that
// block->size >= newSize
int expandBlock( metadata* block, __uint32_t newSize );







void printBlock( metadata* block )
{
    char buffer[256];
    int size = sprintf( buffer, "%p 0x%x %p\n", block, block->size, getNext( block ) );
    write( 1, buffer, size );
}


void printFree()
{
    char buffer[ 256 ];
    int size;
    metadata* iter = freeHead;
    while ( iter != NULL )
    {
        printBlock( iter );
        iter = getNext( iter );
    }
    size = sprintf( buffer, "%p end\n\n", freeTail );
    write( 1, buffer, size );
}










void* allocate( size_t size )
{
    // set up markers
    if ( extraStart == NULL )
    {
        extraStart = sbrk( 0 );
        extraEnd = extraStart;
    }
    
    // return top of stack
    void* data = extraStart;

    // move start forward
    extraStart += size;

    // if overflowed stack, extend it
    if ( extraStart > extraEnd )
    {
        size_t diff = ( extraStart - extraEnd );
        if ( diff % chunkSize != 0 )
        {
            diff += chunkSize - diff % chunkSize;
        }
        sbrk( diff );
        extraEnd += diff;
    }

    return data;
}





void deallocate( void* data )
{
    extraStart = data;
}





metadata* getNext( metadata* block )
{
    if ( block->next == 0 )
    {
        return NULL;
    }
    else
    {
        return block + block->next;
    }
}





void setNext( metadata* block, metadata* next )
{
    if ( next == NULL )
    {
        block->next = 0;
    }
    else
    {
        block->next = next - block;
    }
}





metadata* getBlock( void* data )
{
    return ( (metadata*)data ) - 1;
}





metadata* findBlockByPointer( metadata** prev, metadata* block )
{
    metadata* last = NULL;
    metadata* iter = freeHead;

    // if block should be at end (or list is empty)
    if ( block > freeTail )
    {
        last = freeTail;
        iter = NULL;
    }
    // if block is last block
    else if ( block == freeTail && prev == NULL )
    {
        return freeTail;
    }
    // otherwise
    else
    {
        // iterate until location is found
        while ( iter < block )
        {
            last = iter;
            iter = getNext( iter );
        }
    }

    if ( prev != NULL )
    {
        *prev = last;
    }
    return iter;
}





metadata* findBlockBySize( metadata** prev, __uint32_t blockSize )
{
    metadata* last = NULL;
    metadata* iter = freeHead;
    metadata* beforeBest = NULL;
    metadata* best = NULL;
    __uint32_t bestSize = UINT32_MAX;

    // while end wasn't reached
    while ( iter != NULL )
    {
        // if position is valid
        if ( iter->size >= blockSize && iter->size < bestSize )
        {
            // set best
            beforeBest = last;
            best = iter;
            bestSize = iter->size;

            // break if best is found
            if ( bestSize == blockSize )
            {
                break;
            }
        }
        
        // move forward
        last = iter;
        iter = getNext( iter );
    }

    // set prev
    if ( prev != NULL )
    {
        *prev = beforeBest;
    }

    return best;
}





void insertBlock( metadata* block, metadata* prev )
{
    // if block is last block
    if ( lastBlock( block ) )
    {
        // deallocate block
        deallocate( block );

        // test if free list tail is the last block (and if tail exists)
        if ( freeTail != NULL && lastBlock( freeTail ) )
        {
            // remove that one too
            metadata* tail = freeTail;
            findBlockByPointer( &prev, freeTail );
            eraseBlock( prev );
            deallocate( tail );
        }
    }


    // if the list is empty
    else if ( freeHead == NULL )
    {
        freeHead = block;
        freeTail = block;
        setNext( block, NULL );
    }


    // if block should be the head
    else if ( block < freeHead )
    {
        // try to merge blocks
        if ( adjacent( block, freeHead ) )
        {
            mergeBlocks( block, freeHead );
            // set tail
            if ( freeHead == freeTail )
            {
                setNext( block, NULL );
                freeTail = block;
            }
        }
        // link blocks instead
        else
        {
            setNext( block, freeHead );
        }
        // set head
        freeHead = block;
    }


    // if block should be the tail
    else if ( block > freeTail )
    {
        // try to merge blocks
        if ( adjacent( freeTail, block ) )
        {
            mergeBlocks( freeTail, block );
        }
        // link blocks instead
        else
        {
            setNext( freeTail, block );
            setNext( block, NULL );
            freeTail = block;
        }
    }


    // block should be somewhere else
    else
    {
        metadata* after;
        // if no hint was given
        if ( prev == NULL )
        {
            after = findBlockByPointer( &prev, block );
        }
        // if a hint was given
        else
        {
            after = getNext( prev );
        }
        
        // try to merge with after
        if ( adjacent( block, after ) )
        {
            mergeBlocks( block, after );
            // fix tail
            if ( after == freeTail )
            {
                setNext( block, NULL );
                freeTail = block;
            }
        }
        // link with after
        else
        {
            setNext( block, after );
        }

        // try to merge with prev
        if ( adjacent( prev, block ) )
        {
            mergeBlocks( prev, block );
            // fix tail
            if ( block == freeTail )
            {
                setNext( prev, NULL );
                freeTail = prev;
            }
        }
        // link with prev
        else
        {
            setNext( prev, block );
        }
    }
}





void eraseBlock( metadata* prev )
{
    // if there's only one block to erase
    if ( freeHead == freeTail )
    {
        freeHead = NULL;
        freeTail = NULL;
    }

    // if deleting head
    else if ( prev == NULL )
    {
        freeHead = getNext( freeHead );
    }

    // if deleting tail
    else if ( getNext( prev ) == freeTail )
    {
        freeTail = prev;
        setNext( prev, NULL );
    }

    // delete other node
    else
    {
        setNext( prev, getNext( getNext( prev ) ) );
    }
}





size_t getBlockSize( size_t size )
{
    if ( size % minBlockSize != 0 )
    {
        size += minBlockSize - size % minBlockSize;
    }
    return size;
}





int adjacent( metadata* a, metadata* b )
{
    return (void*)a + sizeof( metadata ) + a->size == b;
}





int lastBlock( metadata* block )
{
    return adjacent( block, extraStart );
}





void mergeBlocks( metadata* a, metadata* b )
{
    // link a to b's next
    setNext( a, getNext( b ) );
    // increase a's size to include b's data and metadata
    a->size += sizeof( metadata ) + b->size;
}





void splitBlock( metadata* block, __uint32_t newSize )
{
    __uint32_t oldSize = block->size;

    // test if block is big enough
    if ( newSize + sizeof( metadata ) + minBlockSize > oldSize )
    {
        return;
    }

    // fix block
    block->size = newSize;
   
    // create new block
    metadata* newBlock = (metadata*)( (char*)block + sizeof( metadata ) + newSize );
    newBlock->size = oldSize - newSize - sizeof( metadata );
    insertBlock( newBlock, NULL );
}





int expandBlock( metadata* block, __uint32_t newSize )
{
    // check if block is last block
    if ( lastBlock( block ) )
    {
        // add memory to "stack"
        allocate( newSize - block->size );
        block->size = newSize;
    }
    else
    {
        // get next block in memory
        metadata* next = (metadata*)( (char*)block + sizeof( metadata ) + block->size );
        metadata* prev = NULL;
        // check if block is large enough and free
        if ( block->size + sizeof( metadata ) + next->size < newSize ||
             next != findBlockByPointer( &prev, next ) )
        {
            return 0;
        }

        // remove next from free list
        eraseBlock( prev );
        
        // merge blocks
        mergeBlocks( block, next );

        // remove slack
        splitBlock( block, newSize );
    }
    return 1;
}









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
    void* data = malloc( num * size );
    size_t blockSize = getBlockSize( num * size );
    memset( data, 0, blockSize );
    return data;
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
void *malloc(size_t size)
{
    // get block info
    size_t blockSize = getBlockSize( size );

    // search for free block
    metadata* prev;
    metadata* block = findBlockBySize( &prev, blockSize );

    // if block was found, remove it from free list
    if ( block != NULL )
    {
        eraseBlock( prev );
    }
    // if no block is found, create a new one
    else
    {
        block = allocate( sizeof( metadata ) + blockSize );
        block->size = blockSize;
    }

    setNext( block, NULL );

    return block->data;
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
void free( void *ptr )
{
    // insert block to free list
    metadata* block = getBlock( ptr );
    insertBlock( block, NULL );
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
void *realloc( void *ptr, size_t size )
{
    if ( size == 0 )
    {
        return NULL;
    }
    else if ( ptr == NULL )
    {
        return malloc( size );
    }

    metadata* block = getBlock( ptr );
    size_t newSize = getBlockSize( size );

    // if block already has enough space, return ptr
    if ( newSize <= block->size )
    {
        // leave extra space to free list
        splitBlock( block, newSize );
        return ptr;
    }

    // try to expand block
    if ( expandBlock( block, newSize ) )
    {
        return ptr;
    }
    
    // I give up, just find a new block
    void* newData = malloc( size );
    memcpy( newData, ptr, min( size, block->size ) );
    free( ptr );
    return newData;
}
