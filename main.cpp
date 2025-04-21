// vulkan_triangle.cpp
// A minimal C-style Vulkan program for Windows that renders a red triangle.
// This version uses hard-coded vertex data from a vertex buffer instead of generating vertices in the shader.

#include "boid_platform.h"
// #include "boid_win32.cpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // for offsetof
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <algorithm>

#include "types.h"

#include "math_linear.h"
#include "io.h"
#include "camera.h"

#include "gl_render.h"

#include "imgui_wrapper.h"

#include "simulation.h"
#include "memory_pool.h"

#include "boid_thread.h"

#include "tracy\public\tracy\Tracy.hpp"
#include "tracy\public\tracy\TracyOpenGL.hpp"

const char *window_class_name = "VulkanWindowClass";
const char *window_title = "Vulkan Red Triangle";

platform::platform_data g_platform_data = {};

u32 g_win_width = 800;
u32 g_win_height = 600;

// Structure for a 2D vertex (screen space coordinates)
typedef struct Vertex
{
    float pos[2]; // X, Y in normalized device coordinates (-1 to 1)
} Vertex;

static inline void draw_axes(f32 line_weight)
{
    ZoneScoped;
    // Draw X axis in red
    bgl::draw_line_ex(line_weight,
                      vec3(0, 0, 0),
                      vec3(1, 0, 0),
                      vec3(1, 0, 0),
                      GL_ALWAYS);

    // Draw Y axis in green
    bgl::draw_line_ex(line_weight,
                      vec3(0, 0, 0),
                      vec3(0, 1, 0),
                      vec3(0, 1, 0),
                      GL_ALWAYS);

    // Draw Z axis in blue
    bgl::draw_line_ex(line_weight,
                      vec3(0, 0, 0),
                      vec3(0, 0, 1),
                      vec3(0, 0, 1),
                      GL_ALWAYS);
}

static inline void draw_grid(f32 line_weight)
{
    ZoneScoped;
    float extents = .5f;
    float spacing = 0.1f;
    vec3 color = vec3(0.5f, 0.5f, 0.5f);
    for (f32 i = -extents; i <= extents; i += spacing)
    {
        // Draw grid lines in XZ plane
        bgl::draw_line(line_weight,
                       vec3(-extents, 0, i),
                       vec3(extents, 0, i),
                       color);
        bgl::draw_line(line_weight,
                       vec3(i, 0, -extents),
                       vec3(i, 0, extents),
                       color);
        // printf("Drawing grid line at %f\n", i);
    }
}

