#!/bin/bash

# Set paths for dependencies
# Use current directory for ImGui if environment variables aren't set
IMGUI_DIR=${IMGUI_DIR:-"./imgui"}
GLFW_DIR=${GLFW_DIR:-"/Users/brynmurrell/Downloads/glfw-3.4.bin.MACOS/include"}
GLFW_LIB_DIR=${GLFW_LIB_DIR:-"/Users/brynmurrell/Downloads/glfw-3.4.bin.MACOS/lib-universal"}
VULKAN_SDK=${VULKAN_SDK:-"/Users/brynmurrell/VulkanSDK/1.4.309.0"}
VULKAN_INCLUDE=${VULKAN_INCLUDE:-"$VULKAN_SDK/macOS/include"}
VULKAN_LIB=${VULKAN_LIB:-"$VULKAN_SDK/macOS/lib"}
LIBMORTON_DIR=${LIBMORTON_DIR:-"/Users/brynmurrell/Documents/GitHub/libmorton/include"}

echo "Using ImGui directory: $IMGUI_DIR"
echo "Using GLFW directory: $GLFW_DIR"
echo "Using Vulkan SDK: $VULKAN_SDK"
echo "Using Vulkan include: $VULKAN_INCLUDE"
echo "Using libmorton directory: $LIBMORTON_DIR"

# Check if we need to use a minimal ImGui implementation for testing
USE_MINIMAL_IMGUI=1

if [ $USE_MINIMAL_IMGUI -eq 1 ]; then
    echo "Using minimal ImGui implementation for testing..."
    
    # Create a temporary imgui_minimal.cpp file
    cat > imgui_minimal.cpp << 'EOF'
#include "imgui_wrapper.h"
#include "imgui.h"

// Minimal ImGui implementation for testing purposes
// This would be replaced with actual ImGui implementation in production

// Define ImGui functions needed for compiling
namespace ImGui {
    bool Begin(const char* name, bool* p_open, int flags) { return true; }
    void End() {}
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, int flags) { return true; }
    void Text(const char* fmt, ...) {}
    void CreateContext() {}
    void StyleColorsDark() {}
    void NewFrame() {}
    void Render() {}
    void DestroyContext() {}
    ImGuiIO& GetIO() { static ImGuiIO io; return io; }
}

// ImGui version macros
#define IMGUI_CHECKVERSION()

// Implementation of ImGui_Impl functions
namespace ImGui_ImplGlfw {
    void InitForOther(void* window, bool install_callbacks) {}
    void NewFrame() {}
    void Shutdown() {}
}

namespace ImGui_ImplMetal {
    void Init(void* device) {}
    void NewFrame() {}
    void RenderDrawData(void* draw_data, void* command_buffer, void* encoder) {}
    void Shutdown() {}
}

#if defined(__APPLE__)
// macOS implementation
void imgui_init(GLFWwindow* window)
{
    // Empty implementation for testing
}

void imgui_shutdown()
{
    // Empty implementation for testing
}

void imgui_render(imgui_data *data)
{
    // Empty implementation for testing
}

void imgui_end_draw()
{
    // Empty implementation for testing
}
#endif

// Common UI functions
void basic_ui(imgui_data *data)
{
    // Empty implementation for testing
}
EOF

    # Compile the minimal implementation
    echo "Compiling minimal ImGui implementation..."
    clang++ -std=c++11 -c imgui_minimal.cpp -o imgui_wrapper.o \
        -I"$GLFW_DIR" -I"." -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -DPLATFORM_MACOS=1
    
    # Create the static library
    ar rcs imgui_wrapper.a imgui_wrapper.o
    
    # Clean up
    rm imgui_minimal.cpp imgui_wrapper.o
else
    # Original implementation that would be used in production
    echo "Compiling ImGui wrapper..."

    # Compile ImGui files individually
    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        imgui_wrapper.cpp -o imgui_wrapper.o

    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        "$IMGUI_DIR/imgui.cpp" -o imgui.o

    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        "$IMGUI_DIR/imgui_draw.cpp" -o imgui_draw.o

    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        "$IMGUI_DIR/imgui_widgets.cpp" -o imgui_widgets.o

    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        "$IMGUI_DIR/imgui_tables.cpp" -o imgui_tables.o

    # For macOS, we would use imgui_impl_glfw.cpp and imgui_impl_metal.cpp
    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        "$IMGUI_DIR/backends/imgui_impl_glfw.cpp" -o imgui_impl_glfw.o

    clang++ -std=c++11 -O3 -march=native -c \
        -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$GLFW_DIR" -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"." \
        -Wno-deprecated-declarations -DPLATFORM_MACOS=1 \
        "$IMGUI_DIR/backends/imgui_impl_metal.cpp" -o imgui_impl_metal.o

    # Create ImGui static library
    ar rcs imgui_wrapper.a imgui_wrapper.o imgui.o imgui_draw.o imgui_widgets.o imgui_tables.o imgui_impl_glfw.o imgui_impl_metal.o

    # Clean up object files
    rm -f *.o
fi

echo "ImGui wrapper compilation complete."