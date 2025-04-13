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

// Replace aligned_alloc with _aligned_malloc and free with _aligned_free
#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)

namespace spatial_hash
{

    // Spatial hash structure updated to support an arbitrary domain.
    typedef struct spatial_hash
    {
        vec4 *positions; // Original positions.
        u64 *entity_ids;
        u32 *hash_table; // Sorted indices (order according to cell assignment).
        u32 *cell_start; // Start index of each cell (in hash_table).
        u32 *cell_end;   // End index (one past last) for each cell in hash_table.
        float cell_size; // Size of a cell (provided as max_radius).
        // Grid dimensions along each axis.
        u32 grid_size_x;
        u32 grid_size_y;
        u32 grid_size_z;
        u32 num_positions; // Total number of positions.
        // Computed domain boundaries.
        vec4 domain_min; // Minimum coordinate across all positions.
        vec4 domain_max; // Maximum coordinate across all positions.
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

    ivec3 get_cell_coordinates(const spatial_hash *hash, const vec4 pos)
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

    int get_cell_index(const spatial_hash *hash, const ivec3 cell_coords)
    {
        // Compute the linear index for the 3D grid cell.
        return cell_coords.x + cell_coords.y * hash->grid_size_x + cell_coords.z * hash->grid_size_x * hash->grid_size_y;
    }

    // Initializes the spatial hash structure.
    // The "max_radius" value is used as the cell size.
    // The domain is computed automatically from the provided positions.
    static inline void init(spatial_hash *hash, float max_radius, u32 num_positions, const vec4 *initial_positions)
    {
        if (!hash || max_radius <= 0.0f || num_positions == 0 || !initial_positions)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash initialization\n");
            return;
        }

        hash->cell_size = max_radius;
        hash->num_positions = num_positions;

        // Allocate memory for positions and the hash table.
        hash->positions = (vec4 *)_aligned_malloc(sizeof(vec4) * num_positions, 32);
        hash->hash_table = (u32 *)malloc(sizeof(u32) * num_positions);
        hash->entity_ids = (u64 *)malloc(sizeof(u64) * num_positions);
        if (!hash->positions || !hash->hash_table)
        {
            fprintf(stderr, "Error: Memory allocation failed for positions or hash_table\n");
            return;
        }

        // Copy the original positions.
        memcpy(hash->positions, initial_positions, sizeof(vec4) * num_positions);

        // Compute the domain (min and max values) of the positions.
        if (!compute_domain(num_positions, initial_positions, &hash->domain_min, &hash->domain_max))
        {
            return;
        }

        // Compute grid sizes along each axis.
        // Adding 1 to ensure that the max boundary is included.
        hash->grid_size_x = (u32)(ceilf((hash->domain_max.x - hash->domain_min.x) / max_radius)) + 1;
        hash->grid_size_y = (u32)(ceilf((hash->domain_max.y - hash->domain_min.y) / max_radius)) + 1;
        hash->grid_size_z = (u32)(ceilf((hash->domain_max.z - hash->domain_min.z) / max_radius)) + 1;
        u32 num_cells = hash->grid_size_x * hash->grid_size_y * hash->grid_size_z;

        // Allocate arrays for cell boundaries.
        hash->cell_start = (u32 *)malloc(sizeof(u32) * num_cells);
        hash->cell_end = (u32 *)malloc(sizeof(u32) * num_cells);
        if (!hash->cell_start || !hash->cell_end)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_start or cell_end\n");
            return;
        }

        // Initialize hash_table with indices 0 .. num_positions - 1.
        for (u32 i = 0; i < num_positions; ++i)
        {
            hash->hash_table[i] = i;
        }

