#pragma once
#include "types.h"
#include "math_linear.h"
#include "malloc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <vector>
#include <immintrin.h> // For AVX2 intrinsics
#include <random>      // For random number generation in the test function
#include <malloc.h>    // For _aligned_malloc and _aligned_free on Windows
#include "morton.h"
#include "memory_pool.h"
#include "boid_thread.h" // For thread pool functionality

#include "tracy\public\tracy\Tracy.hpp"
#include "tracy\public\tracy\TracyOpenGL.hpp"
// Replace aligned_alloc with _aligned_malloc and free with _aligned_free
// #define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
// #define aligned_free(ptr) _aligned_free(ptr)
namespace spatial_hash
{

    // Spatial hash structure updated to support an arbitrary domain.
    typedef struct spatial_hash
    {
        float *position_x;
        float *position_y;
        float *position_z;
        u32 *original_ids;
        u32 *cell_start;     // Start index of each cell (in positions array).
        u32 *cell_end;       // End index (one past last) for each cell in positions array.
        float cell_size;     // Size of a cell (provided as max_radius).
        u32 last_cell_index; // Total number of cells in the grid.
        // Grid dimensions along each axis.
        volatile u32 grid_size_x;
        volatile u32 grid_size_y;
        volatile u32 grid_size_z;
        u32 num_positions; // Total number of positions.
        // Computed domain boundaries.
        vec4 domain_min; // Minimum coordinate across all positions.
        vec4 domain_max; // Maximum coordinate across all positions.
        mpool::memory_pool pool;
    } spatial_hash;

    // Thread data structure for parallel computing of domain
    struct compute_domain_thread_data
    {
        const vec4 *positions; // Pointer to the positions array
        u32 start_index;       // Start index for this thread's work
        u32 end_index;         // End index for this thread's work
        vec4 local_min;        // Thread-local minimum values
        vec4 local_max;        // Thread-local maximum values
    };

    // Thread data structure for parallel cell assignments
    struct compute_cell_assigments_thread_data
    {
        spatial_hash *hash;        // Pointer to the spatial hash
        u32 *cell_vals;            // Array to store cell values
        u32 start_index;           // Start index for this thread's work
        u32 end_index;             // End index for this thread's work
        u32 local_last_cell_index; // Thread-local maximum cell index
    };

    // Thread data structure for parallel radix sort
    struct RadixSortThreadData
    {
        int start;         // Start index for this thread's work
        int end;           // End index for this thread's work
        float *p_x;        // Position x coordinates
        float *p_y;        // Position y coordinates
        float *p_z;        // Position z coordinates
        u32 *original_ids; // Original indices
        u32 *cell_vals;    // Cell values
        u32 *local_count;  // Local count array (256 counts per thread)
        int byte;          // Current byte being processed
        // Shared arrays for all threads
        float *temp_x;
        float *temp_y;
        float *temp_z;
        u32 *temp_ids;
        u32 *temp_cells;
        u32 *global_offsets; // Global offsets for each value (256 values)
        // Synchronization
        volatile u32 *threads_done; // Counter for threads that have completed the current phase
    };

    // Helper function to compute the domain (min and max) of the positions.
    // It computes the axis-aligned bounding box for the input positions.
    static inline int compute_domain(const u32 num_positions, const vec4 *positions, vec4 *out_min, vec4 *out_max)
    {
        ZoneScoped;
        if (!positions || num_positions == 0 || !out_min || !out_max)
        {
            fprintf(stderr, "Error: Invalid parameters for computing domain\n");
            return 0;
        }

        // Initialize min and max with the first position.
        *out_min = positions[0];
        *out_max = positions[0];

        for (u32 i = 1; i < num_positions; ++i)
        {
            vec4 pos = positions[i];
            if (pos.x < out_min->x)
            {
                out_min->x = pos.x;
            }
            if (pos.y < out_min->y)
            {
                out_min->y = pos.y;
            }
            if (pos.z < out_min->z)
            {
                out_min->z = pos.z;
            }
            if (pos.x > out_max->x)
            {
                out_max->x = pos.x;
            }
            if (pos.y > out_max->y)
            {
                out_max->y = pos.y;
            }
            if (pos.z > out_max->z)
            {
                out_max->z = pos.z;
            }
        }

        return 1;
    }

