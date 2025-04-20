#pragma once

#include <windows.h> // For Windows API functions and types
#include "types.h"
#include "memory_pool.h"

#include "tracy\public\tracy\Tracy.hpp"
#include "tracy\public\tracy\TracyOpenGL.hpp"
namespace thread_pool
{
    // Function signature for the thread function
    typedef void (*thread_work_func)(void *data, u32 thread_id, mpool::memory_pool *thread_memory);

    struct work_data
    {
        thread_work_func func; // Function to be executed by the thread
        void *data;            // Data to be passed to the function
        u32 priority;          // Priority of the work item (higher values = higher priority)
    };

    // Lock-free work queue - much more efficient than critical section
    struct work_queue
    {
        volatile LONG head; // Head index for producers (adding work)
        volatile LONG tail; // Tail index for consumers (getting work)
        volatile u32 size;  // Size of the queue
        volatile u32 mask;  // Bit mask for wrapping indices
        work_data *items;   // Array of work items

        // Statistics
        volatile LONG items_processed; // Total items processed
        volatile LONG items_added;     // Total items added
    };

    struct thread_pool
    {
        HANDLE *threads;                             // Array of thread handles
        mpool::memory_pool *thread_transient_memory; // Memory pool for thread data
        work_queue queue;                            // Lock-free work queue
        u32 num_threads;                             // Number of threads in the pool
        u32 max_threads;                             // Maximum number of threads allowed in the pool
        volatile u32 shutdown;                       // Flag to indicate if the pool is shutting down

        // Synchronization primitives
        HANDLE workCompleteEvent;     // Event signaled when all work is complete
        HANDLE workAvailableEvent;    // Event signaled when work is available
        volatile LONG spinlock;       // Simple atomic spinlock for rare contention cases
        volatile LONG active_threads; // Count of actively working threads
    };

    thread_pool *g_thread_pool = nullptr;

    // Fast spinlock implementation for infrequent contention cases
    static void acquire_spinlock(volatile LONG *lock)
    {
        while (InterlockedExchange(lock, 1) != 0)
        {
            // Use pause instruction to improve spinning efficiency
            YieldProcessor();
        }
    }

    static void release_spinlock(volatile LONG *lock)
    {
        InterlockedExchange(lock, 0);
    }

    static bool queue_try_add(work_queue *q, thread_work_func func, void *data, u32 priority = 0)
    {
        // Use InterlockedIncrement to atomically get the next head position
        LONG index = InterlockedIncrement(&q->head) - 1;
        u32 slot = index & q->mask;

        // Set up the work item
        q->items[slot].func = func;
        q->items[slot].data = data;
        q->items[slot].priority = priority;

        // Track statistics
        InterlockedIncrement(&q->items_added);

        // Signal that work is available
        SetEvent(g_thread_pool->workAvailableEvent);

        return true;
    }
    // Try to get work using atomic operations
    static work_data *queue_try_get(work_queue *q)
    {
        // Check if any work is available (compare head and tail)
        if (q->tail >= q->head)
        {
            return nullptr; // No work available
        }

        // Use InterlockedIncrement to atomically claim the next work item
        LONG index = InterlockedIncrement(&q->tail) - 1;

        // Verify we didn't race past the head
        if (index >= q->head)
        {
            InterlockedDecrement(&q->tail); // Undo our increment
            return nullptr;
        }

        u32 slot = index & q->mask;

        // Track statistics
        InterlockedIncrement(&q->items_processed);

        return &q->items[slot];
    }

    // Get work from the queue
    static work_data *get_work_data()
    {
        work_data *item = queue_try_get(&g_thread_pool->queue);
        if (item)
        {
            // Mark thread as active when it gets work
            return item;
        }

        return nullptr; // No work available
    }

    // Check if any work remains - optimized to avoid locking
    static u32 work_remaining()
    {
        ZoneScoped;
        return g_thread_pool->queue.tail < g_thread_pool->queue.head;
    }

    // Thread function that continuously checks for work and executes it

    static void try_wait(u32 *spin_count, u32 threshold)
    {
        // Adaptive waiting strategy based on how long we've been idle
        if (++(*spin_count) < threshold)
        {
            // Short spin for immediate new work - maximizes throughput
            for (int i = 0; i < 10; i++)
            {
                YieldProcessor();
            }
        }
        else if (*spin_count < threshold * 10)
        {
            // Medium wait: yield to other threads but stay ready
            SwitchToThread(); // More efficient than Sleep(0) on modern Windows
        }
        else
        {
            // Long wait: wait on event with timeout
            // If no work is available, reset the event before waiting
            if (!work_remaining() && g_thread_pool->active_threads == 0)
            {
                // No pending work, reset the event
                ResetEvent(g_thread_pool->workAvailableEvent);
            }

            DWORD waitResult = WaitForSingleObject(
                g_thread_pool->workAvailableEvent,
                1); // Short timeout to check shutdown flag

            // Reset spin count after event wait
            if (waitResult == WAIT_OBJECT_0)
            {
                *spin_count = 0;
            }
        }
    }

