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
        
#ifdef __APPLE__
        // On macOS, use aligned_alloc (available in C11 and later)
        // aligned_alloc requires size to be a multiple of alignment
        size_bytes = (size_bytes + 31) & ~31; // Round up to multiple of 32
        pool.memory = aligned_alloc(32, size_bytes);
#elif defined(_WIN32) || defined(_WIN64)
        // On Windows, use _aligned_malloc
        pool.memory = _aligned_malloc(size_bytes, 32);
#else
        // For other platforms, use posix_memalign
        void* mem = NULL;
        if (posix_memalign(&mem, 32, size_bytes) == 0) {
            pool.memory = mem;
        } else {
            pool.memory = NULL;
        }
#endif

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
        if (bytes_to_get % 32 != 0)
        {
            // Ensure the requested size is aligned to 32 bytes
            bytes_to_get += 32 - (bytes_to_get % 32);
        }
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
#ifdef _WIN32
            _aligned_free(pool->memory); // Windows-specific aligned free
#else
            free(pool->memory); // Standard free works for aligned_alloc and posix_memalign
#endif
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