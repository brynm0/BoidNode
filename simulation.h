#pragma once
#include "types.h"
#include "math_linear.h"
#include "malloc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "spatial_hash.h"
#include "boid_thread.h"
#include "tracy\public\tracy\Tracy.hpp"

namespace simulation
{

    enum SIM_COMPONENT_TYPES
    {
        SIM_COMPONENT_SPATIAL = 1 << 0,
        SIM_COMPONENT_BOID = 1 << 1,
        SIM_COMPONENT_PLANE = 1 << 2,
    };

    enum BOID_TYPES
    {
        BOID_TYPE_SEEK = 1 << 0,
        BOID_TYPE_FLEE = 1 << 1,
        BOID_TYPE_ALIGN = 1 << 2,
        BOID_TYPE_COPLANAR = 1 << 3,
    };

    static float g_max_vel = 0.5f;
    static float g_min_vel = 0.15f;
    static float g_max_acc = 0.25f;

    static float g_cell_size = .25f;

    struct sim_data
    {
        // Simulation data structure
        // Add your simulation data members here
        float time_step;    // Time step for the simulation, not used for the moment. TODO: implement fixed time step update
        float current_time; // Current time in the simulation
        int num_iterations; // Number of iterations to run in the simulation
        // Add more members as needed

        u64 num_entities;
        u64 *components; // Array of entity IDs
        u64 *behaviours;
        vec4 *positions;  // Array of entity positions
        vec3 *velocities; // Array of entity velocities

        spatial_hash::spatial_hash search_hash;
        // void *search_memory_pool;
    };
    static inline void
    distribute_boids_random(float extents, sim_data *data)
    {

        for (u32 i = 0; i < data->num_entities; ++i)
        {
            // Assign components to the entity
            data->components[i] = SIM_COMPONENT_SPATIAL | SIM_COMPONENT_BOID;

            // Generate random positions within the extents
            data->positions[i].x = ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->positions[i].y = ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->positions[i].z = ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->positions[i].w = 1.0f;                                             // ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->behaviours[i] = BOID_TYPE_SEEK | BOID_TYPE_FLEE | BOID_TYPE_ALIGN; // Assign behaviours to the entity
            // Initialize velocities to zero
            data->velocities[i] = {.01f, 0, 0};
            // data->velocities[i] =
            //     {2.0f * ((float)rand() / RAND_MAX) - 1.0f,
            //      2.0f * ((float)rand() / RAND_MAX) - 1.0f,
            //      2.0f * ((float)rand() / RAND_MAX) - 1.0f};
            // data->velocities[i] = v3::normalize(data->velocities[i]) * g_min_vel; // Normalize and scale to min velocity
        }
    }

    sim_data init_sim(u64 num_entities, float radius)
    {
#if 0
        bool test_result = spatial_hash::test(); // Test the spatial hash function
        if (!test_result)
        {
            int *tmp = nullptr;
            *tmp = 1;
        }
#endif
        sim_data data = {};
        data.time_step = 0.016f; // 60 FPS
        data.current_time = 0.0f;
        data.num_iterations = 0;
        data.num_entities = num_entities;
        data.components = (u64 *)malloc(sizeof(u64) * num_entities);
        data.behaviours = (u64 *)malloc(sizeof(u64) * num_entities);
        data.positions = (vec4 *)malloc(sizeof(vec4) * num_entities);
        data.velocities = (vec3 *)malloc(sizeof(vec3) * num_entities);
        // data.search_memory_pool = malloc(sizeof(vec3) * num_entities); // Allocate memory for the search pool
        memset(data.velocities, 0, sizeof(vec3) * num_entities); // Initialize velocities to zero
        memset(data.components, 0, sizeof(u64) * num_entities);  // Initialize components to zero
        memset(data.behaviours, 0, sizeof(u64) * num_entities);  // Initialize behaviours to zero
        memset(data.positions, 0, sizeof(vec4) * num_entities);  // Initialize positions to zero

        distribute_boids_random(radius, &data);                                           // Distribute boids randomly in the simulation space
        spatial_hash::init(&data.search_hash, g_cell_size, num_entities, data.positions); // Initialize the spatial hash with the positions

        return data;
    }

    void free_sim(sim_data *data)
    {
        free(data->components);
        free(data->behaviours);
        free(data->positions);
        data->components = NULL;
        data->behaviours = NULL;
        data->positions = NULL;
    }

