#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "nodes.h"
#include "imgui_demo.cpp"

static ImGuiIO g_io;
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

static void basic_ui()
{
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

    ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
    ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
    ImGui::Checkbox("Another Window", &show_another_window);

    ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

    if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / g_io.Framerate, g_io.Framerate);
    ImGui::End();
}

static void imgui_render(graph_context_data *context_data)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(); // Show the demo window (optional)
    basic_ui();
    // Right-click: context menu

    draw_node_editor(context_data->mesh_nodes, context_data->vec3_nodes, context_data->links); // Draw the node editor

    ImGui::Render();
}