    uivec3 get_cell_coordinates(const spatial_hash *hash, const vec4 pos)
    {
        float shifted_x = pos.x - hash->domain_min.x;
        float shifted_y = pos.y - hash->domain_min.y;
        float shifted_z = pos.z - hash->domain_min.z;

        // Compute cell coordinates.
        u32 cell_x = (u32)fmaxf(shifted_x / hash->cell_size, 0.0f);
        u32 cell_y = (u32)fmaxf(shifted_y / hash->cell_size, 0.0f);
        u32 cell_z = (u32)fmaxf(shifted_z / hash->cell_size, 0.0f);

        // Clamp to grid bounds.
        cell_x = (cell_x < hash->grid_size_x) ? cell_x : hash->grid_size_x - 1;
        cell_y = (cell_y < hash->grid_size_y) ? cell_y : hash->grid_size_y - 1;
        cell_z = (cell_z < hash->grid_size_z) ? cell_z : hash->grid_size_z - 1;
        return {cell_x, cell_y, cell_z};
    }

    // Thread worker function for domain computation
    static void compute_domain_thread_worker(void *data, u32 thread_id, mpool::memory_pool *thread_memory)
    {
        ZoneScoped;
        compute_domain_thread_data *thread_data = (compute_domain_thread_data *)data;

        // Initialize with the first position in this thread's range
        if (thread_data->start_index < thread_data->end_index)
        {
            thread_data->local_min = thread_data->positions[thread_data->start_index];
            thread_data->local_max = thread_data->positions[thread_data->start_index];

            // Process all positions assigned to this thread
            for (u32 i = thread_data->start_index + 1; i < thread_data->end_index; ++i)
            {
                vec4 pos = thread_data->positions[i];

                // Update minimum values
                thread_data->local_min.x = pos.x < thread_data->local_min.x ? pos.x : thread_data->local_min.x;
                thread_data->local_min.y = pos.y < thread_data->local_min.y ? pos.y : thread_data->local_min.y;
                thread_data->local_min.z = pos.z < thread_data->local_min.z ? pos.z : thread_data->local_min.z;

                // Update maximum values
                thread_data->local_max.x = pos.x > thread_data->local_max.x ? pos.x : thread_data->local_max.x;
                thread_data->local_max.y = pos.y > thread_data->local_max.y ? pos.y : thread_data->local_max.y;
                thread_data->local_max.z = pos.z > thread_data->local_max.z ? pos.z : thread_data->local_max.z;
            }
        }
    }

