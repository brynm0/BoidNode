#include "imgui_wrapper.h"
#include "imgui.h"

#if PLATFORM_WINDOWS
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#elif PLATFORM_MACOS
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#endif

// Include ImGui demo code
#include "imgui_demo.cpp"

static ImGuiIO g_io;

#if PLATFORM_WINDOWS
// Windows implementation
void imgui_init(HWND hwnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_io = ImGui::GetIO();

    g_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

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

void imgui_render(imgui_data *data)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(); // Show the demo window (optional)
    basic_ui(data);

    ImGui::Render();
}

void imgui_end_draw()
{
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Window procedure for Win32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Let ImGui process Windows messages first
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 0;

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

#elif PLATFORM_MACOS
// macOS implementation using GLFW and Metal
void imgui_init(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_io = ImGui::GetIO();

    g_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings for Metal
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplMetal_Init(MTLCreateSystemDefaultDevice());
}

void imgui_shutdown()
{
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void imgui_render(imgui_data *data)
{
    ImGui_ImplMetal_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(); // Show the demo window (optional)
    basic_ui(data);

    ImGui::Render();
}

void imgui_end_draw()
{
    // In a real implementation, we would need to get the Metal command buffer
    // and pass it to ImGui_ImplMetal_RenderDrawData
    // This is a simplified implementation
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), nullptr, nullptr);
}
#endif

// Common UI functions
void basic_ui(imgui_data *data)
{
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Debug Data"); // Create a window called "Debug Data" and append into it.

    ImGui::SliderFloat("Boid Trail Mult", &data->boid_trail_len, 0.0f, 5.0f);
    ImGui::SliderFloat("Boid Max Vel", &data->boid_max_vel, 0.0f, 1.0f);
    ImGui::SliderFloat("Boid Max Acc", &data->boid_max_acc, 0.0f, 1.0f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f * data->frame_time, 1.0f / data->frame_time);
    ImGui::End();
}