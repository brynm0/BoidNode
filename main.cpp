#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // for offsetof
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "types.h"
#include "math_linear.h"
#include "io.h"
#include "camera.h"
#include "platform.h" // Include our platform header instead of directly including gl_render.h
#include "imgui_wrapper.h"
#include "simulation.h"
#include "memory_pool.h"

// Global variables
const char *WINDOW_TITLE = "Boid Node Simulation";
u32 g_win_width = 800;
u32 g_win_height = 600;
PlatformState g_platform_state;

// Structure for a 2D vertex (screen space coordinates)
typedef struct Vertex
{
    float pos[2]; // X, Y in normalized device coordinates (-1 to 1)
} Vertex;

struct window_rectangle
{
    int width;
    int height;
};

// Platform-independent function to get window dimensions
window_rectangle get_window_rectangle()
{
    window_rectangle win_rect;
#if PLATFORM_WINDOWS
    RECT rect;
    GetClientRect((HWND)g_platform_state.hwnd, &rect);
    win_rect.width = rect.right - rect.left;
    win_rect.height = rect.bottom - rect.top;
#elif PLATFORM_MACOS
    // For macOS, we can get the window size from the platform state
    win_rect.width = g_platform_state.width;
    win_rect.height = g_platform_state.height;
#endif
    return win_rect;
}

static inline void draw_axes(f32 line_weight)
{
    // Draw X axis in red
    render_draw_line_ex(line_weight,
                      vec3(0, 0, 0),
                      vec3(1, 0, 0),
                      vec3(1, 0, 0),
#if PLATFORM_WINDOWS
                      GL_ALWAYS
#elif PLATFORM_MACOS
                      VK_COMPARE_OP_ALWAYS
#endif
                      );

    // Draw Y axis in green
    render_draw_line_ex(line_weight,
                      vec3(0, 0, 0),
                      vec3(0, 1, 0),
                      vec3(0, 1, 0),
#if PLATFORM_WINDOWS
                      GL_ALWAYS
#elif PLATFORM_MACOS
                      VK_COMPARE_OP_ALWAYS
#endif
                      );

    // Draw Z axis in blue
    render_draw_line_ex(line_weight,
                      vec3(0, 0, 0),
                      vec3(0, 0, 1),
                      vec3(0, 0, 1),
#if PLATFORM_WINDOWS
                      GL_ALWAYS
#elif PLATFORM_MACOS
                      VK_COMPARE_OP_ALWAYS
#endif
                      );
}

