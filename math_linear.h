#pragma once

#include <math.h>
#include "string.h"

// AVX2 intrinsics support, only include if needed
#if defined(__AVX2__) || (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64)))
#define HAS_AVX2 1
#include <immintrin.h> // Include AVX2 intrinsics header
#else
#define HAS_AVX2 0
#endif

#define PI(x) ((x) * 3.14159265358979323846f)

struct uivec3
{
    u32 x;
    u32 y;
    u32 z;
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

struct vec4
{
    union
    {
        struct
        {
            float x, y, z, w;
        };
        struct
        {
            vec3 xyz;
            float _ignore_w;
        };
        float data[4]; // optional: for SIMD or array-style access
    };

    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    vec4() : x(0), y(0), z(0), w(0) {}
    vec4 operator-(vec4 v) const
    {
        return {x - v.x, y - v.y, z - v.z, w - v.w};
    }
    vec4 operator+(vec4 v) const
    {
        return {x + v.x, y + v.y, z + v.z, w + v.w};
    }
    vec4 operator*(float scalar) const
    {
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }
};

namespace v4
{
    static inline float sq_mag(const vec4 v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
    }
    static inline vec4 normalize(const vec4 v)
    {
        float len = (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
        if (len == 0.0f)
            return v; // Avoid division by zero
        vec4 result;
        result.x = v.x / len;
        result.y = v.y / len;
        result.z = v.z / len;
        result.w = v.w / len;
        return result;
    }
    static inline float dot(const vec4 a, const vec4 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
    static inline vec4 cross(const vec4 a, const vec4 b)
    {
        vec4 result;
        result.x = a.y * b.z - a.z * b.y;
        result.y = a.z * b.x - a.x * b.z;
        result.z = a.x * b.y - a.y * b.x;
        result.w = 0.0f; // Set w to 0 for cross product in 3D space
        return result;
    }
}

struct mat4
{
    vec4 m[4]; // 4x4 matrix stored in column-major order
};

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

namespace v3
{
#if HAS_AVX2
    // AVX2-optimized vector operations - these are only used internally when the AVX2 optimizations are enabled
    static inline vec3 normalize_avx2(const vec3 v)
    {
        // Load vector components into XMM registers (we only need 3 components)
        __m128 vec = _mm_set_ps(0.0f, v.z, v.y, v.x);

        // Calculate squared length: x*x + y*y + z*z
        __m128 sq = _mm_mul_ps(vec, vec); // [x*x, y*y, z*z, 0]

        // Calculate sum of squares
        __m128 sum = _mm_hadd_ps(sq, sq); // [x*x+y*y, z*z+0, x*x+y*y, z*z+0]
        sum = _mm_hadd_ps(sum, sum);      // [x*x+y*y+z*z, ...]

        // Check if length is zero
        float len_sq;
        _mm_store_ss(&len_sq, sum);

        if (len_sq < 1e-6f)
        {
            return v; // Avoid division by zero
        }

        // Calculate reciprocal square root (1/sqrt(len_sq))
        __m128 inv_len = _mm_rsqrt_ps(sum);

        // Multiply vector by inverse length
        __m128 result = _mm_mul_ps(vec, inv_len);

        // Extract result
        float temp[4];
        _mm_storeu_ps(temp, result);

        return {temp[0], temp[1], temp[2]};
    }

    static inline float dot_avx2(const vec3 a, const vec3 b)
    {
        __m128 va = _mm_set_ps(0.0f, a.z, a.y, a.x);
        __m128 vb = _mm_set_ps(0.0f, b.z, b.y, b.x);

        // Multiply components
        __m128 mul = _mm_mul_ps(va, vb);

        // Sum all components
        __m128 sum = _mm_hadd_ps(mul, mul);
        sum = _mm_hadd_ps(sum, sum);

        // Extract result
        float result;
        _mm_store_ss(&result, sum);

        return result;
    }

    static inline vec3 cross_avx2(const vec3 a, const vec3 b)
    {
        // a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x
        __m128 va = _mm_set_ps(0.0f, a.z, a.y, a.x);
        __m128 vb = _mm_set_ps(0.0f, b.z, b.y, b.x);

        // Shuffle components for cross product calculation
        // a.y, a.z, a.x, 0
        __m128 va_yzx = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 0, 2, 1));
        // a.z, a.x, a.y, 0
        __m128 va_zxy = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 1, 0, 2));