// Function to debug draw the spatial hash grid
void debug_draw_spatial_hash(spatial_hash::spatial_hash *hash, float lineweight, vec3 colour)
{
    if (!hash || hash->cell_size <= 0.0f)
    {
        fprintf(stderr, "Invalid spatial hash or cell size\n");
        return;
    }

    // Iterate through all cells in the spatial hash grid
    for (int x = 0; x < hash->grid_size_x; ++x)
    {
        for (int y = 0; y < hash->grid_size_y; ++y)
        {
            for (int z = 0; z < hash->grid_size_z; ++z)
            {
                // Calculate the world-space coordinates of the cell corners using domain_min and domain_max
                vec3 min_corner = {
                    hash->domain_min.x + x * hash->cell_size,
                    hash->domain_min.y + y * hash->cell_size,
                    hash->domain_min.z + z * hash->cell_size};

                vec3 max_corner = {
                    hash->domain_min.x + (x + 1) * hash->cell_size,
                    hash->domain_min.y + (y + 1) * hash->cell_size,
                    hash->domain_min.z + (z + 1) * hash->cell_size};

                // Draw lines for each edge of the cell, ensuring no duplicates by only drawing edges in positive directions
                bgl::draw_line_ex(lineweight, min_corner, {max_corner.x, min_corner.y, min_corner.z}, colour, GL_ALWAYS); // Bottom edge (X-axis)
                bgl::draw_line_ex(lineweight, min_corner, {min_corner.x, max_corner.y, min_corner.z}, colour, GL_ALWAYS); // Vertical edge (Y-axis)
                bgl::draw_line_ex(lineweight, min_corner, {min_corner.x, min_corner.y, max_corner.z}, colour, GL_ALWAYS); // Depth edge (Z-axis)

                if (x == hash->grid_size_x - 1) // Only draw edges at max X for the last cell in X direction
                {
                    bgl::draw_line_ex(lineweight, {max_corner.x, min_corner.y, min_corner.z}, {max_corner.x, max_corner.y, min_corner.z}, colour, GL_ALWAYS); // Vertical edge at max X
                    bgl::draw_line_ex(lineweight, {max_corner.x, min_corner.y, min_corner.z}, {max_corner.x, min_corner.y, max_corner.z}, colour, GL_ALWAYS); // Depth edge at max X
                }

                if (y == hash->grid_size_y - 1) // Only draw edges at max Y for the last cell in Y direction
                {
                    bgl::draw_line_ex(lineweight, {min_corner.x, max_corner.y, min_corner.z}, {max_corner.x, max_corner.y, min_corner.z}, colour, GL_ALWAYS); // Top edge (X-axis)
                    bgl::draw_line_ex(lineweight, {min_corner.x, max_corner.y, min_corner.z}, {min_corner.x, max_corner.y, max_corner.z}, colour, GL_ALWAYS); // Depth edge at max Y
                }

                if (z == hash->grid_size_z - 1) // Only draw edges at max Z for the last cell in Z direction
                {
                    bgl::draw_line_ex(lineweight, {min_corner.x, min_corner.y, max_corner.z}, {max_corner.x, min_corner.y, max_corner.z}, colour, GL_ALWAYS); // Bottom depth edge (X-axis)
                    bgl::draw_line_ex(lineweight, {min_corner.x, min_corner.y, max_corner.z}, {min_corner.x, max_corner.y, max_corner.z}, colour, GL_ALWAYS); // Vertical edge at max Z
                }
            }
        }
    }
}

// Structure to hold data for the parallel matrix calculation
struct InstMatrixCalcData
{
    mat4 *matrices;                 // Output matrices
    simulation::sim_data *sim_data; // Simulation data
    int start_idx;                  // Start index for this chunk
    int end_idx;                    // End index for this chunk (exclusive)
};

// Worker function for parallel instance matrix calculation
static void calc_matrices_worker(void *data, u32 thread_id, mpool::memory_pool *thread_memory)
{
    ZoneScoped;
    InstMatrixCalcData *calc_data = (InstMatrixCalcData *)data;

    // Process the assigned chunk
    for (int i = calc_data->start_idx; i < calc_data->end_idx; ++i)
    {
        mat4 model_matrix = matrix4::identity();
        mat4 translate = matrix4::mat4_translate(calc_data->sim_data->positions[i].xyz);
        mat4 scale = matrix4::mat4_scale({0.1f, 0.1f, 0.1f});
        mat4 rotation = matrix4::rotate_to(
            calc_data->sim_data->positions[i].xyz,
            calc_data->sim_data->positions[i].xyz + calc_data->sim_data->velocities[i]);

        // Multiply matrices in the order: scale, rotation, translation
        calc_data->matrices[i] = matrix4::mat4_mult(translate, matrix4::mat4_mult(rotation, scale));
    }
}

