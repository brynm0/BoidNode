#pragma once

#include <stdint.h>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
#endif

// Prevent collision with Vulkan headers on macOS
#if PLATFORM_MACOS
    // Define these types without macros to avoid conflicts with Vulkan
    typedef uint_fast64_t u64;
    typedef uint_fast32_t u32;
    typedef uint_fast16_t u16;
    typedef uint_fast8_t u8;
    typedef int_fast32_t i32;
    typedef int_fast16_t i16;
    typedef int_fast8_t i8;
    typedef float f32;
    typedef double f64;
#else
    // Windows can use the macros as before
    #define u64 uint_fast64_t
    #define u32 uint_fast32_t
    #define u16 uint_fast16_t
    #define u8 uint_fast8_t
    #define i32 int_fast32_t
    #define i16 int_fast16_t
    #define i8 int_fast8_t
    #define f32 float
    #define f64 double
#endif

// #define v4 ImVec4
#define v2 vec2

#define MEGABYTES(x) ((x) * 1024 * 1024)
#define KILOBYTES(x) ((x) * 1024)