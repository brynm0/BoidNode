#include "windows.h"

struct imgui_data
{
    float frame_time;
    float boid_trail_len;
    float boid_max_vel;
    float boid_max_acc;
};

// Forward declarations for functions moved to imgui_wrapper.cpp
void imgui_init(void *window_handle);
void imgui_shutdown();
void basic_ui(imgui_data *data);
void imgui_render(imgui_data *data);
void imgui_end_draw();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
