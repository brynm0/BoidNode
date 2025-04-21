// Global declarations for the Win32 window.

#if 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sysinfoapi.h>
#include <winnt.h>
#include "types.h"
#include "boid_platform.h"
#endif
namespace platform
{

    int get_message_type(message *msg)
    {
        return ((MSG *)msg->msg)->message;
    }

    bool compare_message(message *msg, message_type type)
    {
        return ((MSG *)msg->msg)->message == type;
    }

    bool peek_message(message *msg)
    {
        return PeekMessage((MSG *)msg->msg, NULL, 0, 0, PM_REMOVE);
    }

    void translate_message(message *msg)
    {
        TranslateMessage((MSG *)msg->msg);
    }

    void dispatch_message(message *msg)
    {
        DispatchMessage((MSG *)msg->msg);
    }

    bool compare_w_param(message *msg, key w_param)
    {
        WPARAM wParam = ((MSG *)msg->msg)->wParam;
        return wParam & w_param;
    }

    short extract_wheel_movement(message *msg)
    {
        WPARAM wParam = ((MSG *)msg->msg)->wParam;
        return (short)HIWORD(wParam);
    }

    short get_async_key_state(key k)
    {
        return GetAsyncKeyState(k);
    }

    uivec2 get_client_cursor_pos(platform_data *data)
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient((HWND)data->hwnd, &pt);
        return {(u32)pt.x, (u32)pt.y};
    }

    // Function to create a Win32 window.
    void init_window(platform_data *data, int nCmdShow, const char *class_name, const char *window_title, u32 width, u32 height, void *wnd_proc)
    {
        data->window_proc = (WNDPROC)wnd_proc;
        WNDCLASS wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = (WNDPROC)data->window_proc;
        wc.hInstance = (HINSTANCE)data->hInstance;
        wc.lpszClassName = (LPCSTR)class_name;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClass(&wc);

        RECT rect = {0, 0, (LONG)width, (LONG)height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        data->hwnd = CreateWindow((LPCSTR)class_name, (LPCSTR)window_title,
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  rect.right - rect.left, rect.bottom - rect.top,
                                  NULL, NULL, (HINSTANCE)data->hInstance, NULL);
        ShowWindow((HWND)data->hwnd, nCmdShow);
        UpdateWindow((HWND)data->hwnd);
    }

    window_rectangle get_window_rectangle(platform_data *data)
    {

        RECT rect;
        GetClientRect((HWND)data->hwnd, &rect);
        window_rectangle win_rect;
        win_rect.width = rect.right - rect.left;
        win_rect.height = rect.bottom - rect.top;
        return win_rect;
    }

    // Function to get the current time in seconds since the program started running.
    u64 get_current_time_ms()
    {
        static LARGE_INTEGER frequency = {0};
        static LARGE_INTEGER start_time = {0};

        // Initialize the frequency and start time on the first call
        if (frequency.QuadPart == 0)
        {
            if (!QueryPerformanceFrequency(&frequency))
            {
                // If QueryPerformanceFrequency fails, return 0 as an error case
                return 0;
            }
            QueryPerformanceCounter(&start_time);
        }

        LARGE_INTEGER current_time;
        if (!QueryPerformanceCounter(&current_time))
        {
            // If QueryPerformanceCounter fails, return 0 as an error case
            return 0;
        }

        // Calculate the elapsed time in milliseconds
        u64 elapsed_time_ms = (u64)(((current_time.QuadPart - start_time.QuadPart) * 1000) / frequency.QuadPart);
        return elapsed_time_ms;
    }
}