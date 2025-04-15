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

// Replace aligned_alloc with _aligned_malloc and free with _aligned_free
// #define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
// #define aligned_free(ptr) _aligned_free(ptr)

namespace spatial_hash
{

    // Spatial hash structure updated to support an arbitrary domain.
    typedef struct spatial_hash
    {
        vec4 *positions; // Original positions, sorted by cell assignment.
        u32 *original_ids;
        u32 *cell_start; // Start index of each cell (in positions array).
        u32 *cell_end;   // End index (one past last) for each cell in positions array.
        float cell_size; // Size of a cell (provided as max_radius).
        // Grid dimensions along each axis.
        u32 grid_size_x;
        u32 grid_size_y;
        u32 grid_size_z;
        u32 num_positions; // Total number of positions.
        // Computed domain boundaries.
        vec4 domain_min; // Minimum coordinate across all positions.
        vec4 domain_max; // Maximum coordinate across all positions.
        mpool::memory_pool pool;
    } spatial_hash;

    // Helper function to compute the domain (min and max) of the positions.
    // It computes the axis-aligned bounding box for the input positions.
    static inline int compute_domain(const u32 num_positions, const vec4 *positions, vec4 *out_min, vec4 *out_max)
    {
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

    // --- Optimized inline quicksort implementation ---
    // This function sorts the indices in hash_table based on the corresponding cell_vals.
    static inline void quicksort_indices(u32 *hash_table, const u32 *cell_vals, int left, int right)
    {
        if (left >= right)
            return;

        // Choose pivot as middle value.
        int pivot_index = left + (right - left) / 2;
        u32 pivot = cell_vals[hash_table[pivot_index]];

        int i = left, j = right;
        while (i <= j)
        {
            while (cell_vals[hash_table[i]] < pivot)
                i++;
            while (cell_vals[hash_table[j]] > pivot)
                j--;
            if (i <= j)
            {
                // Swap the indices.
                u32 temp = hash_table[i];
                hash_table[i] = hash_table[j];
                hash_table[j] = temp;
                i++;
                j--;
            }
        }
        if (left < j)
            quicksort_indices(hash_table, cell_vals, left, j);
        if (i < right)
            quicksort_indices(hash_table, cell_vals, i, right);
    }
    // --- End of quicksort implementation ---

    // --- Optimized inline quicksort implementation ---
    // This function sorts positions directly based on their cell values.
    static inline void quicksort_positions(vec4 *positions, u32 *original_ids, u32 *cell_vals, int left, int right)
    {
        if (left >= right)
            return;

        // Choose pivot as middle value.
        int pivot_index = left + (right - left) / 2;
        u32 pivot = cell_vals[pivot_index];

        int i = left, j = right;
        while (i <= j)
        {
            while (cell_vals[i] < pivot)
                i++;
            while (cell_vals[j] > pivot)
                j--;
            if (i <= j)
            {
                // Swap both the cell values and the positions.
                u32 temp_cell = cell_vals[i];
                cell_vals[i] = cell_vals[j];
                cell_vals[j] = temp_cell;

                // Swap the positions
                vec4 temp_pos = positions[i];
                positions[i] = positions[j];
                positions[j] = temp_pos;

                // Swap the ids
                u32 temp_id = original_ids[i];
                original_ids[i] = original_ids[j];
                original_ids[j] = temp_id;

                i++;
                j--;
            }
        }
        if (left < j)
            quicksort_positions(positions, original_ids, cell_vals, left, j);
        if (i < right)
            quicksort_positions(positions, original_ids, cell_vals, i, right);
    }
    // --- End of quicksort implementation ---

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

    static inline void set_grid_sizes(spatial_hash *h, float max_radius)
    {
        // Compute grid sizes along each axis.
        h->grid_size_x = (u32)(ceilf((h->domain_max.x - h->domain_min.x) / max_radius));
        h->grid_size_y = (u32)(ceilf((h->domain_max.y - h->domain_min.y) / max_radius));
        h->grid_size_z = (u32)(ceilf((h->domain_max.z - h->domain_min.z) / max_radius));
    }

    static inline u64 get_cell_index(const spatial_hash *hash, const uivec3 cell_coords)
    {
// Compute the linear index for the 3D grid cell.
#if 0
        return cell_coords.x + cell_coords.y * hash->grid_size_x + cell_coords.z * hash->grid_size_x * hash->grid_size_y;
#else
        u64 result = libmorton::morton3D_64_encode(cell_coords.x, cell_coords.y, cell_coords.z);
        u32 temp_x = 0;
        u32 temp_y = 0;
        u32 temp_z = 0;
        libmorton::morton3D_64_decode(result, temp_x, temp_y, temp_z);
        return result; // morton3D_encode(cell_coords.x, cell_coords.y, cell_coords.z);
#endif
    }

    static inline u32 calc_num_cells(u32 grid_size_x, u32 grid_size_y, u32 grid_size_z)
    {
        return libmorton::morton3D_64_encode(grid_size_x, grid_size_y, grid_size_z) + 1;
    }

    static inline void build(spatial_hash *hash, float cell_size, u32 num_positions, const vec4 *initial_positions)
    {

        hash->cell_size = cell_size;
        hash->num_positions = num_positions;

        // Allocate memory for positions.
        hash->positions = (vec4 *)mpool::get_bytes(&hash->pool, sizeof(vec4) * num_positions);
        hash->original_ids = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_positions);
        if (!hash->positions)
        {
            fprintf(stderr, "Error: Memory allocation failed for positions\n");
            return;
        }

        // Copy the original positions.
        memcpy(hash->positions, initial_positions, sizeof(vec4) * num_positions);
        for (int i = 0; i < num_positions; ++i)
        {
            hash->original_ids[i] = i;
        }

        // Compute the domain (min and max values) of the positions.
        if (!compute_domain(num_positions, initial_positions, &hash->domain_min, &hash->domain_max))
        {
            return;
        }

        // Compute grid sizes along each axis.
        set_grid_sizes(hash, cell_size);
        u32 num_cells = calc_num_cells(hash->grid_size_x, hash->grid_size_y, hash->grid_size_z);

        // Allocate arrays for cell boundaries.
        hash->cell_start = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_cells);
        hash->cell_end = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_cells);
        if (!hash->cell_start || !hash->cell_end)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_start or cell_end\n");
            return;
        }

        // Allocate temporary array to store computed cell values for each position.
        u32 *cell_vals = (u32 *)mpool::get_bytes(&hash->pool, sizeof(u32) * num_positions);
        if (!cell_vals)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_vals\n");
            return;
        }

        // Compute cell assignment for each position.
        for (u32 i = 0; i < num_positions; ++i)
        {
            vec4 pos = hash->positions[i];
            uivec3 cell_coords = get_cell_coordinates(hash, pos);
            cell_vals[i] = get_cell_index(hash, cell_coords);
        }

        // Sort positions and cell_vals together
        quicksort_positions(hash->positions, hash->original_ids, cell_vals, 0, num_positions - 1);

        // Initialize cell_start and cell_end arrays.
        for (u32 i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = 0xFFFFFFFF; // sentinel value.
            hash->cell_end[i] = 0;
        }

        // Populate cell_start and cell_end based on the sorted positions.
        for (u32 i = 0; i < num_positions; ++i)
        {
            u32 cell_index = cell_vals[i];
            if (hash->cell_start[cell_index] == 0xFFFFFFFF)
            {
                hash->cell_start[cell_index] = i;
            }
            hash->cell_end[cell_index] = i + 1;
        }
    }

    static inline void rebuild(spatial_hash *hash, float cell_size, u32 num_positions, const vec4 *initial_positions)
    {
        mpool::reset(&hash->pool);
        memset(hash->pool.memory, 0, hash->pool.size); // Reset the memory pool.
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

        hash->pool = mpool::allocate(MEGABYTES(50)); // Allocate memory pool for the hash.
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
        float inv_cell_size = 1.0f / hash->cell_size;

        uivec3 cell_coords = get_cell_coordinates(hash, position);

        // Adjust query position by the domain minimum.
        float query_x = position.x - hash->domain_min.x;
        float query_y = position.y - hash->domain_min.y;
        float query_z = position.z - hash->domain_min.z;

        float offset_up = ceilf(radius * inv_cell_size);
        float offset_down = floorf(-radius * inv_cell_size);
        // Compute the bounds of the search space in grid coordinates.
        int min_x = (int)fmaxf(cell_coords.x + offset_down, 0.0f);
        int min_y = (int)fmaxf(cell_coords.y + offset_down, 0.0f);
        int min_z = (int)fmaxf(cell_coords.z + offset_down, 0.0f);
        int max_x = (int)fminf(cell_coords.x + offset_up, (float)(hash->grid_size_x - 1));
        int max_y = (int)fminf(cell_coords.y + offset_up, (float)(hash->grid_size_y - 1));
        int max_z = (int)fminf(cell_coords.z + offset_up, (float)(hash->grid_size_z - 1));

        float radius_sq = radius * radius;
        __m256 radius_squared = _mm256_set1_ps(radius_sq);

        for (u32 z = min_z; z <= max_z; ++z)
        {
            for (u32 y = min_y; y <= max_y; ++y)
            {
                for (u32 x = min_x; x <= max_x; ++x)

                {
                    cell_coords = {(u32)x, (u32)y, (u32)z};
                    u64 cell_index = get_cell_index(hash, cell_coords);

                    // Get the start and end indices for this cell
                    u32 start = hash->cell_start[cell_index];
                    u32 end = hash->cell_end[cell_index];

                    // Skip empty cells
                    if (start == 0xFFFFFFFF)
                        continue;

                    // Process positions within the cell in chunks of 8 for AVX vectorization
                    for (u32 i = start; i < end; i += 8)
                    {
                        u32 remaining = end - i;

                        // Fallback to scalar loop for less than 8 remaining positions
                        if (remaining < 8)
                        {
                            for (u32 j = 0; j < remaining; ++j)
                            {
                                // Direct access to positions array
                                vec4 pos = hash->positions[i + j];
                                vec4 diff = pos - position;
                                // float dist_squared = v4::dot(diff, diff); // assumes dot ignores w

                                // Compute distance only for x, y, z components (ignore w)
                                float dx = pos.x - position.x;
                                float dy = pos.y - position.y;
                                float dz = pos.z - position.z;
                                float dist_squared = dx * dx + dy * dy + dz * dz;

                                // Compare with strict threshold
                                if (dist_squared <= radius_sq)
                                {
                                    result_indices[*result_count] = hash->original_ids[i + j];
                                    (*result_count)++;
                                }
                            }
                            break;
                        }
                        float *base = (float *)hash->positions;

                        __m256i idx = _mm256_set_epi32(
                            i + 7, i + 6, i + 5, i + 4,
                            i + 3, i + 2, i + 1, i + 0);

                        __m256i byte_offsets = _mm256_mullo_epi32(idx, _mm256_set1_epi32(sizeof(vec4))); // i * 16

                        // Now safely add x/y/z offsets (0, 4, 8) in bytes
                        __m256 pos_x = _mm256_i32gather_ps(base, byte_offsets, 1);     // offset 0
                        __m256 pos_y = _mm256_i32gather_ps(base + 1, byte_offsets, 1); // offset 4 bytes = 1 float
                        __m256 pos_z = _mm256_i32gather_ps(base + 2, byte_offsets, 1); // offset 8 bytes = 2 floats
                        // Compute distance squared to query point
                        __m256 dx = _mm256_sub_ps(pos_x, _mm256_set1_ps(position.x));
                        __m256 dy = _mm256_sub_ps(pos_y, _mm256_set1_ps(position.y));
                        __m256 dz = _mm256_sub_ps(pos_z, _mm256_set1_ps(position.z));

                        __m256 dist_sq = _mm256_add_ps(
                            _mm256_mul_ps(dx, dx),
                            _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));

                        __m256 mask = _mm256_cmp_ps(dist_sq, radius_squared, _CMP_LE_OQ); // Use LE instead of LT
                        int mask_bits = _mm256_movemask_ps(mask);

                        // Add matching positions to result array
                        for (int j = 0; j < 8; ++j)
                        {
                            if (mask_bits & (1 << j))
                            {
                                result_indices[*result_count] = hash->original_ids[i + j];
                                (*result_count)++;
                            }
                        }
                    }
                }
            }
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
