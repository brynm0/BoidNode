#pragma once

#ifdef _WIN32

#include <windows.h>
#include "types.h"
#include "math_linear.h"

// Platform-specific functions for Windows
typedef struct Win32PlatformState {
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    int width;
    int height;
    bool running;
    // Add any Windows-specific state here
} Win32PlatformState;

// Initialize the Windows platform
static bool win32_platform_init(Win32PlatformState* state, int width, int height, const char* title) {
    state->width = width;
    state->height = height;
    state->running = true;
    
    // Create a window class
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "BoidNodeWindow";
    RegisterClassEx(&wc);
    
    // Create the window
    state->hwnd = CreateWindowEx(
        0,
        "BoidNodeWindow",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!state->hwnd) {
        return false;
    }
    
    // Get the device context
    state->hdc = GetDC(state->hwnd);
    if (!state->hdc) {
        DestroyWindow(state->hwnd);
        return false;
    }
    
    // Setup pixel format for OpenGL
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pf = ChoosePixelFormat(state->hdc, &pfd);
    if (!pf) {
        ReleaseDC(state->hwnd, state->hdc);
        DestroyWindow(state->hwnd);
        return false;
    }
    
    if (!SetPixelFormat(state->hdc, pf, &pfd)) {
        ReleaseDC(state->hwnd, state->hdc);
        DestroyWindow(state->hwnd);
        return false;
    }
    
    // Create OpenGL context
    state->hglrc = wglCreateContext(state->hdc);
    if (!state->hglrc) {
        ReleaseDC(state->hwnd, state->hdc);
        DestroyWindow(state->hwnd);
        return false;
    }
    
    // Make the OpenGL context current
    wglMakeCurrent(state->hdc, state->hglrc);
    
    // Show the window
    ShowWindow(state->hwnd, SW_SHOW);
    
    return true;
}

// Clean up Windows platform resources
static void win32_platform_cleanup(Win32PlatformState* state) {
    if (state->hglrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(state->hglrc);
    }
    
    if (state->hwnd && state->hdc) {
        ReleaseDC(state->hwnd, state->hdc);
    }
    
    if (state->hwnd) {
        DestroyWindow(state->hwnd);
    }
    
    // Unregister window class
    UnregisterClass("BoidNodeWindow", GetModuleHandle(NULL));
}

// Process Windows messages
static bool win32_process_messages(Win32PlatformState* state) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            state->running = false;
            return false;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return true;
}

// Swap buffers for double buffering
static void win32_swap_buffers(Win32PlatformState* state) {
    SwapBuffers(state->hdc);
}

// Get the current time in seconds
static double win32_get_time() {
    static LARGE_INTEGER frequency = { 0 };
    static LARGE_INTEGER start_time = { 0 };
    
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_time);
        return 0.0;
    }
    
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    
    return (double)(current_time.QuadPart - start_time.QuadPart) / (double)frequency.QuadPart;
}

#endif // _WIN32