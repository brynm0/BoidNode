#pragma once

#include <math.h>

struct vec4
{
    float x, y, z, w;
};

struct mat4
{
    vec4 m[4]; // 4x4 matrix stored in column-major order
};

mat4 mat4_mult(const mat4 &a, const mat4 &b)
{
    mat4 result = {};
    for (int col = 0; col < 4; ++col) // Iterate over columns of the result
    {
        for (int row = 0; row < 4; ++row) // Iterate over rows of the result
        {
            result.m[col].x += a.m[0].x * b.m[col].x + a.m[1].x * b.m[col].y + a.m[2].x * b.m[col].z + a.m[3].x * b.m[col].w;
            result.m[col].y += a.m[0].y * b.m[col].x + a.m[1].y * b.m[col].y + a.m[2].y * b.m[col].z + a.m[3].y * b.m[col].w;
            result.m[col].z += a.m[0].z * b.m[col].x + a.m[1].z * b.m[col].y + a.m[2].z * b.m[col].z + a.m[3].z * b.m[col].w;
            result.m[col].w += a.m[0].w * b.m[col].x + a.m[1].w * b.m[col].y + a.m[2].w * b.m[col].z + a.m[3].w * b.m[col].w;
        }
    }
    return result;
}
// {
//     vec4 mvp[4]; // 4x4 matrix in column-major order
// };

//------------------------------------------------------------------------------
// Definition of a simple vec3 struct for 3D vector operations.
typedef struct vec3
{
    float x;
    float y;
    float z;
} vec3;

//------------------------------------------------------------------------------
// Utility function: Adds two vec3 vectors (a + b) and returns the result.
static inline vec3 vector_add(const vec3 a, const vec3 b)
{
    vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

//------------------------------------------------------------------------------
// Utility function: Subtracts two vec3 vectors (a - b) and returns the result.
static inline vec3 vector_subtract(const vec3 a, const vec3 b)
{
    vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

//------------------------------------------------------------------------------
// Utility function: Normalizes a vec3 vector and returns the result.
// If the vector length is zero, the function returns the original vector.
static inline vec3 vector_normalize(const vec3 v)
{
    float len = (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len == 0.0f)
        return v; // Avoid division by zero
    vec3 result;
    result.x = v.x / len;
    result.y = v.y / len;
    result.z = v.z / len;
    return result;
}

//------------------------------------------------------------------------------
// Utility function: Computes the cross product of two vec3 vectors (a x b)
// and returns the result.
static inline vec3 vector_cross(const vec3 a, const vec3 b)
{
    vec3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

//------------------------------------------------------------------------------
// Utility function: Computes the dot product of two vec3 vectors (a . b)
// and returns the result.
static inline float vector_dot(const vec3 a, const vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

//------------------------------------------------------------------------------
// Utility function: Rotates a vec3 vector v around an axis by rad radians
// and returns the result.
static inline vec3 vector_rotate(const vec3 v, const vec3 axis, float rad)
{
    vec3 normalized_axis = vector_normalize(axis);
    float cos_theta = cosf(rad);
    float sin_theta = sinf(rad);

    // Rodrigues' rotation formula
    vec3 term1 = {v.x * cos_theta, v.y * cos_theta, v.z * cos_theta};
    vec3 term2 = vector_cross(normalized_axis, v);
    term2.x *= sin_theta;
    term2.y *= sin_theta;
    term2.z *= sin_theta;
    vec3 term3 = normalized_axis;
    float dot_product = vector_dot(normalized_axis, v);
    term3.x *= dot_product * (1.0f - cos_theta);
    term3.y *= dot_product * (1.0f - cos_theta);
    term3.z *= dot_product * (1.0f - cos_theta);

    vec3 result;
    result.x = term1.x + term2.x + term3.x;
    result.y = term1.y + term2.y + term3.y;
    result.z = term1.z + term2.z + term3.z;

    return result;
}

mat4 mat4_identity()
{
    mat4 matrix = {};
    float temp[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};

    memcpy(&matrix.m, temp, 16 * sizeof(float));

    return matrix;

}; // Identity matrix

mat4 perspective_matrix(float width, float height, float fov, float near_plane, float far_plane)
{
    mat4 matrix = {};

    float aspect_ratio = width / height;
    float fov_rad = 1.0f / tanf(fov * 0.5f * (float)M_PI / 180.0f); // Convert FOV to radians and calculate cotangent

    matrix.m[0].x = fov_rad / aspect_ratio;
    matrix.m[1].y = fov_rad;
    matrix.m[2].z = -(far_plane + near_plane) / (far_plane - near_plane);
    matrix.m[2].w = -1.0f;
    matrix.m[3].z = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);

    return matrix;
}