    static DWORD WINAPI thread_function(LPVOID param)
    {
        ZoneScoped;
        u32 thread_id = (u32)param; // Get the thread ID from the parameter

        // Thread-local variables for efficiency
        u32 spin_count = 0;
        const u32 SPIN_THRESHOLD = 1000; // How many spins before yielding

        while (!g_thread_pool->shutdown)
        {
            // Try to get work from the queue
            work_data *curr = get_work_data();

            if (curr)
            {
                // Reset spin count when we get work
                spin_count = 0;

                InterlockedIncrement(&g_thread_pool->active_threads);
                // Execute the task with thread-local memory
                mpool::reset(&g_thread_pool->thread_transient_memory[thread_id]);
                curr->func(curr->data, thread_id, &g_thread_pool->thread_transient_memory[thread_id]);

                // Decrement active threads counter when work is complete
                if (InterlockedDecrement(&g_thread_pool->active_threads) == 0 && !work_remaining())
                {
                    // Signal completion when the last thread finishes work and no work remains
                    SetEvent(g_thread_pool->workCompleteEvent);
                }
            }
            else
            {
#if 0 
                // Adaptive waiting strategy based on how long we've been idle
                if (++spin_count < SPIN_THRESHOLD)
                {
                    // Short spin for immediate new work - maximizes throughput
                    for (int i = 0; i < 10; i++)
                    {
                        YieldProcessor();
                    }
                }
                else if (spin_count < SPIN_THRESHOLD * 10)
                {
                    // Medium wait: yield to other threads but stay ready
                    SwitchToThread(); // More efficient than Sleep(0) on modern Windows
                }
                else
                {
                    // Long wait: wait on event with timeout
                    DWORD waitResult = WaitForSingleObject(
                        g_thread_pool->workAvailableEvent,
                        1); // Short timeout to check shutdown flag

                    // Reset spin count after event wait
                    if (waitResult == WAIT_OBJECT_0)
                    {
                        spin_count = 0;
                    }
                }
#else
                try_wait(&spin_count, SPIN_THRESHOLD); // Call to try_wait function for adaptive waiting
#endif
            }
        }
        return 0;
    }

    static u32 start_thread_pool(u32 num_threads, u32 max_work_orders)
    {
        ZoneScoped;
        g_thread_pool = (thread_pool *)malloc(sizeof(thread_pool));
        if (!g_thread_pool)
        {
            // Handle allocation failure
            return -1;
        }

        // Initialize the pool structure
        g_thread_pool->num_threads = num_threads;
        g_thread_pool->max_threads = num_threads;
        g_thread_pool->shutdown = 0;
        g_thread_pool->spinlock = 0;
        g_thread_pool->active_threads = 0;

        // Create synchronization events
        g_thread_pool->workCompleteEvent = CreateEvent(
            NULL, // Default security attributes
            TRUE, // Manual reset - stays signaled until reset
            TRUE, // Initially signaled (no work pending)
            NULL  // No name
        );

        g_thread_pool->workAvailableEvent = CreateEvent(
            NULL,  // Default security attributes
            TRUE,  // Manual reset - stays signaled until reset
            FALSE, // Initially not signaled (no work available)
            NULL   // No name
        );

        if (!g_thread_pool->workCompleteEvent || !g_thread_pool->workAvailableEvent)
        {
            // Handle event creation failure
            if (g_thread_pool->workCompleteEvent)
                CloseHandle(g_thread_pool->workCompleteEvent);
            if (g_thread_pool->workAvailableEvent)
                CloseHandle(g_thread_pool->workAvailableEvent);
            free(g_thread_pool);
            return -1;
        }

        // Allocate memory for thread handles
        g_thread_pool->threads = (HANDLE *)malloc(sizeof(HANDLE) * num_threads);

        // Initialize lock-free queue (use power of 2 size for efficient masking)
        u32 queue_size = 1;
        while (queue_size < max_work_orders * 2)
        {
            queue_size *= 2;
        }

        g_thread_pool->queue.size = queue_size;
        g_thread_pool->queue.mask = queue_size - 1;
        g_thread_pool->queue.head = 0;
        g_thread_pool->queue.tail = 0;
        g_thread_pool->queue.items_processed = 0;
        g_thread_pool->queue.items_added = 0;
        g_thread_pool->queue.items = (work_data *)malloc(sizeof(work_data) * queue_size);

        if (!g_thread_pool->threads || !g_thread_pool->queue.items)
        {
            // Handle memory allocation failure
            if (g_thread_pool->threads)
                free(g_thread_pool->threads);
            if (g_thread_pool->queue.items)
                free(g_thread_pool->queue.items);
            CloseHandle(g_thread_pool->workCompleteEvent);
            CloseHandle(g_thread_pool->workAvailableEvent);
            free(g_thread_pool);
            return -1; // Return -1 to indicate failure
        }

        g_thread_pool->thread_transient_memory = (mpool::memory_pool *)malloc(sizeof(mpool::memory_pool) * num_threads);

        // Create and start worker threads
        for (u32 i = 0; i < num_threads; i++)
        {
            g_thread_pool->thread_transient_memory[i] = mpool::allocate(MEGABYTES(1)); // Allocate memory pool for each thread

            // Create the thread with the thread function
            g_thread_pool->threads[i] = CreateThread(
                NULL,            // Default security attributes
                0,               // Default stack size
                thread_function, // Thread function
                (LPVOID)i,       // Parameter to thread function
                0,               // Run immediately
                NULL             // Thread identifier (not needed)
            );

            if (!g_thread_pool->threads[i])
            {
                // Handle thread creation failure
                g_thread_pool->num_threads = i; // Set actual number of threads created
                // Could add cleanup here if needed for partial failure
            }
        }

        return 0;
    }

