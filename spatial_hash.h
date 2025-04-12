#include "types.h"
#include "math_linear.h"
#include "malloc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <vector>
namespace spatial_hash
{

    // Spatial hash structure updated to support an arbitrary domain.
    typedef struct spatial_hash
    {
        vec3 *positions; // Original positions.
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
        vec3 domain_min; // Minimum coordinate across all positions.
        vec3 domain_max; // Maximum coordinate across all positions.
    } spatial_hash;

    // Helper function to compute the domain (min and max) of the positions.
    // It computes the axis-aligned bounding box for the input positions.
    static inline int compute_domain(const u32 num_positions, const vec3 *positions, vec3 *out_min, vec3 *out_max)
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
            vec3 pos = positions[i];
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

    // Initializes the spatial hash structure.
    // The "max_radius" value is used as the cell size.
    // The domain is computed automatically from the provided positions.
    static inline void init(spatial_hash *hash, float max_radius, u32 num_positions, const vec3 *initial_positions)
    {
        if (!hash || max_radius <= 0.0f || num_positions == 0 || !initial_positions)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash initialization\n");
            return;
        }

        hash->cell_size = max_radius;
        hash->num_positions = num_positions;

        // Allocate memory for positions and the hash table.
        hash->positions = (vec3 *)malloc(sizeof(vec3) * num_positions);
        hash->hash_table = (u32 *)malloc(sizeof(u32) * num_positions);

        hash->entity_ids = (u64 *)malloc(sizeof(u64) * num_positions);
        // We will compute the domain first to determine the grid dimensions.
        if (!hash->positions || !hash->hash_table)
        {
            fprintf(stderr, "Error: Memory allocation failed for positions or hash_table\n");
            return;
        }

        // Copy the original positions.
        memcpy(hash->positions, initial_positions, sizeof(vec3) * num_positions);

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

