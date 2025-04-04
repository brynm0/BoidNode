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

#include "io.h"
#include "math_linear.h"
#include "camera.h"

#include "gl_render.h"

// Global declarations for the Win32 window.
HINSTANCE g_hInstance;
HWND g_hWnd;
const char *WINDOW_CLASS_NAME = "VulkanWindowClass";
const char *WINDOW_TITLE = "Vulkan Red Triangle";

// Structure for a 2D vertex (screen space coordinates)
typedef struct Vertex
{
    float pos[2]; // X, Y in normalized device coordinates (-1 to 1)
} Vertex;

// Window procedure for Win32.
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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

    RECT rect = {0, 0, 800, 600};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(WINDOW_CLASS_NAME, WINDOW_TITLE,
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

// WinMain entry point.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

    camera cam = {};
    cam.position = {1, 0, 0};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0, 1, 0};

    cam.distance = 1.0f;

    Mesh bunny = read_mesh("meshes\\bunny.obj");

    g_hInstance = hInstance;
    InitWindow(hInstance, nCmdShow);
    gl_render_init(g_hWnd, 800, 600);

    gl_render_set_mesh(&bunny);
    // uint32_t bunny_id = vk_render_create_mesh(&bunny);
    MSG msg;
    int quit = 0;
    mat4 view_matrix = mat4_identity();
    window_rectangle win_rect = get_window_rectangle(g_hWnd);
    mat4 projection_matrix = perspective_matrix(win_rect.width, win_rect.height, 60.0f, 0.1f, 100.0f);
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
        // vk_render_mesh(bunny_id);

        view_matrix = view_matrix_from_cam(&cam);
        mat4 mvp = mat4_mult(projection_matrix, view_matrix);

        gl_render_set_mvp(mvp);
        gl_render_draw();

        printf("cam.position: %f %f %f\n", cam.position.x, cam.position.y, cam.position.z);

        // vk_render_set_mvp(const float mvp[16]);
    }

    gl_render_cleanup();

    return 0;
}