    static inline u32 get_cell_index(const spatial_hash *hash, const uivec3 cell_coords)
    {

// Compute the linear index for the 3D grid cell.
#if 1
        return cell_coords.x +
               cell_coords.y * hash->grid_size_x + cell_coords.z * hash->grid_size_x * hash->grid_size_y;
#elif 0
        u32 result = libmorton::morton3D_32_encode(cell_coords.x, cell_coords.y, cell_coords.z);
        return result; // morton3D_encode(cell_coords.x, cell_coords.y, cell_coords.z);
#elif 0
        // Use Hilbert curve encoding for better spatial locality.
        u32 result = hilbert3D_encode(cell_coords.x, cell_coords.y, cell_coords.z);
        return result; // morton3D_encode(cell_coords.x, cell_coords.y, cell_coords.z);
#endif
    }
    static inline int compute_domain_mt(const u32 num_positions,
                                        const vec4 *positions,
                                        vec4 *out_min,
                                        vec4 *out_max)
    {
        ZoneScoped;
        if (!positions || num_positions == 0 || !out_min || !out_max)
        {
            fprintf(stderr, "Error: Invalid parameters for computing domain\n");
            return 0;
        }
        if (num_positions < 1024)
        {
            return compute_domain(num_positions, positions, out_min, out_max);
        }

        const u32 num_threads = (thread_pool::g_thread_pool)
                                    ? thread_pool::g_thread_pool->num_threads
                                    : 1;
        if (num_threads <= 1)
        {
            return compute_domain(num_positions, positions, out_min, out_max);
        }

        const u32 min_positions_per_thread = 512;
        const u32 actual_threads = (num_threads < num_positions / min_positions_per_thread)
                                       ? num_threads
                                       : (num_positions / min_positions_per_thread);
        if (actual_threads <= 1)
        {
            return compute_domain(num_positions, positions, out_min, out_max);
        }

        // Use a std::vector so its buffer stays alive until we exit the function.
        // *** scoped buffer lives until this function returns ***
        static mpool::memory_pool thread_memory_pool = mpool::allocate(sizeof(compute_cell_assigments_thread_data) * actual_threads * 2);
        mpool::reset(&thread_memory_pool);
        compute_domain_thread_data *tdata = (compute_domain_thread_data *)mpool::get_bytes(&thread_memory_pool, sizeof(compute_domain_thread_data) * actual_threads);

        for (u32 i = 0; i < actual_threads; ++i)
        {
            tdata[i].positions = positions;
            tdata[i].start_index = i * (num_positions / actual_threads);
            tdata[i].end_index = (i == actual_threads - 1)
                                     ? num_positions
                                     : (i + 1) * (num_positions / actual_threads);
            thread_pool::add_work(compute_domain_thread_worker, &tdata[i]);
        }

        // This must *really* wait for all threads to finish their work.
        thread_pool::wait_for_completion();

        // Reduce the per-thread mins/maxes into the final out_min/out_max.
        *out_min = tdata[0].local_min;
        *out_max = tdata[0].local_max;
        for (u32 i = 1; i < actual_threads; ++i)
        {
            out_min->x = min(out_min->x, tdata[i].local_min.x);
            out_min->y = min(out_min->y, tdata[i].local_min.y);
            out_min->z = min(out_min->z, tdata[i].local_min.z);
            out_max->x = max(out_max->x, tdata[i].local_max.x);
            out_max->y = max(out_max->y, tdata[i].local_max.y);
            out_max->z = max(out_max->z, tdata[i].local_max.z);
        }

        // No free needed—vector memory is only released when we exit the function
        return 1;
    }

    static inline void set_grid_sizes(spatial_hash *h, float max_radius)
    {
        // Use adaptive cell sizing: cell_size is a multiple of max_radius
        // This significantly reduces the number of cells in the grid
        const float cell_size_multiplier = 2.0f; // Can be tuned based on density of objects
        h->cell_size = max_radius * cell_size_multiplier;

        // Compute grid sizes along each axis based on the adjusted cell size
        h->grid_size_x = (u32)(ceilf((h->domain_max.x - h->domain_min.x) / h->cell_size));
        h->grid_size_y = (u32)(ceilf((h->domain_max.y - h->domain_min.y) / h->cell_size));
        h->grid_size_z = (u32)(ceilf((h->domain_max.z - h->domain_min.z) / h->cell_size));
    }

    // Compute 3D Hilbert index for 21-bit input coordinates (max 2097152 per axis)
    static inline u64 hilbert3D_encode(u32 x, u32 y, u32 z)
    {
        // Ensure inputs are in 21-bit range
        x &= 0x1FFFFF;
        y &= 0x1FFFFF;
        z &= 0x1FFFFF;

        u64 hilbert = 0;

        // Initial rotation state
        u32 rotation = 0;

        // For each bit (starting from most significant)
        for (int i = 20; i >= 0; --i)
        {
            // Extract the i-th bit from each coordinate
            u32 xi = (x >> i) & 1;
            u32 yi = (y >> i) & 1;
            u32 zi = (z >> i) & 1;

            // Apply current rotation to the bits
            u32 index = (xi << 2) | (yi << 1) | zi;
            static const u8 hilbert_table[24][8] = {
                // Next rotation table: [current_rotation][current_index] -> {next_rotation}
                // Precomputed based on 3D Hilbert state transitions
                {0, 3, 5, 6, 1, 2, 4, 7}, // rot 0
                {1, 2, 6, 7, 0, 3, 5, 4}, // rot 1
                {2, 1, 7, 4, 3, 0, 6, 5}, // rot 2
                {3, 0, 4, 5, 2, 1, 7, 6}, // rot 3
                {4, 5, 1, 2, 7, 6, 0, 3}, // rot 4
                {5, 4, 2, 3, 6, 7, 1, 0}, // rot 5
                {6, 7, 3, 0, 5, 4, 2, 1}, // rot 6
                {7, 6, 0, 1, 4, 5, 3, 2}, // rot 7
                // Fallback for safety if above indices go out of range
            };

            static const u8 hilbert_entry[8] = {
                0, 1, 3, 2, 7, 6, 4, 5};

            u32 entry = hilbert_entry[index];

            hilbert = (hilbert << 3) | entry;

            // Update rotation for next level
            rotation = hilbert_table[rotation][index] % 8;
        }

        return hilbert;
    }