    static inline void boid_process_neighbors(
        u64 entity_id,
        const sim_data *data,
        u32 num_neighbours,
        const u32 *neighbour_ids,
        float seek_radius,
        float flee_radius,
        float align_radius,
        vec3 *seek_result,
        vec3 *flee_result,
        vec3 *align_result)
    {
        ZoneScoped;
        // Pre-fetch current boid data to avoid repeated memory access
        const vec3 current_position = data->positions[entity_id].xyz;

        // Initialize counters and result vectors
        u32 num_seek_neighbours = 0;
        u32 num_flee_neighbours = 0;
        u32 num_align_neighbours = 0;

        vec3 seek_acc = {0.0f, 0.0f, 0.0f};
        vec3 flee_acc = {0.0f, 0.0f, 0.0f};
        vec3 align_acc = {0.0f, 0.0f, 0.0f};

        // Calculate squared radii once to avoid repeated multiplications
        const float seek_radius_sq = seek_radius * seek_radius;
        const float flee_radius_sq = flee_radius * flee_radius;
        const float align_radius_sq = align_radius * align_radius;

        // Process all neighbors in a single pass for better cache usage
        for (u32 i = 0; i < num_neighbours; i++)
        {
            // Prefetch next neighbor data to reduce cache misses
            const u32 neighbor_idx = neighbour_ids[i];

            // Skip self-comparison
            if (neighbor_idx == entity_id)
                continue;

            const vec3 neighbour_position = data->positions[neighbor_idx].xyz;
            const vec3 difference = neighbour_position - current_position;
            const float distance_squared = v3::dot(difference, difference);

            // Calculate seek behavior (cohesion)
            if (distance_squared > 0 && distance_squared < seek_radius_sq)
            {
                seek_acc = seek_acc + difference;
                num_seek_neighbours++;
            }

            // Calculate flee behavior (separation) - only if within flee radius
            if (distance_squared > 0 && distance_squared < flee_radius_sq)
            {
                // Use reciprocal of distance for weighted flee (closer boids have stronger repulsion)
                const float weight = flee_radius_sq / (distance_squared + 0.0001f); // Avoid division by zero
                flee_acc = flee_acc + (difference * weight);
                num_flee_neighbours++;
            }

            // Calculate align behavior
            if (distance_squared > 0 && distance_squared < align_radius_sq)
            {
                const vec3 neighbour_velocity = data->velocities[neighbor_idx];
                align_acc = align_acc + neighbour_velocity;
                num_align_neighbours++;
            }
        }

        // Finalize results with safe division
        if (num_seek_neighbours > 0)
        {
            const float inv_count = 1.0f / (float)num_seek_neighbours;
            seek_acc.x *= inv_count;
            seek_acc.y *= inv_count;
            seek_acc.z *= inv_count;
            *seek_result = seek_acc;
        }

        if (num_flee_neighbours > 0)
        {
            const float inv_count = -1.0f / (float)num_flee_neighbours; // Negative because it's flee
            flee_acc.x *= inv_count;
            flee_acc.y *= inv_count;
            flee_acc.z *= inv_count;
            *flee_result = flee_acc;
        }

        if (num_align_neighbours > 0)
        {
            const float inv_count = 1.0f / (float)num_align_neighbours;
            align_acc.x *= inv_count;
            align_acc.y *= inv_count;
            align_acc.z *= inv_count;
            *align_result = align_acc;
        }
    }

    void update_sim_block(simulation::sim_data *data, float delta_time, u32 start_id, u32 end_id, mpool::memory_pool *transient_memory)
    {
        ZoneScoped;

        // Pre-compute constants to avoid redundant calculations
        const float seek_radius = 0.25f;
        const float flee_radius = 0.15f;
        const float align_radius = 0.25f;
        const float min_vel_sq = g_min_vel * g_min_vel;
        u32 *search_indices_start = (u32 *)mpool::get_bytes(transient_memory, sizeof(u32) * data->num_entities); // Allocate memory for search indices

        // First pass: Calculate all forces and update velocities
        // This improves cache locality by processing all entities before updating positions
        for (u32 i = start_id; i < end_id; ++i)
        {
            const u64 entity_components = data->components[i];

            // Skip entities without required components
            if (!(entity_components & SIM_COMPONENT_SPATIAL))
                continue;

            const u64 entity_behaviours = data->behaviours[i];

            // Skip processing if no behaviors are active
            const u64 behavior_mask = BOID_TYPE_SEEK | BOID_TYPE_FLEE | BOID_TYPE_ALIGN;
            if (!(entity_behaviours & behavior_mask))
                continue;

            // Get spatial hash neighbors only once for all behaviors
            u32 search_count = 0;
            // u32 *search_indices = (u32 *)data->search_memory_pool;
            // Prefetch entity position data
            const vec4 current_position = data->positions[i];
            u32 *search_indices = search_indices_start;
            spatial_hash::search(&data->search_hash, current_position, seek_radius, search_indices, &search_count);

            // Temporary storage for behavior results
            vec3 seek_result = {0.0f, 0.0f, 0.0f};
            vec3 flee_result = {0.0f, 0.0f, 0.0f};
            vec3 align_result = {0.0f, 0.0f, 0.0f};

            // Process all neighbors in a single pass if any behavior is active
            if (entity_behaviours & behavior_mask)
            {
                boid_process_neighbors(
                    i,
                    data,
                    search_count,
                    search_indices,
                    seek_radius,
                    flee_radius,
                    align_radius,
                    &seek_result,
                    &flee_result,
                    &align_result);
            }

            // Calculate final acceleration based on active behaviors
            vec3 acceleration = {0.0f, 0.0f, 0.0f};

            if (entity_behaviours & BOID_TYPE_SEEK)
            {
                acceleration = acceleration + seek_result;
                vec3 seek_origin = seek_pos - current_position.xyz;
            }

            if (entity_behaviours & BOID_TYPE_FLEE)
            {
                acceleration = acceleration + flee_result; // Already negated in the process function
            }

            if (entity_behaviours & BOID_TYPE_ALIGN)
            {
                acceleration = acceleration + align_result;
            }

            // Apply acceleration limits and update velocity
            acceleration = v3::clamp(acceleration, g_max_acc); // Clamp acceleration to max value

            // Update velocity with acceleration
            data->velocities[i] = data->velocities[i] + acceleration * delta_time;
            data->velocities[i] = v3::clamp(data->velocities[i], g_max_vel); // Clamp velocity to max value

            // Ensure minimum velocity
            if (v3::sq_mag(data->velocities[i]) < min_vel_sq)
            {
                data->velocities[i] = v3::normalize(data->velocities[i]) * g_min_vel;
            }
        }

        // Second pass: Update positions
        // This gives better cache coherence by separating reads and writes
        for (u32 i = start_id; i < end_id; ++i)
        {
            // Update position based on velocity
            data->positions[i].xyz = data->positions[i].xyz + data->velocities[i] * delta_time;
        }
    }

