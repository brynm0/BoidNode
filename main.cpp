// vulkan_triangle.cpp
// A minimal C-style Vulkan program for Windows that renders a red triangle.
// This version uses hard-coded vertex data from a vertex buffer instead of generating vertices in the shader.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#include "gl_render.h"

#include "imgui_wrapper.h"

#include "simulation.h"

// Global declarations for the Win32 window.
HINSTANCE g_hInstance;
HWND g_hWnd;

const char *WINDOW_CLASS_NAME = "VulkanWindowClass";
const char *WINDOW_TITLE = "Vulkan Red Triangle";

u32 g_win_width = 800;
u32 g_win_height = 600;

// Structure for a 2D vertex (screen space coordinates)
typedef struct Vertex
{
    float pos[2]; // X, Y in normalized device coordinates (-1 to 1)
} Vertex;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window procedure for Win32.
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Let ImGui process Windows messages first
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 0; // Or return 0, depending on your application's needs

    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// Function to create a Win32 window.
void InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    RECT rect = {0, 0, (LONG)g_win_width, (LONG)g_win_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow((LPCSTR)WINDOW_CLASS_NAME, (LPCSTR)WINDOW_TITLE,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          rect.right - rect.left, rect.bottom - rect.top,
                          NULL, NULL, hInstance, NULL);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
}

struct window_rectangle
{
    int width;
    int height;
};

window_rectangle get_window_rectangle(HWND hwnd)
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    window_rectangle win_rect;
    win_rect.width = rect.right - rect.left;
    win_rect.height = rect.bottom - rect.top;
    return win_rect;
}

// Function to get the current time in seconds since the program started running.
u64 get_current_time_ms()
{
    static LARGE_INTEGER frequency = {0};
    static LARGE_INTEGER start_time = {0};

    // Initialize the frequency and start time on the first call
    if (frequency.QuadPart == 0)
    {
        if (!QueryPerformanceFrequency(&frequency))
        {
            // If QueryPerformanceFrequency fails, return 0 as an error case
            return 0;
        }
        QueryPerformanceCounter(&start_time);
    }

    LARGE_INTEGER current_time;
    if (!QueryPerformanceCounter(&current_time))
    {
        // If QueryPerformanceCounter fails, return 0 as an error case
        return 0;
    }

    // Calculate the elapsed time in milliseconds
    u64 elapsed_time_ms = (u64)(((current_time.QuadPart - start_time.QuadPart) * 1000) / frequency.QuadPart);
    return elapsed_time_ms;
}

static inline void draw_axes(f32 line_weight)
{

    // Draw X axis in red
    draw_line_ex(line_weight,
                 vec3(0, 0, 0),
                 vec3(1, 0, 0),
                 vec3(1, 0, 0),
                 GL_ALWAYS);

    // Draw Y axis in green
    draw_line_ex(line_weight,
                 vec3(0, 0, 0),
                 vec3(0, 1, 0),
                 vec3(0, 1, 0),
                 GL_ALWAYS);

    // Draw Z axis in blue
    draw_line_ex(line_weight,
                 vec3(0, 0, 0),
                 vec3(0, 0, 1),
                 vec3(0, 0, 1),
                 GL_ALWAYS);
}

static inline void draw_grid(f32 line_weight)
{
    float extents = .5f;
    float spacing = 0.1f;
    vec3 color = vec3(0.5f, 0.5f, 0.5f);
    for (f32 i = -extents; i <= extents; i += spacing)
    {
        // Draw grid lines in XZ plane
        draw_line(line_weight,
                  vec3(-extents, 0, i),
                  vec3(extents, 0, i),
                  color);
        draw_line(line_weight,
                  vec3(i, 0, -extents),
                  vec3(i, 0, extents),
                  color);
        // printf("Drawing grid line at %f\n", i);
    }
}