    static void morton_test()
    {
        std::vector<u64> morton_codes;
        for (int z = 0; z < 8; ++z)
        {
            for (int y = 0; y < 8; ++y)
            {
                for (int x = 0; x < 8; ++x)
                {
                    u64 morton_code = libmorton::morton3D_64_encode(x, y, z);
                    morton_codes.push_back(morton_code);
                    uivec3 decoded_coords;
                    libmorton::morton3D_64_decode(morton_code, decoded_coords.x, decoded_coords.y, decoded_coords.z);
                    printf("x: %d, y: %d, z: %d -> Morton Code: %llu -> Decoded: (%d, %d, %d)\n",
                           x, y, z, morton_code, decoded_coords.x, decoded_coords.y, decoded_coords.z);
                }
            }
        }
        std::sort(morton_codes.begin(), morton_codes.end());
        for (size_t i = 0; i < morton_codes.size(); ++i)
        {
            printf("Sorted Morton Code: %llu\n", morton_codes[i]);
        }
    }

    static inline u32 calc_num_cells(spatial_hash *hash, u32 grid_size_x, u32 grid_size_y, u32 grid_size_z)
    {
        return get_cell_index(hash, {grid_size_x, grid_size_y, grid_size_z}) + 1;
    }

    void compute_cell_assigments(uint_fast32_t num_positions, spatial_hash *hash, uint_fast32_t *cell_vals)
    {
        ZoneScoped;
        for (u32 i = 0; i < num_positions; ++i)
        {
            vec4 pos = {hash->position_x[i], hash->position_y[i], hash->position_z[i], 1.0f};

            uivec3 cell_coords = get_cell_coordinates(hash, pos);
            cell_vals[i] = get_cell_index(hash, cell_coords);
            if (cell_vals[i] > hash->last_cell_index)
            {
                hash->last_cell_index = cell_vals[i];
            }
        }
    }

    struct bin_assigment_thread_data
    {
        spatial_hash *hash;
        volatile u32 *cell_vals;
        volatile u32 *cell_counts;
        float *temp_position_x;
        float *temp_position_y;
        float *temp_position_z;
        u32 *temp_original_ids;
        u32 start;
        u32 end;
    };

    static void
    bin_positions_assignment_worker(void *data, u32 thread_id, mpool::memory_pool *thread_memory)
    {
        ZoneScoped;
        bin_assigment_thread_data *thread_data = (bin_assigment_thread_data *)data;
        for (int i = thread_data->start; i < thread_data->end; ++i)
        {

            u32 cell_id = thread_data->cell_vals[i];
            u32 offset = InterlockedDecrement(&thread_data->cell_counts[cell_id]) + 1;
            u32 start = thread_data->hash->cell_start[cell_id];
            thread_data->temp_position_x[start + offset] = thread_data->hash->position_x[i];
            thread_data->temp_position_y[start + offset] = thread_data->hash->position_y[i];
            thread_data->temp_position_z[start + offset] = thread_data->hash->position_z[i];
            thread_data->temp_original_ids[start + offset] = thread_data->hash->original_ids[i];
        }
    }

    struct compute_cell_countsvals_thread_data
    {
        spatial_hash *hash;
        volatile u32 *cell_vals;
        volatile u32 *cell_counts;
        u32 num_positions;
        u32 start_index;
        u32 end_index;
    };

