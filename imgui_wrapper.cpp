#include "imgui_wrapper.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
// #include "nodes.h"
#include "imgui_demo.cpp"
#include "tracy\public\tracy\Tracy.hpp"
#include "tracy\public\tracy\TracyOpenGL.hpp"

// Implementation of functions moved from imgui_wrapper.h

static ImGuiIO g_io;
void imgui_init(void *window_handle)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_io = ImGui::GetIO();

    g_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    HWND hwnd = (HWND)window_handle;
    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init("#version 150");
}

void imgui_shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void basic_ui(imgui_data *data)
{
    ZoneScoped;
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Debug Data"); // Create a window called "Hello, world!" and append into it.

    ImGui::SliderFloat("Boid Trail Mult", &data->boid_trail_len, 0.0f, 5.0f); // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::SliderFloat("Boid Max Vel", &data->boid_max_vel, 0.0f, 1.0f);      // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::SliderFloat("Boid Max Acc", &data->boid_max_acc, 0.0f, 1.0f);      // Edit 1 float using a slider from 0.0f to 1.0f

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f * data->frame_time, 1.0f / data->frame_time);
    ImGui::End();
}

void imgui_render(imgui_data *data)
{

    ZoneScoped;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(); // Show the demo window (optional)
    basic_ui(data);
    // Right-click: context menu

    ImGui::Render();
}

void imgui_end_draw()
{
    ZoneScoped;
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

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