// WinMain entry point.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

    camera cam = {};
    cam.position = {1, 1, 1};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0, 1, 0};

    cam.distance = 1.0f;

    Mesh bunny = read_mesh("meshes\\bunny.obj");

    g_hInstance = hInstance;
    InitWindow(hInstance, nCmdShow);
    gl_render_init(g_hWnd, g_win_width, g_win_height);
    imgui_init(g_hWnd);
    gl_line_render_init(10000);

    //  graph_context graph_context = init_im_nodes();

    // uint32_t bunny_id = vk_render_create_mesh(&bunny);
    MSG msg;
    int quit = 0;
    mat4 view_matrix = mat4_identity();
    window_rectangle win_rect = get_window_rectangle(g_hWnd);
    mat4 projection_matrix = perspective_matrix(win_rect.width, win_rect.height, 60.0f, 0.1f, 100.0f);

    gl_mesh *mesh = gl_render_add_mesh(&bunny);

    simulation::sim_data simulation_data = simulation::init_sim(1000);

    // register_new_mesh_node(&bunny, "Bunny Mesh");
    // init_mesh_node(&graph_context, &bunny, "Bunny Mesh");
    // register_new_vec3_node();
    // init_vec3_node(&graph_context, {0, 0, 0});

    u64 start_time = get_current_time_ms(); // Initialize the timer
    u64 last_time = start_time;
    float dt_last_ten_frames[10] = {};
    int current_frame_id = 0;
    while (!quit)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                quit = 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            process_camera_input(&cam, g_hWnd, msg.message, msg.wParam, msg.lParam);
        }
        static f32 dt = 1.0f / 60.f; // Initialize delta time

        if (dt < 0.016f)
        {
            u64 sleep_ms = 16 - (dt * 1000.0f);
            Sleep(sleep_ms);
            dt = 0.016f;
            printf("Slept for %llu ms\n", sleep_ms);
        }
        static imgui_data ui_data = {0.0, 1.0f, .25, .1f};

        u64 current_time = get_current_time_ms();       // Get the current time
        dt = (f32)(current_time - last_time) / 1000.0f; // Calculate delta time
        dt_last_ten_frames[current_frame_id++] = dt;    // Store the delta time for the current frame
        current_frame_id %= 10;                         // Wrap around the index to keep it within the last 10 frames
        for (int i = 0; i < 10; i++)
        {
            ui_data.frame_time += dt_last_ten_frames[i]; // Sum the delta times for the last 10 frames
        }
        ui_data.frame_time /= 10.f;                   // Update frame time in UI data
        simulation::update_sim(&simulation_data, dt); // Update simulation logic here
        last_time = current_time;                     // Update last time for the next frame

        // vk_render_set_mvp(const float mvp[16]);
        imgui_render(&ui_data);

        draw_axes(.5f);
        draw_grid(.5f);
        //  process_and_store_new_links(&graph_context);
        //  update_links(&graph_context); // Update existing links
        for (int i = 0; i < simulation_data.num_entities; ++i)
        {
            vec3 position = simulation_data.positions[i];
            vec3 vel = simulation_data.velocities[i];
            vec3 color = {0.5f, 0.5f, 0.5f};
            draw_line(.25f, position, position + vel * ui_data.boid_trail_len, color);
            // draw_sphere(.25f, position, color);
        }

        // vk_render_mesh(bunny_id);
        win_rect = get_window_rectangle(g_hWnd);
        view_matrix = view_matrix_from_cam(&cam);
        // mat4 mvp = mat4_mult(projection_matrix, view_matrix);

        set_light({0.1f, 0.1f, 0.1f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}, cam.position);

        gl_render_set_mvp(view_matrix, projection_matrix, cam);

        gl_render_draw(win_rect.width, win_rect.height);
        // if (dt < 0.016f)
        // {
        //     Sleep(16 - (current_time - last_time)); // Sleep to maintain a consistent frame rate
        //     printf("Slept for %llu ms\n", 16 - (current_time - last_time));

        //     last_time = get_current_time_ms(); // Update last time for the next frame
        // }
        // else
        // {
        //     last_time = current_time; // Update last time for the next frame
        // }
    }
    gl_render_cleanup();
    imgui_shutdown();
    simulation::free_sim(&simulation_data);
    return 0;
}