        // Allocate temporary array to store computed cell values for each index.
        u32 *cell_vals = (u32 *)malloc(sizeof(u32) * num_positions);
        if (!cell_vals)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_vals\n");
            return;
        }

        // Compute cell assignment for each position.
        // The cell value is computed by shifting the position by domain_min and then dividing by cell_size.
        for (u32 i = 0; i < num_positions; ++i)
        {
            vec4 pos = initial_positions[i];
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
            cell_vals[i] = cell_x + cell_y * hash->grid_size_x + cell_z * hash->grid_size_x * hash->grid_size_y;
        }

        // --- Optimized Sorting ---
        // Replace bubble sort with inline quicksort.
        quicksort_indices(hash->hash_table, cell_vals, 0, num_positions - 1);

        // Initialize cell_start and cell_end arrays.
        for (u32 i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = 0xFFFFFFFF; // sentinel value.
            hash->cell_end[i] = 0;
        }

        // Populate cell_start and cell_end based on the sorted order in hash_table.
        // Instead of recomputing the cell coordinates, reuse the precomputed cell_vals.
        for (u32 i = 0; i < num_positions; ++i)
        {
            u32 idx = hash->hash_table[i];
            u32 cell_index = cell_vals[idx];
            if (hash->cell_start[cell_index] == 0xFFFFFFFF)
            {
                hash->cell_start[cell_index] = i;
            }
            hash->cell_end[cell_index] = i + 1;
        }

        free(cell_vals);
    }

    // Updates the spatial hash with new positions.
    // Replaces the positions and re-bins them into cells.
    // May recalculate domain and reallocate total number of cells.
    static inline void update(spatial_hash *hash, vec4 *new_positions, u32 count)
    {
        if (!hash || !new_positions || count == 0)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash update\n");
            return;
        }

        // Free existing memory for positions and hash table.
        aligned_free(hash->positions);
        free(hash->hash_table);
        free(hash->cell_start);
        free(hash->cell_end);

        // Update the number of positions.
        hash->num_positions = count;

        // Allocate memory for new positions and hash table.
        hash->positions = (vec4 *)_aligned_malloc(sizeof(vec4) * count, 32);
        hash->hash_table = (u32 *)malloc(sizeof(u32) * count);
        if (!hash->positions || !hash->hash_table)
        {
            fprintf(stderr, "Error: Memory allocation failed for positions or hash_table during update\n");
            return;
        }

        // Copy the new positions.
        memcpy(hash->positions, new_positions, sizeof(vec4) * count);

        // Recompute the domain (min and max values) of the new positions.
        if (!compute_domain(count, new_positions, &hash->domain_min, &hash->domain_max))
        {
            return;
        }

        // Recompute grid sizes along each axis.
        hash->grid_size_x = (u32)(ceilf((hash->domain_max.x - hash->domain_min.x) / hash->cell_size)) + 1;
        hash->grid_size_y = (u32)(ceilf((hash->domain_max.y - hash->domain_min.y) / hash->cell_size)) + 1;
        hash->grid_size_z = (u32)(ceilf((hash->domain_max.z - hash->domain_min.z) / hash->cell_size)) + 1;
        u32 num_cells = hash->grid_size_x * hash->grid_size_y * hash->grid_size_z;

        // Allocate memory for cell boundaries.
        hash->cell_start = (u32 *)malloc(sizeof(u32) * num_cells);
        hash->cell_end = (u32 *)malloc(sizeof(u32) * num_cells);
        if (!hash->cell_start || !hash->cell_end)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_start or cell_end during update\n");
            return;
        }

        // Initialize hash_table with indices 0 .. count - 1.
        for (u32 i = 0; i < count; ++i)
        {
            hash->hash_table[i] = i;
        }

        // Allocate temporary array to store computed cell values for each index.
        u32 *cell_vals = (u32 *)malloc(sizeof(u32) * count);
        if (!cell_vals)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_vals during update\n");
            return;
        }

        // Compute cell assignment for each new position.
        for (u32 i = 0; i < count; ++i)
        {
            vec4 pos = new_positions[i];
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
            cell_vals[i] = cell_x + cell_y * hash->grid_size_x + cell_z * hash->grid_size_x * hash->grid_size_y;
        }

        // --- Optimized Sorting ---
        quicksort_indices(hash->hash_table, cell_vals, 0, count - 1);

        // Initialize cell_start and cell_end arrays.
        for (u32 i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = 0xFFFFFFFF; // sentinel value.
            hash->cell_end[i] = 0;
        }

        // Populate cell_start and cell_end based on sorted order in hash_table.
        for (u32 i = 0; i < count; ++i)
        {
            u32 idx = hash->hash_table[i];
            u32 cell_index = cell_vals[idx];
            if (hash->cell_start[cell_index] == 0xFFFFFFFF)
            {
                hash->cell_start[cell_index] = i;
            }
            hash->cell_end[cell_index] = i + 1;
        }

        free(cell_vals);
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

        ivec3 cell_coords = get_cell_coordinates(hash, position);
        u32 cell_index = get_cell_index(hash, cell_coords);

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

        for (int x = min_x; x <= max_x; ++x)
        {
            for (int y = min_y; y <= max_y; ++y)
            {
                for (int z = min_z; z <= max_z; ++z)
                {
                    cell_coords = {(u32)x, (u32)y, (u32)z};
                    u32 cell_index = get_cell_index(hash, cell_coords);
                    u32 start = hash->cell_start[cell_index];
                    u32 end = hash->cell_end[cell_index];

                    for (u32 i = start; i < end; i += 8)
                    {
                        u32 remaining = end - i;

                        // Fallback to scalar loop for less than 8 remaining
                        if (remaining < 8)
                        {
                            for (u32 j = 0; j < remaining; ++j)
                            {
                                u32 index = hash->hash_table[i + j];
                                vec4 pos = ((vec4 *)hash->positions)[index]; // safe cast
                                vec4 diff = pos - position;
                                float dist_squared = v4::dot(diff, diff); // assumes dot ignores w
                                if (dist_squared <= radius_sq)
                                {
                                    result_indices[*result_count] = index;
                                    (*result_count)++;
                                }
                            }
                            break;
                        }
                        float *base = (float *)hash->positions; // Reinterpreted as tightly packed floats
                        __m256i idx = _mm256_set_epi32(
                            hash->hash_table[i + 7], hash->hash_table[i + 6],
                            hash->hash_table[i + 5], hash->hash_table[i + 4],
                            hash->hash_table[i + 3], hash->hash_table[i + 2],
                            hash->hash_table[i + 1], hash->hash_table[i + 0]);

                        // Multiply indices by 4 to account for vec4 stride (each vec4 is 4 floats)
                        __m256i idx_scaled = _mm256_slli_epi32(idx, 2); // Shift left by 2 bits (equivalent to *4)

                        // Gather with scale=4 (bytes) to compute correct addresses
                        __m256 pos_x = _mm256_i32gather_ps(base + 0, idx_scaled, 4); // x at offset 0
                        __m256 pos_y = _mm256_i32gather_ps(base + 1, idx_scaled, 4); // y at offset 4
                        __m256 pos_z = _mm256_i32gather_ps(base + 2, idx_scaled, 4); // z at offset 8

                        // Compute distance squared to query point
                        __m256 dx = _mm256_sub_ps(pos_x, _mm256_set1_ps(position.x));
                        __m256 dy = _mm256_sub_ps(pos_y, _mm256_set1_ps(position.y));
                        __m256 dz = _mm256_sub_ps(pos_z, _mm256_set1_ps(position.z));

                        __m256 dist_sq = _mm256_add_ps(
                            _mm256_mul_ps(dx, dx),
                            _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));

                        // Mask for dist² <= radius²
                        __m256 mask = _mm256_cmp_ps(dist_sq, radius_squared, _CMP_LE_OQ);
                        int mask_bits = _mm256_movemask_ps(mask);

                        // Push matching indices
                        for (int j = 0; j < 8; ++j)
                        {
                            if (mask_bits & (1 << j))
                            {
                                result_indices[*result_count] = hash->hash_table[i + j];
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
        vec4 *test_positions = (vec4 *)aligned_alloc(32, sizeof(vec4) * num_positions);

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
        u32 *result_indices = (u32 *)aligned_alloc(32, sizeof(u32) * num_positions);
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

        aligned_free(test_positions);
        aligned_free(result_indices);
        return 1;
    }

} // namespace spatial_hash
