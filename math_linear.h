#pragma once

#include <math.h>

struct vec4
{
    float x, y, z, w;
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    vec4() : x(0), y(0), z(0), w(0) {}
};

//------------------------------------------------------------------------------
// Definition of a simple vec3 struct for 3D vector operations.
typedef struct vec3
{
    float x;
    float y;
    float z;
    vec3 operator*(float scalar) const
    {
        return {x * scalar, y * scalar, z * scalar};
    }

    // Multiply float * vec3
    friend vec3 operator*(float scalar, const vec3 &v)
    {
        return {v.x * scalar, v.y * scalar, v.z * scalar};
    }

    vec3 operator+(const vec3 &v) const
    {
        return {x + v.x, y + v.y, z + v.z};
    }
    vec3 operator-(const vec3 &v) const
    {
        return {x - v.x, y - v.y, z - v.z};
    }
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    vec3() : x(0), y(0), z(0) {}
} vec3;

struct vec2
{
    float x, y;

    vec2 operator+(const vec2 &v) const
    {
        return {x + v.x, y + v.y};
    }
    // Multiply vec2 * float
    vec2 operator*(float scalar) const
    {
        return {x * scalar, y * scalar};
    }

    // Multiply float * vec2
    friend vec2 operator*(float scalar, const vec2 &v)
    {
        return {v.x * scalar, v.y * scalar};
    }

    vec2 operator-(const vec2 &v) const
    {
        return {x - v.x, y - v.y};
    }

    vec2 operator/(float scalar) const
    {
        return {x / scalar, y / scalar};
    }
    vec2(float x, float y) : x(x), y(y) {}
    vec2() : x(0), y(0) {}
};

struct mat4
{
    vec4 m[4]; // 4x4 matrix stored in column-major order
};

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

// Build a scale matrix
mat4 mat4_scale(vec3 s)
{
    mat4 out = mat4_identity();
    out.m[0].x = s.x;
    out.m[1].y = s.y;
    out.m[2].z = s.z;
    return out;
}

// Build a translation matrix
mat4 mat4_translate(vec3 t)
{
    mat4 out = mat4_identity();
    out.m[3].x = t.x;
    out.m[3].y = t.y;
    out.m[3].z = t.z;
    return out;
}

// Build a rotation matrix around X axis
mat4 mat4_rotate_x(float angle)
{
    mat4 out = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    out.m[1].y = c;
    out.m[1].z = s;
    out.m[2].y = -s;
    out.m[2].z = c;
    return out;
}

// Build a rotation matrix around Y axis
mat4 mat4_rotate_y(float angle)
{
    mat4 out = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    out.m[0].x = c;
    out.m[0].z = -s;
    out.m[2].x = s;
    out.m[2].z = c;
    return out;
}

// Build a rotation matrix around Z axis
mat4 mat4_rotate_z(float angle)
{
    mat4 out = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    out.m[0].x = c;
    out.m[0].y = s;
    out.m[1].x = -s;
    out.m[1].y = c;
    return out;
}
// Final model matrix builder
mat4 get_model_matrix(vec3 position, vec3 rotation, vec3 scale)
{
    mat4 S = mat4_scale(scale);
    mat4 Rx = mat4_rotate_x(rotation.x);
    mat4 Ry = mat4_rotate_y(rotation.y);
    mat4 Rz = mat4_rotate_z(rotation.z);
    mat4 T = mat4_translate(position);

    // Rotation order ZYX
    mat4 R = mat4_mult(Rz, mat4_mult(Ry, Rx));

    // M = T * R * S
    return mat4_mult(T, mat4_mult(R, S));
}

static vec4 mat4_mult_vec4(const mat4 m, const vec4 v)
{
    vec4 result;

    // Column 0 contributes x component
    result.x = m.m[0].x * v.x + m.m[1].x * v.y + m.m[2].x * v.z + m.m[3].x * v.w;

    // Column 1 contributes y component
    result.y = m.m[0].y * v.x + m.m[1].y * v.y + m.m[2].y * v.z + m.m[3].y * v.w;

    // Column 2 contributes z component
    result.z = m.m[0].z * v.x + m.m[1].z * v.y + m.m[2].z * v.z + m.m[3].z * v.w;

    // Column 3 contributes w component
    result.w = m.m[0].w * v.x + m.m[1].w * v.y + m.m[2].w * v.z + m.m[3].w * v.w;

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
