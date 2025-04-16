#!/bin/bash

# Make sure scripts are executable
chmod +x compile.sh
chmod +x compile_imgui.sh

# Define default paths (these can be overridden by environment variables)
IMGUI_DIR=${IMGUI_DIR:-"./imgui"}
IMNODES_DIR=${IMNODES_DIR:-"./imnodes"}
GLFW_DIR=${GLFW_DIR:-"/Users/brynmurrell/Downloads/glfw-3.4.bin.MACOS/include"}
GLFW_LIB_DIR=${GLFW_LIB_DIR:-"/Users/brynmurrell/Downloads/glfw-3.4.bin.MACOS/lib-universal"}
VULKAN_SDK=${VULKAN_SDK:-"/Users/brynmurrell/VulkanSDK/1.4.309.0"}
VULKAN_INCLUDE=${VULKAN_INCLUDE:-"$VULKAN_SDK/macOS/include"}
VULKAN_LIB=${VULKAN_LIB:-"$VULKAN_SDK/macOS/lib"}
LIBMORTON_DIR=${LIBMORTON_DIR:-"/Users/brynmurrell/Documents/GitHub/libmorton/include"}

echo "Using paths:"
echo "IMGUI_DIR: $IMGUI_DIR"
echo "IMNODES_DIR: $IMNODES_DIR"
echo "GLFW_DIR: $GLFW_DIR"
echo "GLFW_LIB_DIR: $GLFW_LIB_DIR"
echo "VULKAN_SDK: $VULKAN_SDK"
echo "VULKAN_INCLUDE: $VULKAN_INCLUDE"
echo "VULKAN_LIB: $VULKAN_LIB"
echo "LIBMORTON_DIR: $LIBMORTON_DIR"

# Check if imgui_wrapper.a needs to be recompiled
if [ -f "imgui_wrapper.a" ]; then
    # Get last modification time
    LIB_MTIME=$(stat -f "%m" imgui_wrapper.a)
    
    # Check if source files are newer than the library
    NEEDS_RECOMPILE=0
    
    if [ -f "imgui_wrapper.cpp" ] && [ $(stat -f "%m" imgui_wrapper.cpp) -gt $LIB_MTIME ]; then
        echo "Recompiling imgui_wrapper.a because imgui_wrapper.cpp is newer."
        NEEDS_RECOMPILE=1
    fi
    
    if [ -f "imgui_wrapper.h" ] && [ $(stat -f "%m" imgui_wrapper.h) -gt $LIB_MTIME ]; then
        echo "Recompiling imgui_wrapper.a because imgui_wrapper.h is newer."
        NEEDS_RECOMPILE=1
    fi
    
    if [ $NEEDS_RECOMPILE -eq 1 ]; then
        VULKAN_SDK=$VULKAN_SDK VULKAN_INCLUDE=$VULKAN_INCLUDE VULKAN_LIB=$VULKAN_LIB ./compile_imgui.sh
    fi
else
    echo "imgui_wrapper.a not found. Compiling..."
    VULKAN_SDK=$VULKAN_SDK VULKAN_INCLUDE=$VULKAN_INCLUDE VULKAN_LIB=$VULKAN_LIB ./compile_imgui.sh
fi

# Define the ImGui include header (create it if it doesn't exist)
if [ ! -f "imgui.h" ]; then
    echo "Creating minimal imgui.h for testing..."
    cat > imgui.h << 'EOF'
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
EOF
fi

# Create minimal Metal.h if needed
if [ ! -f "Metal.h" ]; then
    echo "Creating minimal Metal.h for testing..."
    cat > Metal.h << 'EOF'
#pragma once
// Minimal Metal header for testing purposes
void* MTLCreateSystemDefaultDevice() { return nullptr; }
EOF
fi

# Compile main program
echo "Compiling main program..."
clang++ -std=c++11 -O3 -march=native -Wno-deprecated-declarations \
    -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends" -I"$IMNODES_DIR" -I"$GLFW_DIR" -I"." \
    -I"$VULKAN_INCLUDE" -I"$VULKAN_SDK/MoltenVK/include" -I"$LIBMORTON_DIR" \
    -framework Cocoa -framework IOKit -framework CoreVideo \
    -L"$GLFW_LIB_DIR" -L"$VULKAN_LIB" -L"$VULKAN_SDK/macOS/lib/MoltenVK.xcframework/macos-arm64_x86_64" \
    -DPLATFORM_MACOS=1 \
    main.cpp imgui_wrapper.a -o main \
    -lglfw3 -Wl,-rpath,"$VULKAN_LIB" -lMoltenVK -ldl -lpthread

if [ $? -eq 0 ]; then
    echo "Compilation successful. Generated executable: main"
    
    # Make the executable file executable
    chmod +x main
    
    echo "Run the program with ./main"
else
    echo "Compilation failed."
    exit 1
fi