#pragma once

// Include platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #include <windows.h>
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
    #include <GLFW/glfw3.h>
#endif

struct imgui_data
{
    float frame_time;
    float boid_trail_len;
    float boid_max_vel;
    float boid_max_acc;
};

// Forward declarations for functions moved to imgui_wrapper.cpp
#if PLATFORM_WINDOWS
void imgui_init(HWND hwnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#elif PLATFORM_MACOS
void imgui_init(GLFWwindow* window);
#endif

void imgui_shutdown();
void basic_ui(imgui_data *data);
void imgui_render(imgui_data *data);
void imgui_end_draw();
