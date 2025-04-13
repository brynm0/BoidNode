#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "types.h"

namespace mpool
{
    // Structure to represent a memory pool
    typedef struct
    {
        void *memory; // Pointer to the allocated memory block
        u32 size;     // Total size of the memory pool in bytes
        u32 offset;   // Current offset in the memory pool
    } memory_pool;

    // Function to allocate a memory pool
    memory_pool allocate(u32 size_bytes)
    {
        memory_pool pool;
        pool.memory = malloc(size_bytes); // Allocate memory
        if (!pool.memory)
        {
            // Handle allocation failure
            pool.size = 0;
            pool.offset = 0;
        }
        else
        {
            pool.size = size_bytes;
            pool.offset = 0;
        }
        return pool;
    }

    // Function to get a portion of memory from the pool
    void *get_bytes(memory_pool *pool, u32 bytes_to_get)
    {
        if (!pool || !pool->memory || pool->offset + bytes_to_get > pool->size)
        {
            // Return NULL if the request cannot be fulfilled
            return NULL;
        }

        // Get a pointer to the requested memory
        void *ptr = (char *)pool->memory + pool->offset;

        // Update the offset
        pool->offset += bytes_to_get;

        return ptr;
    }

    // Function to free the memory pool
    void deallocate(memory_pool *pool)
    {
        if (pool && pool->memory)
        {
            free(pool->memory); // Free the allocated memory
            pool->memory = NULL;
            pool->size = 0;
            pool->offset = 0;
        }
    }

    void reset(memory_pool *pool)
    {
        if (pool && pool->memory)
        {
            pool->offset = 0; // Reset the offset to reuse the memory
        }
    }
}
#endif // MEMORY_POOL_H