        // b.z, b.x, b.y, 0
        __m128 vb_zxy = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 1, 0, 2));
        // b.y, b.z, b.x, 0
        __m128 vb_yzx = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 0, 2, 1));

        // Multiply the shuffled vectors
        __m128 m1 = _mm_mul_ps(va_yzx, vb_zxy);
        __m128 m2 = _mm_mul_ps(va_zxy, vb_yzx);

        // Subtract for final result
        __m128 result = _mm_sub_ps(m1, m2);

        // Extract result
        float temp[4];
        _mm_storeu_ps(temp, result);

        return {temp[0], temp[1], temp[2]};
    }
#endif

    static inline float sq_mag(const vec3 v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    static inline vec3 clamp(const vec3 v, float max_length)
    {
        float len = (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > max_length)
        {
            vec3 result;
            result.x = v.x / len * max_length;
            result.y = v.y / len * max_length;
            result.z = v.z / len * max_length;
            return result;
        }
        return v; // No change needed
    }

    //------------------------------------------------------------------------------
    // Utility function: Normalizes a vec3 vector and returns the result.
    // If the vector length is zero, the function returns the original vector.
    static inline vec3 normalize(const vec3 v)
    {
#if HAS_AVX2
        return normalize_avx2(v);
#else
        float len = (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len == 0.0f)
            return v; // Avoid division by zero
        vec3 result;
        result.x = v.x / len;
        result.y = v.y / len;
        result.z = v.z / len;
        return result;
#endif
    }

    //------------------------------------------------------------------------------
    // Utility function: Computes the cross product of two vec3 vectors (a x b)
    // and returns the result.
    static inline vec3 cross(const vec3 a, const vec3 b)
    {
#if HAS_AVX2
        return cross_avx2(a, b);
#else
        vec3 result;
        result.x = a.y * b.z - a.z * b.y;
        result.y = a.z * b.x - a.x * b.z;
        result.z = a.x * b.y - a.y * b.x;
        return result;
#endif
    }

    //------------------------------------------------------------------------------
    // Utility function: Computes the dot product of two vec3 vectors (a . b)
    // and returns the result.
    static inline float dot(const vec3 a, const vec3 b)
    {
#if HAS_AVX2
        return dot_avx2(a, b);
#else
        return a.x * b.x + a.y * b.y + a.z * b.z;
#endif
    }

    //------------------------------------------------------------------------------
    // Utility function: Rotates a vec3 vector v around an axis by rad radians
    // and returns the result.
    static inline vec3 rotate(const vec3 v, const vec3 axis, float rad)
    {
        vec3 normalized_axis = normalize(axis);
        float cos_theta = cosf(rad);
        float sin_theta = sinf(rad);

        // Rodrigues' rotation formula
        vec3 term1 = {v.x * cos_theta, v.y * cos_theta, v.z * cos_theta};
        vec3 term2 = cross(normalized_axis, v);
        term2.x *= sin_theta;
        term2.y *= sin_theta;
        term2.z *= sin_theta;
        vec3 term3 = normalized_axis;
        float dot_product = dot(normalized_axis, v);
        term3.x *= dot_product * (1.0f - cos_theta);
        term3.y *= dot_product * (1.0f - cos_theta);
        term3.z *= dot_product * (1.0f - cos_theta);

        vec3 result;
        result.x = term1.x + term2.x + term3.x;
        result.y = term1.y + term2.y + term3.y;
        result.z = term1.z + term2.z + term3.z;

        return result;
    }
}