        // Temporary array to store computed cell values for each index.
        u32 *cell_vals = (u32 *)malloc(sizeof(u32) * num_positions);
        if (!cell_vals)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_vals\n");
            return;
        }

        // Compute cell assignment for each position.
        for (u32 i = 0; i < num_positions; ++i)
        {
            vec3 pos = initial_positions[i];
            // Shift position by the domain minimum so that the domain starts at zero.
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

        // --- Sort hash_table by the associated cell value (simple bubble sort) ---
        for (u32 i = 0; i < num_positions; ++i)
        {
            for (u32 j = i + 1; j < num_positions; ++j)
            {
                if (cell_vals[hash->hash_table[i]] > cell_vals[hash->hash_table[j]])
                {
                    // Swap indices in hash_table.
                    u32 temp = hash->hash_table[i];
                    hash->hash_table[i] = hash->hash_table[j];
                    hash->hash_table[j] = temp;
                }
            }
        }

        free(cell_vals);

        // Initialize cell_start and cell_end arrays.
        for (u32 i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = 0xFFFFFFFF; // sentinel value.
            hash->cell_end[i] = 0;
        }

        // Populate cell_start and cell_end based on sorted order in hash_table.
        for (u32 i = 0; i < num_positions; ++i)
        {
            // Retrieve the position index.
            u32 idx = hash->hash_table[i];
            vec3 pos = initial_positions[idx];
            // Shift the position.
            float shifted_x = pos.x - hash->domain_min.x;
            float shifted_y = pos.y - hash->domain_min.y;
            float shifted_z = pos.z - hash->domain_min.z;

            // Compute cell coordinates.
            u32 cell_x = (u32)fmaxf(shifted_x / hash->cell_size, 0.0f);
            u32 cell_y = (u32)fmaxf(shifted_y / hash->cell_size, 0.0f);
            u32 cell_z = (u32)fmaxf(shifted_z / hash->cell_size, 0.0f);
            cell_x = (cell_x < hash->grid_size_x) ? cell_x : hash->grid_size_x - 1;
            cell_y = (cell_y < hash->grid_size_y) ? cell_y : hash->grid_size_y - 1;
            cell_z = (cell_z < hash->grid_size_z) ? cell_z : hash->grid_size_z - 1;
            u32 cell_index = cell_x + cell_y * hash->grid_size_x + cell_z * hash->grid_size_x * hash->grid_size_y;
            if (hash->cell_start[cell_index] == 0xFFFFFFFF)
            {
                hash->cell_start[cell_index] = i;
            }
            hash->cell_end[cell_index] = i + 1;
        }
    }

    // Updates the spatial hash with new positions.
    // Replaces the positions and re-bins them into cells.
    // May recalculate domain and reallocate total number of cells.
    static inline void update(spatial_hash *hash, vec3 *new_positions, u32 count)
    {
        if (!hash || !new_positions || count == 0)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash update\n");
            return;
        }

        // Free existing memory for positions and hash table.
        free(hash->positions);
        free(hash->hash_table);
        free(hash->cell_start);
        free(hash->cell_end);

        // Update the number of positions.
        hash->num_positions = count;

        // Allocate memory for new positions and hash table.
        hash->positions = (vec3 *)malloc(sizeof(vec3) * count);
        hash->hash_table = (u32 *)malloc(sizeof(u32) * count);

        if (!hash->positions || !hash->hash_table)
        {
            fprintf(stderr, "Error: Memory allocation failed for positions or hash_table during update\n");
            return;
        }

        // Copy the new positions.
        memcpy(hash->positions, new_positions, sizeof(vec3) * count);

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

        // Temporary array to store computed cell values for each index.
        u32 *cell_vals = (u32 *)malloc(sizeof(u32) * count);
        if (!cell_vals)
        {
            fprintf(stderr, "Error: Memory allocation failed for cell_vals during update\n");
            return;
        }

        // Compute cell assignment for each position.
        for (u32 i = 0; i < count; ++i)
        {
            vec3 pos = new_positions[i];
            // Shift position by the domain minimum so that the domain starts at zero.
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

        // --- Sort hash_table by the associated cell value (simple bubble sort) ---
        for (u32 i = 0; i < count; ++i)
        {
            for (u32 j = i + 1; j < count; ++j)
            {
                if (cell_vals[hash->hash_table[i]] > cell_vals[hash->hash_table[j]])
                {
                    // Swap indices in hash_table.
                    u32 temp = hash->hash_table[i];
                    hash->hash_table[i] = hash->hash_table[j];
                    hash->hash_table[j] = temp;
                }
            }
        }

        free(cell_vals);

        // Initialize cell_start and cell_end arrays.
        for (u32 i = 0; i < num_cells; ++i)
        {
            hash->cell_start[i] = 0xFFFFFFFF; // sentinel value.
            hash->cell_end[i] = 0;
        }

        // Populate cell_start and cell_end based on sorted order in hash_table.
        for (u32 i = 0; i < count; ++i)
        {
            // Retrieve the position index.
            u32 idx = hash->hash_table[i];
            vec3 pos = new_positions[idx];
            // Shift the position.
            float shifted_x = pos.x - hash->domain_min.x;
            float shifted_y = pos.y - hash->domain_min.y;
            float shifted_z = pos.z - hash->domain_min.z;

            // Compute cell coordinates.
            u32 cell_x = (u32)fmaxf(shifted_x / hash->cell_size, 0.0f);
            u32 cell_y = (u32)fmaxf(shifted_y / hash->cell_size, 0.0f);
            u32 cell_z = (u32)fmaxf(shifted_z / hash->cell_size, 0.0f);
            cell_x = (cell_x < hash->grid_size_x) ? cell_x : hash->grid_size_x - 1;
            cell_y = (cell_y < hash->grid_size_y) ? cell_y : hash->grid_size_y - 1;
            cell_z = (cell_z < hash->grid_size_z) ? cell_z : hash->grid_size_z - 1;
            u32 cell_index = cell_x + cell_y * hash->grid_size_x + cell_z * hash->grid_size_x * hash->grid_size_y;
            if (hash->cell_start[cell_index] == 0xFFFFFFFF)
            {
                hash->cell_start[cell_index] = i;
            }
            hash->cell_end[cell_index] = i + 1;
        }
    }

    // Searches for positions within a given radius of the query position.
    // The query position is shifted by the domain minimum just like the stored positions.
    static inline void search(const spatial_hash *hash, vec3 position, float radius, u32 *result_indices, u32 *result_count)
    {
        if (!hash || !result_indices || !result_count || radius <= 0.0f)
        {
            fprintf(stderr, "Error: Invalid parameters for spatial hash search\n");
            return;
        }

        *result_count = 0;
        float inv_cell_size = 1.0f / hash->cell_size;

        // Adjust query position by the domain minimum.
        float query_x = position.x - hash->domain_min.x;
        float query_y = position.y - hash->domain_min.y;
        float query_z = position.z - hash->domain_min.z;

        // Compute cell boundaries (in cell coordinates) for the search sphere.
        float min_x_f = (query_x - radius) * inv_cell_size;
        float min_y_f = (query_y - radius) * inv_cell_size;
        float min_z_f = (query_z - radius) * inv_cell_size;
        float max_x_f = (query_x + radius) * inv_cell_size;
        float max_y_f = (query_y + radius) * inv_cell_size;
        float max_z_f = (query_z + radius) * inv_cell_size;

        // Clamp the boundaries to the grid dimensions.
        int min_x = (int)fmaxf(floorf(min_x_f), 0.0f);
        int min_y = (int)fmaxf(floorf(min_y_f), 0.0f);
        int min_z = (int)fmaxf(floorf(min_z_f), 0.0f);
        int max_x = (int)fminf(ceilf(max_x_f), (float)(hash->grid_size_x - 1));
        int max_y = (int)fminf(ceilf(max_y_f), (float)(hash->grid_size_y - 1));
        int max_z = (int)fminf(ceilf(max_z_f), (float)(hash->grid_size_z - 1));

        // Iterate over the cells within the computed boundaries.
        for (int x = min_x; x <= max_x; ++x)
        {
            for (int y = min_y; y <= max_y; ++y)
            {
                for (int z = min_z; z <= max_z; ++z)
                {
                    u32 cell_index = x + y * hash->grid_size_x + z * hash->grid_size_x * hash->grid_size_y;
                    // For each index in this cell, check the actual distance.
                    for (u32 i = hash->cell_start[cell_index]; i < hash->cell_end[cell_index]; ++i)
                    {
                        u32 pos_index = hash->hash_table[i];
                        vec3 diff = hash->positions[pos_index] - position;
                        float dist_squared = v3::dot(diff, diff);
                        if (dist_squared <= radius * radius)
                        {
                            result_indices[*result_count] = pos_index;
                            (*result_count)++;
                        }
                    }
                }
            }
        }
    }

        // Test function to validate the spatial hash.
    // Note: The test positions are in the [0,1] range, but the code now supports arbitrary domains.
    static inline int test()
    {
        // Define test parameters.
        const float max_radius = 0.1f;
        const u32 num_positions = 5;
        vec3 test_positions[num_positions] = {
            {0.05f, -0.05f, 0.05f},
            {-.15f, 0.15f, 0.15f},
            {0.25f, 15.25f, 0.25f},
            {0.35f, 0.35f, -.35f},
            {0.45f, 0.45f, 0.45f}};

        spatial_hash hash = {0};
        init(&hash, max_radius, num_positions, test_positions);

        // Test the search functionality.
        vec3 search_position = {0.1f, 0.1f, 0.1f};
        float search_radius = 0.2f;
        u32 result_indices[num_positions];
        u32 result_count = 0;

        search(&hash, search_position, search_radius, result_indices, &result_count);
        u32 ground_truth_result_count = 0;
        std::vector<u32> gt_results;
        gt_results.reserve(num_positions);
        for (int i = 0; i < num_positions; i++)
        {
            vec3 diff = test_positions[i] - search_position;
            float dist_squared = v3::dot(diff, diff);
            if (dist_squared <= search_radius * search_radius)
            {
                ground_truth_result_count++;
                gt_results.push_back(i); // Store the index of the position that is within the search radius.
            }
        }

        // For these test positions, we expect positions with original indices 0 and 1.
        if (result_count != ground_truth_result_count)
        {
            fprintf(stderr, "Error: Expected %u results, got %u\n", ground_truth_result_count, result_count);
            return 0;
        }

        for (int i = 0; i < result_count; i++)
        {
            // Check if the result index is in the ground truth results.
            if (std::find(gt_results.begin(), gt_results.end(), result_indices[i]) == gt_results.end())
            {
                fprintf(stderr, "Error: Unexpected result index %u\n", result_indices[i]);
                return 0;
            }
        }

        // Clean up allocated memory.
        free(hash.positions);
        free(hash.hash_table);
        free(hash.cell_start);
        free(hash.cell_end);

        return 1;
    }

} // namespace spatial_hash
