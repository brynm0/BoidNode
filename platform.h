#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
#else
    #error "Unsupported platform"
#endif

// Include platform-specific headers
#if PLATFORM_WINDOWS
    #include "boid_win32.h"
    #include "gl_render.h"
    
    // Define platform-specific types and functions
    typedef bgl::gl_mesh RenderMesh;
    typedef bgl::ubo_material Material;
    typedef bgl::ubo_light Light;
    
    #define platform_init win32_platform_init
    #define platform_cleanup win32_platform_cleanup
    #define platform_process_messages win32_process_messages
    #define platform_swap_buffers win32_swap_buffers
    #define platform_get_time win32_get_time
    
    #define render_init bgl::init
    #define render_cleanup bgl::cleanup
    #define render_add_mesh bgl::add_mesh
    #define render_set_mvp bgl::set_mvp
    #define render_set_light bgl::set_light
    #define render_set_material bgl::set_material
    #define render_draw_line bgl::draw_line
    #define render_draw_line_ex bgl::draw_line_ex
    #define render_line_init bgl::line_render_init
    #define render_start_draw bgl::start_draw
    #define render_draw_statics bgl::draw_statics
    #define render_instances bgl::render_instances
    #define render_lines bgl::render_lines
    #define render_end_draw bgl::end_draw
    
    // Platform-specific state
    typedef Win32PlatformState PlatformState;
#elif PLATFORM_MACOS
    #include "boid_macos.h"
    #include "moltenvk_render.h"
    
    // Define platform-specific types and functions
    typedef mvk::vk_mesh RenderMesh;
    typedef mvk::ubo_material Material;
    typedef mvk::ubo_light Light;
    
    #define platform_init macos_platform_init
    #define platform_cleanup macos_platform_cleanup
    #define platform_process_messages macos_process_messages
    #define platform_swap_buffers macos_swap_buffers
    #define platform_get_time macos_get_time
    
    #define render_init mvk::init
    #define render_cleanup mvk::cleanup
    #define render_add_mesh mvk::add_mesh
    #define render_set_mvp mvk::set_mvp
    #define render_set_light mvk::set_light
    #define render_set_material mvk::set_material
    #define render_draw_line mvk::draw_line
    #define render_draw_line_ex mvk::draw_line_ex
    #define render_line_init mvk::line_render_init
    #define render_start_draw mvk::start_draw
    #define render_draw_statics mvk::draw_statics
    #define render_instances mvk::render_instances
    #define render_lines mvk::render_lines
    #define render_end_draw mvk::end_draw
    
    // Platform-specific state
    typedef MacOSPlatformState PlatformState;
#endif