namespace matrix4
{
    static inline mat4 identity()
    {
        mat4 matrix = {};
        float temp[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};

        memcpy(&matrix.m, temp, 16 * sizeof(float));

        return matrix;
    }

#if HAS_AVX2
    // AVX2-optimized matrix multiplication - only used internally
    static inline mat4 mat4_mult_avx2(const mat4 &a, const mat4 &b)
    {
        mat4 result = {}; // Initialize result to zeros

        // Process each column of the result
        for (int col = 0; col < 4; ++col)
        {
            // Extract the column from matrix b
            __m128 b_col = _mm_set_ps(b.m[col].w, b.m[col].z, b.m[col].y, b.m[col].x);

            // For each row of matrix a
            for (int row = 0; row < 4; ++row)
            {
                // Process row of matrix a
                __m128 a_row = _mm_set_ps(a.m[3].data[row], a.m[2].data[row],
                                          a.m[1].data[row], a.m[0].data[row]);

                // Calculate dot product of the row and column
                __m128 mul = _mm_mul_ps(a_row, b_col);
                __m128 sum = _mm_hadd_ps(mul, mul);
                sum = _mm_hadd_ps(sum, sum);

                // Store the result in the output matrix
                float dot_result;
                _mm_store_ss(&dot_result, sum);

                // Assign to appropriate component based on row
                switch (row)
                {
                case 0:
                    result.m[col].x = dot_result;
                    break;
                case 1:
                    result.m[col].y = dot_result;
                    break;
                case 2:
                    result.m[col].z = dot_result;
                    break;
                case 3:
                    result.m[col].w = dot_result;
                    break;
                }
            }
        }

        return result;
    }

    // AVX2-optimized matrix-vector multiplication - only used internally
    static inline vec4 mat4_mult_vec4_avx2(const mat4 &m, const vec4 &v)
    {
        // Extract the vector components
        __m128 vec = _mm_set_ps(v.w, v.z, v.y, v.x);

        // Process each component of the resulting vector
        __m128 result = _mm_setzero_ps();

        for (int i = 0; i < 4; ++i)
        {
            // Load column i from the matrix
            __m128 col = _mm_set_ps(m.m[i].w, m.m[i].z, m.m[i].y, m.m[i].x);

            // Multiply by corresponding vector component and add to result
            __m128 component = _mm_set1_ps(v.data[i]);
            result = _mm_add_ps(result, _mm_mul_ps(col, component));
        }

        // Store the result
        vec4 output;
        _mm_storeu_ps(output.data, result);

        return output;
    }
#endif

    // Matrix multiplication function that will use AVX2 if available
    mat4 mat4_mult(const mat4 &a, const mat4 &b)
    {
#if HAS_AVX2
        return mat4_mult_avx2(a, b);
#else
        mat4 result = {};
        for (int col = 0; col < 4; ++col) // Iterate over columns of the result
        {
            for (int row = 0; row < 4; ++row) // Iterate over rows of the result
            {
                result.m[col].data[row] = a.m[0].data[row] * b.m[col].x +
                                          a.m[1].data[row] * b.m[col].y +
                                          a.m[2].data[row] * b.m[col].z +
                                          a.m[3].data[row] * b.m[col].w;
            }
        }
        return result;
#endif
    }

    // Build a scale matrix
    mat4 mat4_scale(vec3 s)
    {
        mat4 out = identity();
        out.m[0].x = s.x;
        out.m[1].y = s.y;
        out.m[2].z = s.z;
        return out;
    }

    // Build a translation matrix
    mat4 mat4_translate(vec3 t)
    {
        mat4 out = identity();
        out.m[3].x = t.x;
        out.m[3].y = t.y;
        out.m[3].z = t.z;
        return out;
    }

    // Build a rotation matrix around X axis
    mat4 mat4_rotate_x(float angle)
    {
        mat4 out = identity();
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
        mat4 out = identity();
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
        mat4 out = identity();
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
#if HAS_AVX2
        return mat4_mult_vec4_avx2(m, v);
#else
        vec4 result;
        result.x = m.m[0].x * v.x + m.m[1].x * v.y + m.m[2].x * v.z + m.m[3].x * v.w;
        result.y = m.m[0].y * v.x + m.m[1].y * v.y + m.m[2].y * v.z + m.m[3].y * v.w;
        result.z = m.m[0].z * v.x + m.m[1].z * v.y + m.m[2].z * v.z + m.m[3].z * v.w;
        result.w = m.m[0].w * v.x + m.m[1].w * v.y + m.m[2].w * v.z + m.m[3].w * v.w;
        return result;
#endif
    }

