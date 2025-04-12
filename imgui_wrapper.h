#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "nodes.h"
#include "imgui_demo.cpp"

static ImGuiIO g_io;

struct imgui_data
{
    float frame_time;
    float boid_trail_len;
    float boid_max_vel;
    float boid_max_acc;
};

static void imgui_init(HWND hwnd)
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

static void imgui_shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

static void right_click_menu()
{
}

bool show_demo_window = true;
bool show_another_window = false;

static void basic_ui(imgui_data *data)
{
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Debug Data"); // Create a window called "Hello, world!" and append into it.

    ImGui::SliderFloat("Boid Trail Mult", &data->boid_trail_len, 0.0f, 5.0f); // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::SliderFloat("Boid Max Vel", &data->boid_max_vel, 0.0f, 1.0f);      // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::SliderFloat("Boid Max Acc", &data->boid_max_acc, 0.0f, 1.0f);      // Edit 1 float using a slider from 0.0f to 1.0f

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f * data->frame_time, 1.0f / data->frame_time);
    ImGui::End();
}

static void imgui_render(graph_context *context_data, imgui_data *d)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(); // Show the demo window (optional)
    basic_ui(d);
    // Right-click: context menu

    draw_node_editor(context_data); // Draw the node editor

    ImGui::Render();
}

static void imgui_render(imgui_data *d)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(); // Show the demo window (optional)
    basic_ui(d);
    // Right-click: context menu

    ImGui::Render();
}