#pragma once
#include "types.h"
#include "math_linear.h"
#include "malloc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "spatial_hash.h"

namespace simulation
{

    enum SIM_COMPONENT_TYPES
    {
        SIM_COMPONENT_SPATIAL = 1 << 0,
        SIM_COMPONENT_BOID = 1 << 1,
    };

    enum BOID_TYPES
    {
        BOID_TYPE_SEEK = 1 << 0,
        BOID_TYPE_FLEE = 1 << 1,
        BOID_TYPE_ALIGN = 1 << 2,
    };

    static float g_max_vel = 0.25f;
    static float g_max_acc = 0.1f;

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
        vec3 *positions;  // Array of entity positions
        vec3 *velocities; // Array of entity velocities

        spatial_hash::spatial_hash search_hash;
        void *search_memory_pool;
    };

    static inline void distribute_boids_random(float extents, sim_data *data)
    {

        for (u32 i = 0; i < data->num_entities; ++i)
        {
            // Assign components to the entity
            data->components[i] = SIM_COMPONENT_SPATIAL | SIM_COMPONENT_BOID;

            // Generate random positions within the extents
            data->positions[i].x = ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->positions[i].y = ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->positions[i].z = ((float)rand() / RAND_MAX) * 2.0f * extents - extents;
            data->behaviours[i] = BOID_TYPE_SEEK | BOID_TYPE_FLEE | BOID_TYPE_ALIGN; // Assign behaviours to the entity
            // Initialize velocities to zero
            data->velocities[i] = {0.0f, 0.0f, 0.0f};
        }
    }

    sim_data init_sim(u64 num_entities)
    {
        bool test_result = spatial_hash::test(); // Test the spatial hash function
        sim_data data = {};
        data.time_step = 0.016f; // 60 FPS
        data.current_time = 0.0f;
        data.num_iterations = 0;
        data.num_entities = num_entities;
        data.components = (u64 *)malloc(sizeof(u64) * num_entities);
        data.behaviours = (u64 *)malloc(sizeof(u64) * num_entities);
        data.positions = (vec3 *)malloc(sizeof(vec3) * num_entities);
        data.velocities = (vec3 *)malloc(sizeof(vec3) * num_entities);
        data.search_memory_pool = malloc(sizeof(vec3) * num_entities); // Allocate memory for the search pool
        memset(data.velocities, 0, sizeof(vec3) * num_entities);       // Initialize velocities to zero
        memset(data.components, 0, sizeof(u64) * num_entities);        // Initialize components to zero
        memset(data.behaviours, 0, sizeof(u64) * num_entities);        // Initialize behaviours to zero
        memset(data.positions, 0, sizeof(vec3) * num_entities);        // Initialize positions to zero

        distribute_boids_random(1.0f, &data);                                      // Distribute boids randomly in the simulation space
        spatial_hash::init(&data.search_hash, .25f, num_entities, data.positions); // Initialize the spatial hash with the positions

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

    static inline vec3 boid_seek(u64 entity_id, float radius, const sim_data *data)
    {

        u32 search_count = 0;
        u32 *search_indices = (u32 *)data->search_memory_pool; // Use the search memory pool for storing results

        spatial_hash::search(&data->search_hash, data->positions[entity_id], radius, search_indices, &search_count);
        vec3 acceleration = {0.0f, 0.0f, 0.0f};
        vec3 current_position = data->positions[entity_id]; // Assume positions array exists in sim_data
                                                            // u32 neighbour_count = 0;
        for (int i = 0; i < search_count; i++)
        {
            vec3 neighbour_position = data->search_hash.positions[search_indices[i]];
            vec3 difference = neighbour_position - current_position;
            float distance_squared = v3::dot(difference, difference);

            if (distance_squared > 0)
            {
                acceleration = acceleration + difference;
            }
        }
        // Average the acceleration vector if there are neighbours
        if (search_count > 0)
        {
            acceleration = acceleration * (1.0f / (float)search_count);
        }

        return acceleration;
    }

    static inline vec3 boid_align(u64 boid_id, float radius, const sim_data *data)
    {

        vec3 average_velocity = {0.0f, 0.0f, 0.0f};
        vec3 current_position = data->positions[boid_id];

        u32 search_count = 0;
        u32 *search_indices = (u32 *)data->search_memory_pool; // Use the search memory pool for storing results

        spatial_hash::search(&data->search_hash, data->positions[boid_id], radius, search_indices, &search_count);

        if (search_count != 0)
        {
            for (int i = 0; i < search_count; i++)
            {
                vec3 neighbour_position = data->search_hash.positions[search_indices[i]];
                vec3 difference = neighbour_position - current_position;
                float distance_squared = v3::dot(difference, difference);

                if (distance_squared > 0)
                {
                    average_velocity = average_velocity + data->velocities[search_indices[i]];
                }
            }
        }

        // Compute the average velocity if there are neighbours
        if (search_count > 0)
        {
            average_velocity = average_velocity * (1.0f / (float)search_count);
            // Return the alignment vector (difference from current velocity)
            return average_velocity - data->velocities[boid_id];
        }

        // No neighbours, return zero vector
        return {0.0f, 0.0f, 0.0f};
    }

    void update_sim(sim_data *data, float delta_time)
    {
        // Update simulation logic here
        data->current_time += delta_time;
        data->num_iterations++;

        for (u32 i = 0; i < data->num_entities; ++i)
        {
            u64 entity_components = data->components[i];
            u64 entity_behaviours = data->behaviours[i];
            vec3 acceleration = {0.0f, 0.0f, 0.0f};
            // Check if the entity has a transform component
            if (entity_components & SIM_COMPONENT_SPATIAL)
            {

                // Apply boid behaviours based on the entity's behaviour flags
                if (entity_behaviours & BOID_TYPE_SEEK)
                {
                    acceleration = acceleration + boid_seek(i, .25f, data);
                }

                if (entity_behaviours & BOID_TYPE_FLEE)
                {
                    acceleration = acceleration - boid_seek(i, .15f, data);
                }

                if (entity_behaviours & BOID_TYPE_ALIGN)
                {
                    acceleration = acceleration + boid_align(i, .1f, data);
                }
            }
            acceleration = v3::clamp(acceleration, g_max_acc);                          // Clamp acceleration to max value
            data->velocities[i] = data->velocities[i] + acceleration * delta_time;      // Update velocity
            data->velocities[i] = v3::clamp(data->velocities[i], g_max_vel);            // Clamp velocity to max value
            data->positions[i] = data->positions[i] + data->velocities[i] * delta_time; // Update position
        }

        spatial_hash::update(&data->search_hash, data->positions, data->num_entities); // Update the spatial hash with new positions
    }

};