    mat4 perspective_matrix(float width, float height, float fov, float near_plane, float far_plane)
    {
        mat4 matrix = {};

        float aspect_ratio = width / height;
        float fov_rad = 1.0f / tanf(fov * 0.5f * (float)PI(1.f) / 180.0f); // Convert FOV to radians and calculate cotangent

        matrix.m[0].x = fov_rad / aspect_ratio;
        matrix.m[1].y = fov_rad;
        matrix.m[2].z = -(far_plane + near_plane) / (far_plane - near_plane);
        matrix.m[2].w = -1.0f;
        matrix.m[3].z = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);

        return matrix;
    }

    // Rotate around a given axis by an angle (in radians) using Rodrigues' formula.
    // The resulting matrix rotates vectors in column-major order.
    struct mat4 rotate_around_axis(vec3 axis, float angle)
    {
        float s = sinf(angle);
        float c = cosf(angle);
        float t = 1.0f - c;

        // For clarity, assign axis components to variables.
        float x = axis.x;
        float y = axis.y;
        float z = axis.z;

        // Create the rotation matrix using Rodrigues' formula.
        // In a matrix written in row-major order, we would have:
        //   R00 = t*x*x + c,    R01 = t*x*y - s*z,   R02 = t*x*z + s*y
        //   R10 = t*x*y + s*z,   R11 = t*y*y + c,     R12 = t*y*z - s*x
        //   R20 = t*x*z - s*y,   R21 = t*y*z + s*x,   R22 = t*z*z + c
        //
        // Since we are using a column-major mat4 (each vec4 m[i] is a column),
        // we assign the values as follows:
        mat4 R = {};

        // First column (column 0)
        R.m[0].x = t * x * x + c;     // R00
        R.m[0].y = t * x * y + s * z; // R10
        R.m[0].z = t * x * z - s * y; // R20
        R.m[0].w = 0.0f;

        // Second column (column 1)
        R.m[1].x = t * x * y - s * z; // R01
        R.m[1].y = t * y * y + c;     // R11
        R.m[1].z = t * y * z + s * x; // R21
        R.m[1].w = 0.0f;

        // Third column (column 2)
        R.m[2].x = t * x * z + s * y; // R02
        R.m[2].y = t * y * z - s * x; // R12
        R.m[2].z = t * z * z + c;     // R22
        R.m[2].w = 0.0f;

        // Fourth column (column 3) remains unchanged for affine transformations.
        R.m[3].x = 0.0f;
        R.m[3].y = 0.0f;
        R.m[3].z = 0.0f;
        R.m[3].w = 1.0f;

        return R;
    }

    mat4 rotate_to(vec3 from, vec3 to)
    {
        // Calculate the desired direction vector and normalize it.
        vec3 targetDir = v3::normalize(to - from);
        // The original up vector we want to rotate. (0,1,0)
        vec3 origUp = {0.0f, 1.0f, 0.0f};
        const float EPSILON = 1e-6f;

        // Compute the dot product to determine the angle relative to origUp.
        float dot_val = v3::dot(origUp, targetDir);

        // If the vectors are nearly identical, return the identity matrix.
        if (fabsf(dot_val - 1.0f) < EPSILON)
        {
            return identity();
        }

        // If the vectors are opposite, we need a 180-degree rotation.
        // Choose an arbitrary axis perpendicular to origUp.
        if (fabsf(dot_val + 1.0f) < EPSILON)
        {
            vec3 axis = {1.0f, 0.0f, 0.0f};
            // If origUp is (0,1,0), this axis is fine.

            return rotate_around_axis(axis, 3.14159265358979323846f);
        }

        // Calculate the rotation axis: cross product of the original up and target direction.
        vec3 axis = v3::normalize(v3::cross(origUp, targetDir));
        // Compute the angle to rotate.
        float angle = acosf(dot_val);

        // Create the rotation matrix that rotates origUp to targetDir.
        return rotate_around_axis(axis, angle);
    }
}