    static void bin_positions_countsvals_worker(void *data, u32 thread_id, mpool::memory_pool *thread_memory)
    {
        ZoneScoped;
        compute_cell_countsvals_thread_data *thread_data = (compute_cell_countsvals_thread_data *)data;
        for (int i = thread_data->start_index; i < thread_data->end_index; ++i)
        {
            vec4 position = {thread_data->hash->position_x[i], thread_data->hash->position_y[i], thread_data->hash->position_z[i], 1.0f};
            uivec3 cell_coords = get_cell_coordinates(thread_data->hash, position);
            u32 cell_value = get_cell_index(thread_data->hash, cell_coords);

            thread_data->cell_vals[i] = cell_value;
            InterlockedIncrement(&thread_data->cell_counts[cell_value]);

            // cell_counts[cell_vals[i]]++;
        }
    }
    static inline void bin_positions(spatial_hash *hash, u32 num_positions, const vec4 *initial_positions, u32 num_cells, volatile u32 *cell_vals)
    {
        ZoneScoped;
        float *temp_position_x = (float *)mpool::get_bytes(&hash->pool, sizeof(float) * num_positions);
        float *temp_position_y = (float *)mpool::get_bytes(&hash->pool, sizeof(float) * num_positions);
        float *temp_position_z = (float *)mpool::get_bytes(&hash->pool, sizeof(float) * num_positions);
        u32 *temp_original_ids = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_positions);

        volatile u32 *cell_counts = (volatile u32 *)mpool::get_bytes(&hash->pool, sizeof(volatile u32) * num_cells);
        for (int i = 0; i < num_cells; ++i)
        {
            cell_counts[i] = 0;
        }

        u32 num_jobs = 64; // thread_pool::g_thread_pool->num_threads;
        compute_cell_countsvals_thread_data *count_job_datas = (compute_cell_countsvals_thread_data *)mpool::get_bytes(&hash->pool, sizeof(compute_cell_countsvals_thread_data) * num_jobs);
        for (int i = 0; i < num_jobs; ++i)
        {
            count_job_datas[i].hash = hash;
            count_job_datas[i].cell_vals = cell_vals;
            count_job_datas[i].cell_counts = cell_counts;
            count_job_datas[i].start_index = i * (num_positions / num_jobs);
            count_job_datas[i].end_index = (i == num_jobs - 1) ? (num_positions) : (i + 1) * (num_positions / num_jobs);
            count_job_datas[i].num_positions = num_positions;
            thread_pool::add_work(bin_positions_countsvals_worker, &count_job_datas[i]);
        }

        thread_pool::wait_for_completion();