static inline void draw_grid(f32 line_weight)
{
    float extents = .5f;
    float spacing = 0.1f;
    vec3 color = vec3(0.5f, 0.5f, 0.5f);
    for (f32 i = -extents; i <= extents; i += spacing)
    {
        // Draw grid lines in XZ plane
        render_draw_line(line_weight,
                       vec3(-extents, 0, i),
                       vec3(extents, 0, i),
                       color);
        render_draw_line(line_weight,
                       vec3(i, 0, -extents),
                       vec3(i, 0, extents),
                       color);
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
                render_draw_line_ex(lineweight, min_corner, {max_corner.x, min_corner.y, min_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                  GL_ALWAYS
#elif PLATFORM_MACOS
                                  VK_COMPARE_OP_ALWAYS
#endif
                                  ); // Bottom edge (X-axis)
                render_draw_line_ex(lineweight, min_corner, {min_corner.x, max_corner.y, min_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                  GL_ALWAYS
#elif PLATFORM_MACOS
                                  VK_COMPARE_OP_ALWAYS
#endif
                                  ); // Vertical edge (Y-axis)
                render_draw_line_ex(lineweight, min_corner, {min_corner.x, min_corner.y, max_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                  GL_ALWAYS
#elif PLATFORM_MACOS
                                  VK_COMPARE_OP_ALWAYS
#endif
                                  ); // Depth edge (Z-axis)

                if (x == hash->grid_size_x - 1) // Only draw edges at max X for the last cell in X direction
                {
                    render_draw_line_ex(lineweight, {max_corner.x, min_corner.y, min_corner.z}, {max_corner.x, max_corner.y, min_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                      GL_ALWAYS
#elif PLATFORM_MACOS
                                      VK_COMPARE_OP_ALWAYS
#endif
                                      ); // Vertical edge at max X
                    render_draw_line_ex(lineweight, {max_corner.x, min_corner.y, min_corner.z}, {max_corner.x, min_corner.y, max_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                      GL_ALWAYS
#elif PLATFORM_MACOS
                                      VK_COMPARE_OP_ALWAYS
#endif
                                      ); // Depth edge at max X
                }

                if (y == hash->grid_size_y - 1) // Only draw edges at max Y for the last cell in Y direction
                {
                    render_draw_line_ex(lineweight, {min_corner.x, max_corner.y, min_corner.z}, {max_corner.x, max_corner.y, min_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                      GL_ALWAYS
#elif PLATFORM_MACOS
                                      VK_COMPARE_OP_ALWAYS
#endif
                                      ); // Top edge (X-axis)
                    render_draw_line_ex(lineweight, {min_corner.x, max_corner.y, min_corner.z}, {min_corner.x, max_corner.y, max_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                      GL_ALWAYS
#elif PLATFORM_MACOS
                                      VK_COMPARE_OP_ALWAYS
#endif
                                      ); // Depth edge at max Y
                }

                if (z == hash->grid_size_z - 1) // Only draw edges at max Z for the last cell in Z direction
                {
                    render_draw_line_ex(lineweight, {min_corner.x, min_corner.y, max_corner.z}, {max_corner.x, min_corner.y, max_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                      GL_ALWAYS
#elif PLATFORM_MACOS
                                      VK_COMPARE_OP_ALWAYS
#endif
                                      ); // Bottom depth edge (X-axis)
                    render_draw_line_ex(lineweight, {min_corner.x, min_corner.y, max_corner.z}, {min_corner.x, max_corner.y, max_corner.z}, colour, 
#if PLATFORM_WINDOWS
                                      GL_ALWAYS
#elif PLATFORM_MACOS
                                      VK_COMPARE_OP_ALWAYS
#endif
                                      ); // Vertical edge at max Z
                }
            }
        }
    }
}

#if PLATFORM_WINDOWS
// WinMain entry point for Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize Win32 window
    platform_init(&g_platform_state, g_win_width, g_win_height, WINDOW_TITLE);
    void* window_handle = g_platform_state.hwnd;
#elif PLATFORM_MACOS
// main entry point for macOS
int main(int argc, char* argv[])
{
    // Initialize macOS window using GLFW (handled in platform_init)
    platform_init(&g_platform_state, g_win_width, g_win_height, WINDOW_TITLE);
    void* window_handle = g_platform_state.window;
#endif

    camera cam = {};
    cam.position = {1, 1, 1};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0, 1, 0};
    cam.distance = 1.0f;

    Mesh bunny = read_mesh("meshes/bunny.obj");

    // Initialize renderer
    render_init(window_handle, g_win_width, g_win_height);
    
#if PLATFORM_WINDOWS
    imgui_init((HWND)window_handle);
#elif PLATFORM_MACOS
    imgui_init((GLFWwindow*)window_handle);
#endif

    render_line_init(100000);

    // Setup matrices
    mat4 view_matrix = matrix4::identity();
    window_rectangle win_rect = get_window_rectangle();
    mat4 projection_matrix = matrix4::perspective_matrix(win_rect.width, win_rect.height, 60.0f, 0.1f, 100.0f);

    // Add meshes
    RenderMesh *render_bunny = render_add_mesh(&bunny, true);
    Mesh cone = read_mesh("meshes/cone.obj");
    RenderMesh *render_cone = render_add_mesh(&cone, false);

    // Initialize simulation
    simulation::sim_data simulation_data = simulation::init_sim(5000);

    // Set up timing
    double start_time = platform_get_time();
    double last_time = start_time;
    float dt_last_ten_frames[10] = {};
    int current_frame_id = 0;
    
    // Memory pool for transient allocations
    mpool::memory_pool transient_memory = mpool::allocate(MEGABYTES(50));

    // Main loop
    bool quit = false;
    while (!quit)
    {
        // Process messages
        quit = !platform_process_messages(&g_platform_state);
        if (quit) break;

        // Calculate delta time
        double current_time = platform_get_time();
        float dt = (float)(current_time - last_time);
        
        // Maintain minimum frame rate
        if (dt < 0.016f)
        {
            // Sleep to maintain 60 fps
            dt = 0.016f;
        }
        
        // ImGui data
        static imgui_data ui_data = {0.0, 1.0f, .25, .1f};

        // Update frame timing
        dt_last_ten_frames[current_frame_id++] = dt;
        current_frame_id %= 10;
        ui_data.frame_time = 0.0f;
        for (int i = 0; i < 10; i++)
        {
            ui_data.frame_time += dt_last_ten_frames[i];
        }
        ui_data.frame_time /= 10.f;
        
        // Update simulation
        simulation::update_sim(&simulation_data, dt);
        last_time = current_time;

        // Render ImGui
        imgui_render(&ui_data);

        // Draw axes and grid
        draw_axes(.5f);
        draw_grid(.5f);

        // Create instance matrices for boids
        u32 nbytes_instances = sizeof(mat4) * simulation_data.num_entities;
        mat4 *instance_matrices = (mat4 *)mpool::get_bytes(&transient_memory, nbytes_instances);
        for (int i = 0; i < simulation_data.num_entities; ++i)
        {
            mat4 model_matrix = matrix4::identity();
            mat4 translate = matrix4::mat4_translate(simulation_data.positions[i].xyz);
            mat4 scale = matrix4::mat4_scale({0.1f, 0.1f, 0.1f});
            mat4 rotation = matrix4::rotate_to(simulation_data.positions[i].xyz, simulation_data.positions[i].xyz + simulation_data.velocities[i]);
            
            // Multiply matrices: model = translate * rotate * scale
            model_matrix = matrix4::mat4_mult(translate, matrix4::mat4_mult(rotation, scale));
            instance_matrices[i] = model_matrix;
        }

        // Update window size and matrices
        win_rect = get_window_rectangle();
        view_matrix = view_matrix_from_cam(&cam);

        // Set lighting and camera
        render_set_light({0.1f, 0.1f, 0.1f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}, cam.position);
        render_set_mvp(view_matrix, projection_matrix, cam);

        // Render frame
        render_start_draw(win_rect.width, win_rect.height);
        render_draw_statics();
        render_lines();
        render_instances(render_cone, instance_matrices, simulation_data.num_entities);
        imgui_end_draw();
        render_end_draw();

        // Reset memory pool for next frame
        mpool::reset(&transient_memory);
    }

    // Cleanup
    mpool::deallocate(&transient_memory);
    render_cleanup();
    imgui_shutdown();
    simulation::free_sim(&simulation_data);
    platform_cleanup(&g_platform_state);
    
    return 0;
}
