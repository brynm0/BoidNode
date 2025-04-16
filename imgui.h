#pragma once
#include <string>

// Minimal ImGui header for testing purposes
// This is a temporary file that would be replaced with the actual ImGui headers

struct ImGuiIO {
    float ConfigFlags;
};

namespace ImGui {
    bool Begin(const char* name, bool* p_open = nullptr, int flags = 0);
    void End();
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, 
                    const char* format = "%.3f", int flags = 0);
    void Text(const char* fmt, ...);
    void CreateContext();
    void StyleColorsDark();
    void NewFrame();
    void Render();
    void DestroyContext();
    ImGuiIO& GetIO();
}

#define ImGuiConfigFlags_NavEnableKeyboard 1
#define IMGUI_CHECKVERSION()

// Forward declarations for ImGui implementation functions
namespace ImGui_ImplGlfw {
    void InitForOther(void* window, bool install_callbacks);
    void NewFrame();
    void Shutdown();
}

namespace ImGui_ImplMetal {
    void Init(void* device);
    void NewFrame();
    void RenderDrawData(void* draw_data, void* command_buffer, void* encoder);
    void Shutdown();
}

void* MTLCreateSystemDefaultDevice();