    struct sim_thread_data
    {
        // Thread data structure for parallel processing
        sim_data *data;  // Pointer to the simulation data
        u32 start_index; // Start index for the thread's work
        u32 end_index;   // End index for the thread's work
    };

    void sim_work_func(void *data, u32 id, mpool::memory_pool *transient_memory)
    {

        ZoneScoped;
        sim_thread_data *thread_data = (sim_thread_data *)data;
        update_sim_block(
            thread_data->data,
            thread_data->data->time_step,
            thread_data->start_index,
            thread_data->end_index,
            transient_memory);
    }

    void update_sim(sim_data *data, float delta_time)
    {
        ZoneScoped;
        // Update simulation logic here
        data->current_time += delta_time;
        data->num_iterations++;
        static mpool::memory_pool thread_data = mpool::allocate(MEGABYTES(1)); // Allocate memory pool for transient data
        mpool::reset(&thread_data);                                            // Reset the memory pool for thread data

        // More fine-grained work distribution for better thread utilization
        const u32 hw_threads = thread_pool::g_thread_pool->num_threads;

        // Use a much smaller work unit size to reduce thread waiting time and improve load balancing
        const u32 tasks_per_thread = 12;      // Create multiple smaller tasks per thread for better load balancing
        const u32 min_entities_per_task = 48; // Even smaller chunks for more parallel work

        // Calculate maximum num_work_orders that won't exceed memory pool
        const u32 max_tasks = thread_data.size / sizeof(sim_thread_data);

        // Calculate initial work orders based on hardware threads
        u32 num_work_orders = hw_threads * tasks_per_thread;

        // Don't create more tasks than we have memory for
        num_work_orders = num_work_orders > max_tasks ? max_tasks : num_work_orders;

        // Ensure we don't divide into too-small chunks
        u32 num_entities_per_order = data->num_entities / num_work_orders;
        if (num_entities_per_order < min_entities_per_task)
        {
            // Adjust work orders to ensure each has at least min_entities_per_task
            num_work_orders = data->num_entities / min_entities_per_task;
            if (num_work_orders == 0)
                num_work_orders = 1; // Handle case with very few entities
            num_entities_per_order = data->num_entities / num_work_orders;
        }

        // Reset work queue using lock-free implementation
        thread_pool::reset_work();

        // We'll prepare all the task data first before submitting any work
        sim_thread_data *thread_data_array = (sim_thread_data *)mpool::get_bytes(
            &thread_data, sizeof(sim_thread_data) * num_work_orders);

        // Prepare all tasks without synchronization
        for (u32 i = 0; i < num_work_orders; i++)
        {
            // Calculate the range for this chunk with improved work distribution
            u32 start_idx = i * num_entities_per_order;
            u32 end_idx = (i == num_work_orders - 1) ? data->num_entities : (i + 1) * num_entities_per_order;

            thread_data_array[i].data = data;
            thread_data_array[i].start_index = start_idx;
            thread_data_array[i].end_index = end_idx;
        }

        // Submit all work using the lock-free queue
        for (u32 i = 0; i < num_work_orders; i++)
        {
            // Use lock-free queue to add work items
            thread_pool::add_work(sim_work_func, &thread_data_array[i]);
        }

        thread_pool::wait_for_completion();
        // More efficient main thread participation with adaptive waiting
        static mpool::memory_pool main_thread_memory = mpool::allocate(MEGABYTES(1));
        mpool::reset(&main_thread_memory);

        // Rebuild the spatial hash with new positions
        spatial_hash::rebuild(&data->search_hash, g_cell_size, data->num_entities, data->positions);
    }
};