static void calc_instance_matrices(mat4 *instance_matrices, simulation::sim_data *simulation_data)
{
    ZoneScoped;
#if 1
    // Basic error checking
    if (!instance_matrices || !simulation_data || simulation_data->num_entities <= 0)
    {
        fprintf(stderr, "Error: Invalid inputs to calc_instance_matrices\n");
        return;
    }

    // Only use parallel approach if we have enough entities to justify it
    const int MIN_ENTITIES_FOR_PARALLEL = 1000;
    const int MAX_CHUNKS = 512; // Maximum number of work chunks

    static mpool::memory_pool mem = mpool::allocate(MEGABYTES(1)); // Allocate memory pool for thread data
    mpool::reset(&mem);                                            // Reset the memory pool for thread data

    if (simulation_data->num_entities >= MIN_ENTITIES_FOR_PARALLEL && thread_pool::g_thread_pool != nullptr)
    {
        // Calculate how many chunks we need
        u32 num_threads = thread_pool::g_thread_pool->num_threads;
        int num_chunks = min(num_threads * 8, MAX_CHUNKS);

        // But never use more chunks than entities
        if (num_chunks > simulation_data->num_entities)
        {
            num_chunks = simulation_data->num_entities;
        }

        // Allocate data for each chunk
        InstMatrixCalcData *chunk_data = (InstMatrixCalcData *)mpool::get_bytes(&mem, sizeof(InstMatrixCalcData) * num_chunks);

        // malloc(sizeof(InstMatrixCalcData) * num_chunks);

        // Divide work among chunks
        int base_chunk_size = simulation_data->num_entities / num_chunks;
        int remainder = simulation_data->num_entities % num_chunks;
        int current_start = 0;

        // Submit work to thread pool
        for (int i = 0; i < num_chunks; i++)
        {
            // Calculate chunk size (distribute remainder among first chunks)
            int chunk_size = base_chunk_size + (i < remainder ? 1 : 0);

            // Set up data for this chunk
            chunk_data[i].matrices = instance_matrices;
            chunk_data[i].sim_data = simulation_data;
            chunk_data[i].start_idx = current_start;
            chunk_data[i].end_idx = current_start + chunk_size;

            // Add work to the thread pool
            thread_pool::add_work(calc_matrices_worker, &chunk_data[i]);

            current_start += chunk_size;
        }

        // Wait for all work to complete
        thread_pool::wait_for_completion();

        // Clean up
        return;
    }
#else
    // Process the assigned chunk

    for (int i = 0; i < simulation_data->num_entities; ++i)
    {
        mat4 model_matrix = matrix4::identity();
        mat4 translate = matrix4::mat4_translate(simulation_data->positions[i].xyz);
        mat4 scale = matrix4::mat4_scale({0.1f, 0.1f, 0.1f});
        mat4 rotation = matrix4::rotate_to(
            simulation_data->positions[i].xyz,
            simulation_data->positions[i].xyz + simulation_data->velocities[i]);

        // Multiply matrices in the order: scale, rotation, translation
        instance_matrices[i] = matrix4::mat4_mult(translate, matrix4::mat4_mult(rotation, scale));
    }
#endif
}

// WinMain entry point.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ZoneScoped;
    camera cam = {};
    cam.position = {1, 1, 1};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0, 1, 0};

    cam.distance = 1.0f;

    Mesh bunny = read_mesh("meshes\\bunny.obj");

    g_platform_data.hInstance = hInstance;
    platform::init_window(&g_platform_data, nCmdShow, window_class_name, window_title, g_win_width, g_win_height, WndProc);
    bgl::init(g_platform_data.hwnd, g_win_width, g_win_height);
    imgui_init(g_platform_data.hwnd);
    bgl::line_render_init(100000);

    //  graph_context graph_context = init_im_nodes();

    // uint32_t bunny_id = vk_render_create_mesh(&bunny);

#if 0 
    MSG msg;
#else
    platform::message msg;
