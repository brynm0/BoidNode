// Global declarations for the Win32 window.

#if 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sysinfoapi.h>
#include <winnt.h>
#endif
#include "types.h"
namespace platform
{

    struct message
    {
        void *msg; // (MSG)
    };
    struct platform_data
    {
        HINSTANCE hInstance; // (HINSTANCE)
        HWND hwnd;           // (HWND)
        WNDPROC window_proc; // (WNDPROC)
    };

    enum message_type
    {
        MSG_QUIT = WM_QUIT,
        MSG_RBUTTONDOWN = WM_RBUTTONDOWN,
        MSG_RBUTTONUP = WM_RBUTTONUP,
        MSG_MOUSEMOVE = WM_MOUSEMOVE,
        MSG_LBUTTONDOWN = WM_LBUTTONDOWN,
    };

    enum key
    {
        KEY_SHIFT = VK_SHIFT,
        KEY_RBUTTON = MK_RBUTTON,
    };

    struct window_rectangle
    {
        int width;
        int height;
    };

    int get_message_type(message *msg);
    bool compare_message(message *msg, message_type type);
    bool peek_message(message *msg);
    void translate_message(message *msg);
    void dispatch_message(message *msg);
    bool compare_w_param(message *msg, key w_param);
    short extract_wheel_movement(message *msg);
    short get_async_key_state(key k);
    uivec2 get_client_cursor_pos(platform_data *data);
    // Function to create a Win32 window.
    void init_window(platform_data *data, int nCmdShow, const char *class_name, const char *window_title, u32 width, u32 height, void *wnd_proc);
    window_rectangle get_window_rectangle(platform_data *data);
    // Function to get the current time in seconds since the program started running.
    u64 get_current_time_ms();
}