    // Add work to the lock-free queue
    static u32 add_work(thread_work_func func, void *data)
    {
        ZoneScoped;
        if (queue_try_add(&g_thread_pool->queue, func, data))
        {

            // Reset the workCompleteEvent since we have new work
            ResetEvent(g_thread_pool->workCompleteEvent);
            return 0;
        }
        assert(0 && "Failed to add work to thread pool"); // This should never happen
        return -1;                                        // Couldn't add work
    }

    // Add prioritized work - higher priority work will be processed first
    static u32 add_prioritized_work(thread_work_func func, void *data, u32 priority)
    {
        ZoneScoped;
        if (queue_try_add(&g_thread_pool->queue, func, data, priority))
        {

            // Reset the workCompleteEvent since we have new work
            ResetEvent(g_thread_pool->workCompleteEvent);
            return 0;
        }

        return -1; // Couldn't add work
    }

    static u32 reset_work()
    {
        ZoneScoped;
        // Reset the lock-free queue
        acquire_spinlock(&g_thread_pool->spinlock);
        g_thread_pool->queue.head = 0;
        g_thread_pool->queue.tail = 0;
        g_thread_pool->queue.items_processed = 0;
        g_thread_pool->queue.items_added = 0;
        g_thread_pool->active_threads = 0;
        release_spinlock(&g_thread_pool->spinlock);

        // Reset events to appropriate state
        SetEvent(g_thread_pool->workCompleteEvent);    // No work to do
        ResetEvent(g_thread_pool->workAvailableEvent); // No work available

        return 0; // Reset successful
    }

    // Execute the next work item (for main thread participation)
    static bool execute_next_work_item()
    {
        work_data *work = get_work_data();
        if (work)
        {
            // We have work to do in the main thread
            static mpool::memory_pool main_thread_memory = mpool::allocate(MEGABYTES(1));
            mpool::reset(&main_thread_memory);
            work->func(work->data, 0xFFFFFFFF, &main_thread_memory); // Special ID for main thread
            return true;
        }
        return false;
    }

    // Wait efficiently for all work to complete with active participation
    static void wait_for_completion(DWORD timeout_ms = 500)
    {
        ZoneScoped;
        // First check if we're already done without waiting
        if (!work_remaining() && g_thread_pool->active_threads == 0)
        {
            return;
        }

        // Time-based approach to balance responsiveness and CPU usage
        DWORD start_time = GetTickCount();

        while (true)
        {
            // Try to do some work in the calling thread to help process the queue
            if (!execute_next_work_item())
            {
                // Check if all work is complete
                if (!work_remaining() && g_thread_pool->active_threads == 0)
                {
                    break;
                }

                // No work available right now, use adaptive waiting strategy
                DWORD elapsed = GetTickCount() - start_time;

                if (elapsed < 10)
                {
                    // Aggressive spinning for first 10ms
                    YieldProcessor();
                }
                else if (elapsed < 50)
                {
                    // Yield to other threads but stay ready
                    SwitchToThread();
                }
                else
                {
                    // Wait on workCompleteEvent
                    DWORD wait_result = WaitForSingleObject(g_thread_pool->workCompleteEvent, 1);
                    if (wait_result == WAIT_OBJECT_0)
                    {
                        // Work is complete, exit loop
                        break;
                    }

                    // Check timeout condition
                    if (timeout_ms != INFINITE && elapsed > timeout_ms)
                    {
                        break;
                    }
                }
            }
        }
    }

    // Clean shutdown of thread pool
    static void shutdown_thread_pool()
    {
        if (!g_thread_pool)
            return;

        // Signal shutdown
        g_thread_pool->shutdown = 1;

        // Wake up all waiting threads
        SetEvent(g_thread_pool->workAvailableEvent);

        // Wait for all threads to exit (with timeout)
        WaitForMultipleObjects(g_thread_pool->num_threads, g_thread_pool->threads, TRUE, 1000);

        // Close thread handles
        for (u32 i = 0; i < g_thread_pool->num_threads; i++)
        {
            CloseHandle(g_thread_pool->threads[i]);
            mpool::deallocate(&g_thread_pool->thread_transient_memory[i]);
        }

        // Clean up resources
        free(g_thread_pool->threads);
        free(g_thread_pool->queue.items);
        free(g_thread_pool->thread_transient_memory);
        CloseHandle(g_thread_pool->workCompleteEvent);
        CloseHandle(g_thread_pool->workAvailableEvent);

        free(g_thread_pool);
        g_thread_pool = nullptr;
    }
}