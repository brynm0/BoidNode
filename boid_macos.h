#pragma once

#ifdef __APPLE__

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

// Define the platform-specific state struct for macOS
typedef struct MacOSPlatformState {
    GLFWwindow* window;     // GLFW window handle
    int width;              // Window width
    int height;             // Window height
    bool shouldClose;       // Flag to indicate if window should close
    double lastTime;        // Last frame time for timing calculations
} MacOSPlatformState;

// Platform-specific initialization
static void macos_platform_init(MacOSPlatformState* state, int width, int height, const char* title) {
    if (!state) {
        fprintf(stderr, "Invalid platform state\n");
        return;
    }
    
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(EXIT_FAILURE);
    }
    
    // Set GLFW to use the Metal API on macOS
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    // Create a window
    state->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!state->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    // Save window dimensions
    state->width = width;
    state->height = height;
    state->shouldClose = false;
    state->lastTime = glfwGetTime();
    
    // Set up window resize callback
    glfwSetWindowSizeCallback(state->window, [](GLFWwindow* window, int width, int height) {
        MacOSPlatformState* state = (MacOSPlatformState*)glfwGetWindowUserPointer(window);
        if (state) {
            state->width = width;
            state->height = height;
        }
    });
    
    // Store the state pointer for callbacks
    glfwSetWindowUserPointer(state->window, state);
    
    printf("MacOS platform initialized with window size %dx%d\n", width, height);
}

// Process window events
static bool macos_process_messages(MacOSPlatformState* state) {
    if (!state || !state->window) {
        return false;
    }
    
    // Check if window should close
    if (glfwWindowShouldClose(state->window)) {
        state->shouldClose = true;
        return false;
    }
    
    // Process events
    glfwPollEvents();
    
    // Check for ESC key to exit
    if (glfwGetKey(state->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        state->shouldClose = true;
        return false;
    }
    
    return true;
}

// Swap buffers (for rendering)
static void macos_swap_buffers(MacOSPlatformState* state) {
    if (state && state->window) {
        glfwSwapBuffers(state->window);
    }
}

// Get high-precision time in seconds
static double macos_get_time() {
    return glfwGetTime();
}

// Clean up resources
static void macos_platform_cleanup(MacOSPlatformState* state) {
    if (state && state->window) {
        glfwDestroyWindow(state->window);
        glfwTerminate();
    }
}

#endif // __APPLE__