        for (int i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = (i == 0) ? 0 : hash->cell_start[i - 1] + cell_counts[i - 1];
            hash->cell_end[i] = hash->cell_start[i] + cell_counts[i];
        }
        {
            ZoneScoped;
            for (int i = 0; i < num_positions; ++i)
            {

                u32 cell_id = cell_vals[i];
                u32 offset = InterlockedDecrement(&cell_counts[cell_id]) + 1;
                u32 start = hash->cell_start[cell_id];
                temp_position_x[start + offset] = hash->position_x[i];
                temp_position_y[start + offset] = hash->position_y[i];
                temp_position_z[start + offset] = hash->position_z[i];
                temp_original_ids[start + offset] = hash->original_ids[i];
            }
        }
        // swap the array pointers
        std::swap(hash->position_x, temp_position_x);
        std::swap(hash->position_y, temp_position_y);
        std::swap(hash->position_z, temp_position_z);
        std::swap(hash->original_ids, temp_original_ids);
    }

    static inline void build(spatial_hash *hash, float cell_size, u32 num_positions, const vec4 *initial_positions)
    {
        ZoneScoped;
        hash->cell_size = cell_size;
        hash->num_positions = num_positions;

        hash->position_x = (float *)mpool::get_bytes(&hash->pool, sizeof(float) * num_positions);
        hash->position_y = (float *)mpool::get_bytes(&hash->pool, sizeof(float) * num_positions);
        hash->position_z = (float *)mpool::get_bytes(&hash->pool, sizeof(float) * num_positions);
        hash->original_ids = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_positions);

        // Copy the original positions.
        for (int i = 0; i < num_positions; ++i)
        {

            hash->position_x[i] = initial_positions[i].x;
            hash->position_y[i] = initial_positions[i].y;
            hash->position_z[i] = initial_positions[i].z;
            hash->original_ids[i] = i;
        }
        compute_domain_mt(num_positions, initial_positions, &hash->domain_min, &hash->domain_max);

        // Compute grid sizes along each axis.
        set_grid_sizes(hash, cell_size);
        u32 num_cells = calc_num_cells(hash, hash->grid_size_x, hash->grid_size_y, hash->grid_size_z);

        // Allocate arrays for cell boundaries.
        hash->cell_start = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_cells);
        hash->cell_end = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_cells);
        if (!hash->cell_start || !hash->cell_end)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_start or cell_end\n");
            return;
        }

        // Allocate temporary array to store computed cell values for each position.
        volatile u32 *cell_vals = (volatile u32 *)mpool::get_bytes(&hash->pool, sizeof(volatile u32) * num_positions);
        if (!cell_vals)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_vals\n");
            return;
        }
        hash->last_cell_index = 0;

        // Initialize cell_start and cell_end arrays.
        for (u32 i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = 0xFFFFFFFF; // sentinel value.
            hash->cell_end[i] = 0;
        }

        bin_positions(hash, num_positions, initial_positions, num_cells, cell_vals);
    }

    static inline void rebuild(spatial_hash *hash, float cell_size, u32 num_positions, const vec4 *initial_positions)
    {
        ZoneScoped;
        mpool::reset(&hash->pool);
        build(hash, cell_size, num_positions, initial_positions);
    }

    // Initializes the spatial hash structure.
    // The "max_radius" value is used as the cell size.
    // The domain is computed automatically from the provided positions.
    static inline void init(spatial_hash *hash, float cell_size, u32 num_positions, const vec4 *initial_positions)
    {
        if (!hash || cell_size <= 0.0f || num_positions == 0 || !initial_positions)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash initialization\n");
            return;
        }

        hash->pool = mpool::allocate(MEGABYTES(500)); // Allocate memory pool for the hash.
        build(hash, cell_size, num_positions, initial_positions);
    }

    static inline void search(const spatial_hash *hash, vec4 position, float radius, u32 *result_indices, u32 *result_count)
    {
        if (!hash || !result_indices || !result_count || radius <= 0.0f)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash search\n");
            return;
        }

        *result_count = 0;

        // Pre-compute these values once to avoid repeated calculations
        const float radius_sq = radius * radius;
        const float inv_cell_size = 1.0f / hash->cell_size;

        // Get the central cell for the query position
        uivec3 cell_coords = get_cell_coordinates(hash, position);

        // Calculate cell search bounds (optimization: computed once outside the loops)
        const float offset_up = ceilf(radius * inv_cell_size);
        const float offset_down = floorf(-radius * inv_cell_size);

        const int min_x = (int)fmaxf(cell_coords.x + offset_down, 0.0f);
        const int min_y = (int)fmaxf(cell_coords.y + offset_down, 0.0f);
        const int min_z = (int)fmaxf(cell_coords.z + offset_down, 0.0f);
        const int max_x = (int)fminf(cell_coords.x + offset_up, (float)(hash->grid_size_x - 1));
        const int max_y = (int)fminf(cell_coords.y + offset_up, (float)(hash->grid_size_y - 1));
        const int max_z = (int)fminf(cell_coords.z + offset_up, (float)(hash->grid_size_z - 1));

        // Prepare vectorized constants for distance calculations
        __m256 radius_squared = _mm256_set1_ps(radius_sq);
        __m256 pos_x_vec = _mm256_set1_ps(position.x);
        __m256 pos_y_vec = _mm256_set1_ps(position.y);
        __m256 pos_z_vec = _mm256_set1_ps(position.z);

        // Pre-allocate a local buffer to store found indices first before copying
        // This improves cache locality by batching memory operations
        // Increased buffer size for fewer flushes
        const u32 MAX_LOCAL_RESULTS = 2048; // Tunable parameter, increased from 1024
        // static mpool::memory_pool local_result_pool = mpool::allocate(MAX_LOCAL_RESULTS * sizeof(u32));
        // mpool::reset(&local_result_pool); // Reset the memory pool for local results
        // u32 *local_result_indices = (u32 *)mpool::get_bytes(&local_result_pool, sizeof(u32) * MAX_LOCAL_RESULTS);
        u32 local_result_indices[MAX_LOCAL_RESULTS];
        u32 local_result_count = 0;

        u32 num_avx = 0;
        u32 num_scalar = 0;

        // u32 max_valid_index = get_cell_index(hash, {hash->grid_size_x - 1, hash->grid_size_y - 1, hash->grid_size_z - 1});

        // Process one z-layer at a time to improve cache coherence
        for (u32 z = min_z; z <= max_z; ++z)
        {
            for (u32 y = min_y; y <= max_y; ++y)
            {
                // Process x-coordinates in a contiguous batch for better cache locality
                for (u32 x = min_x; x <= max_x; ++x)
                {
                    // Get cell and indices
                    const uivec3 current_cell_coords = {(u32)x, (u32)y, (u32)z};
                    const u32 cell_index = get_cell_index(hash, current_cell_coords);

                    // Skip empty cells early
                    u32 start = hash->cell_start[cell_index];
                    if (start == 0xFFFFFFFF)
                        continue;
                    u32 end = hash->cell_end[cell_index]; // hash->cell_end[cell_index];

                    // Process positions in chunks of 8 for AVX vectorization
                    u32 i = start;

                    // Process full vector blocks
                    for (; i + 8 <= end; i += 8)
                    {
                        num_avx += 8;
                        // Access positions memory linearly for better cache utilization

                        // Load x, y, z coordinates from separate arrays
                        __m256 pos_x0 = _mm256_loadu_ps(&hash->position_x[i]);
                        __m256 pos_y0 = _mm256_loadu_ps(&hash->position_y[i]);
                        __m256 pos_z0 = _mm256_loadu_ps(&hash->position_z[i]);
                        // Compute squared distances in one go
                        __m256 dx = _mm256_sub_ps(pos_x0, pos_x_vec);
                        __m256 dy = _mm256_sub_ps(pos_y0, pos_y_vec);
                        __m256 dz = _mm256_sub_ps(pos_z0, pos_z_vec);

                        // Compute squared distance using FMA when available for better precision

                        __m256 dist_sq = _mm256_add_ps(
                            _mm256_mul_ps(dx, dx),
                            _mm256_add_ps(
                                _mm256_mul_ps(dy, dy),
                                _mm256_mul_ps(dz, dz)));

                        // Create mask for positions within radius
                        __m256 mask = _mm256_cmp_ps(dist_sq, radius_squared, _CMP_LE_OQ);
                        int mask_bits = _mm256_movemask_ps(mask);

                        // Prefetch next positions data to reduce cache misses
                        if (i + 16 < end)
                        {

                            _mm_prefetch((const char *)&hash->position_x[i + 16], _MM_HINT_T0);
                            _mm_prefetch((const char *)&hash->position_y[i + 16], _MM_HINT_T0);
                            _mm_prefetch((const char *)&hash->position_z[i + 16], _MM_HINT_T0);
                            _mm_prefetch((const char *)&hash->original_ids[i + 16], _MM_HINT_T0);
                        }

                        // Use a fast bit counting method to determine how many results we'll add
                        const u32 count_to_add = _mm_popcnt_u32(mask_bits);

                        // Ensure we don't overflow local buffer
                        if (local_result_count + count_to_add > MAX_LOCAL_RESULTS)
                        {
                            // Copy the buffer to the output and reset
                            memcpy(&result_indices[*result_count], local_result_indices,
                                   local_result_count * sizeof(u32));
                            *result_count += local_result_count;
                            local_result_count = 0;
                        }

                        // Process matching indices using bit manipulation instead of looping
                        for (int j = 0; j < 8; ++j)
                        {
                            // Use conditional move instead of branch for better pipelining
                            const u32 should_add = (mask_bits >> j) & 1;
                            local_result_indices[local_result_count] = hash->original_ids[i + j];
                            local_result_count += should_add;
                        }
                    }

                    // Process remaining positions (less than 8) with scalar code
                    for (; i < end; ++i)
                    {
                        num_scalar++;

                        const vec4 pos = {hash->position_x[i], hash->position_y[i], hash->position_z[i], 1.0f};
                        // Compute distance only for x, y, z components (ignore w)
                        const float dx = pos.x - position.x;
                        const float dy = pos.y - position.y;
                        const float dz = pos.z - position.z;
                        const float dist_squared = dx * dx + dy * dy + dz * dz;

                        // Compare with radius threshold
                        if (dist_squared <= radius_sq)
                        {
                            // Add to local buffer if there's room
                            if (local_result_count < MAX_LOCAL_RESULTS)
                            {
                                local_result_indices[local_result_count++] = hash->original_ids[i];
                            }
                            else
                            {
                                // Flush buffer to output and reset
                                memcpy(&result_indices[*result_count], local_result_indices,
                                       local_result_count * sizeof(u32));
                                *result_count += local_result_count;
                                local_result_count = 0;
                                local_result_indices[local_result_count++] = hash->original_ids[i];
                            }
                        }
                    }
                }
            }
        }

        // float proportion_avx = (float)num_avx / (float)(num_avx + num_scalar);
        // printf("AVX proportion: %.2f%%\n", proportion_avx * 100.0f);

        // Copy any remaining results to the output buffer
        if (local_result_count > 0)
        {
            memcpy(&result_indices[*result_count], local_result_indices,
                   local_result_count * sizeof(u32));
            *result_count += local_result_count;
        }
    }

    // Corrected the brute-force validation logic in the test function to ensure proper comparison of distances.
    static inline int test()
    {
        const float max_radius = 0.5f;
        const u32 num_positions = 1000;

        mpool::memory_pool test_pool = mpool::allocate(MEGABYTES(50)); // Allocate memory pool for the test.
        vec4 *test_positions = (vec4 *)mpool::get_bytes(&test_pool, sizeof(vec4) * num_positions);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (u32 i = 0; i < num_positions; ++i)
        {
            test_positions[i] = {dis(gen), dis(gen), dis(gen), 1.0f};
        }

        spatial_hash hash = {0};
        init(&hash, max_radius, num_positions, test_positions);

        vec4 search_position = {0.0f, 0.0f, 0.0f, 1.0f};
        float search_radius = max_radius;
        u32 *result_indices = (u32 *)mpool::get_bytes(&test_pool, sizeof(u32) * num_positions);
        u32 result_count = 0;

        search(&hash, search_position, search_radius, result_indices, &result_count);

        // Validate results with brute force
        u32 ground_truth_result_count = 0;
        for (u32 i = 0; i < num_positions; ++i)
        {
            vec4 diff = test_positions[i] - search_position;
            float dist_squared = v4::dot(diff, diff);
            if (dist_squared <= search_radius * search_radius)
            {
                ground_truth_result_count++;
            }
        }

        if (result_count != ground_truth_result_count)
        {
            fprintf(stderr, "Error: Expected %u results, got %u\n", ground_truth_result_count, result_count);
            return 0;
        }

        int unique_count = 0;
        for (int i = 0; i < result_count; ++i)
        {
            for (int j = 0; j < result_count; j++)
            {
                if (result_indices[i] == result_indices[j])
                {
                    unique_count++;
                }
            }
        }
        if (unique_count != result_count)
        {
            fprintf(stderr, "Error: Found duplicate results\n");
            return 0;
        }

        for (u32 i = 0; i < num_positions; ++i)
        {
            test_positions[i] = {dis(gen), dis(gen), dis(gen), 1.0f};
        }

        rebuild(&hash, max_radius, num_positions, test_positions);
        search(&hash, search_position, search_radius, result_indices, &result_count);

        // Validate results with brute force
        ground_truth_result_count = 0;
        for (u32 i = 0; i < num_positions; ++i)
        {
            vec4 diff = test_positions[i] - search_position;
            float dist_squared = v4::dot(diff, diff);
            if (dist_squared <= search_radius * search_radius)
            {
                ground_truth_result_count++;
            }
        }

        if (result_count != ground_truth_result_count)
        {
            fprintf(stderr, "Error: Expected %u results, got %u\n", ground_truth_result_count, result_count);
            return 0;
        }

        unique_count = 0;
        for (int i = 0; i < result_count; ++i)
        {
            for (int j = 0; j < result_count; j++)
            {
                if (result_indices[i] == result_indices[j])
                {
                    unique_count++;
                }
            }
        }
        if (unique_count != result_count)
        {
            fprintf(stderr, "Error: Found duplicate results\n");
            return 0;
        }
        mpool::deallocate(&test_pool); // Deallocate the memory pool for the test.
        return 1;
    }

} // namespace spatial_hash
