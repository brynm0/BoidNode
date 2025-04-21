#pragma once

#include <stdint.h>
#define u64 uint_fast64_t
#define u32 uint_fast32_t
#define u16 uint_fast16_t
#define u8 uint_fast8_t
#define i32 int_fast32_t
#define i16 int_fast16_t
#define i8 int_fast8_t
#define f32 float
#define f64 double
// #define v4 ImVec4
#define v2 vec2

#define MEGABYTES(x) ((x) * 1024 * 1024)
#define KILOBYTES(x) ((x) * 1024)

struct uivec2
{
    u32 x;
    u32 y;
};