#endif
    int quit = 0;
    mat4 view_matrix = matrix4::identity();
    platform::window_rectangle win_rect = get_window_rectangle(&g_platform_data);
    mat4 projection_matrix = matrix4::perspective_matrix(win_rect.width, win_rect.height, 60.0f, 0.1f, 100.0f);

    bgl::gl_mesh *gl_bunny = bgl::add_mesh(&bunny, true);

    Mesh cone = read_mesh("meshes\\cone.obj");
    bgl::gl_mesh *gl_cone = bgl::add_mesh(&cone, false);

    u32 thread_fail = thread_pool::start_thread_pool(14, 256); // Start the thread pool with 4 threads
    if (thread_fail != 0)
    {
        printf("Thread pool failed to start\n\r");
        return -1;
    }
    simulation::sim_data simulation_data = simulation::init_sim(100000, 5.f);

    // register_new_mesh_node(&bunny, "Bunny Mesh");
    // init_mesh_node(&graph_context, &bunny, "Bunny Mesh");
    // register_new_vec3_node();
    // init_vec3_node(&graph_context, {0, 0, 0});

    u64 start_time = platform::get_current_time_ms(); // Initialize the timer
    u64 last_time = start_time;
    float dt_last_ten_frames[10] = {};
    int current_frame_id = 0;
    mpool::memory_pool transient_memory = mpool::allocate(MEGABYTES(50));
    bgl::load_instanced_shaders();

    while (!quit)
    {
#if 0 
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                quit = 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            process_camera_input(&cam, g_hWnd, msg.message, msg.wParam, msg.lParam);
        }
#else
        while (platform::peek_message(&msg))
        {
            if (compare_message(&msg, platform::MSG_QUIT)) // msg.message == WM_QUIT)
                quit = 1;
            platform::translate_message(&msg);
            platform::dispatch_message(&msg);
            process_camera_input(&cam, &g_platform_data, &msg);
        }

#endif
        static f32 dt = 1.0f / 60.f; // Initialize delta time

        if (dt < 0.016f)
        {
            u64 sleep_ms = 16 - (dt * 1000.0f);
            // Sleep(sleep_ms);
            dt = 0.016f;
        }
        static imgui_data ui_data = {0.0, 1.0f, .25, .1f};

        u64 current_time = platform::get_current_time_ms(); // Get the current time
        dt = (f32)(current_time - last_time) / 1000.0f;     // Calculate delta time
        dt_last_ten_frames[current_frame_id++] = dt;        // Store the delta time for the current frame
        current_frame_id %= 10;                             // Wrap around the index to keep it within the last 10 frames
        for (int i = 0; i < 10; i++)
        {
            ui_data.frame_time += dt_last_ten_frames[i]; // Sum the delta times for the last 10 frames
        }
        ui_data.frame_time /= 10.f; // Update frame time in UI data
        // dt = 0.016f;                                  // Reset dt to a fixed value for simulation
        simulation::update_sim(&simulation_data, dt); // Update simulation logic here
        last_time = current_time;                     // Update last time for the next frame

        // vk_render_set_mvp(const float mvp[16]);
        imgui_render(&ui_data);

        draw_axes(.5f);
        draw_grid(.5f);
        // debug_draw_spatial_hash(&simulation_data.search_hash, 0.5f, {0.0f, 1.f, 1.f});
        //  process_and_store_new_links(&graph_context);
        //  update_links(&graph_context); // Update existing links
        u32 nbytes_instances = sizeof(mat4) * simulation_data.num_entities;
        mat4 *instance_matrices = (mat4 *)mpool::get_bytes(&transient_memory, nbytes_instances);

        calc_instance_matrices(instance_matrices, &simulation_data);

        // vk_render_mesh(bunny_id);
        win_rect = platform::get_window_rectangle(&g_platform_data);
        view_matrix = view_matrix_from_cam(&cam);
        // mat4 mvp = mat4_mult(projection_matrix, view_matrix);

        bgl::set_light({0.1f, 0.1f, 0.1f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}, cam.position);

        bgl::set_mvp(view_matrix, projection_matrix, cam);

        bgl::start_draw(win_rect.width, win_rect.height);
        bgl::draw_statics();
        bgl::render_lines();

        bgl::render_instances(gl_cone, instance_matrices, simulation_data.num_entities);

        imgui_end_draw();

        bgl::end_draw();

        mpool::reset(&transient_memory); // Reset the memory pool for the next frame
        FrameMark;
    }
    thread_pool::shutdown_thread_pool(); // Stop the thread pool
    mpool::deallocate(&transient_memory);
    bgl::cleanup();
    imgui_shutdown();
    simulation::free_sim(&simulation_data);
    return 0;
}
