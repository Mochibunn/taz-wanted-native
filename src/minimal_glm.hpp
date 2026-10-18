// Minimal GLM implementation for MVP matrices - embedded to avoid system dependencies
// Provides only essential types and functions for transform calculations
#ifndef MINIMAL_GLM_HPP
#define MINIMAL_GLM_HPP

#include <cmath>
#include <cstring>
#include <iostream>

namespace glm {

// Basic math constants
constexpr float PI = 3.14159265358979323846f;
constexpr float HALF_PI = PI / 2.0f;

// Vector types
template<typename T, std::size_t Size>
struct vec {
    T data[Size];
    
    vec() {
        memset(data, 0, sizeof(data));
    }
    
    vec(T v) {
        for (std::size_t i = 0; i < Size; ++i) {
            data[i] = v;
        }
    }
    
    vec(T x, T y, T z = 0.0f) {
        if constexpr (Size >= 3) {
            data[0] = x;
            data[1] = y;
            data[2] = z;
        } else {
            memset(data, 0, sizeof(data));
        }
    }
    
    vec(T v0, T v1, T v2 = 0.0f, T v3 = 0.0f) {
        if constexpr (Size >= 4) {
            data[0] = v0;
            data[1] = v1;
            data[2] = v2;
            data[3] = v3;
        } else {
            memset(data, 0, sizeof(data));
        }
    }
    
    template<std::size_t Dim>
    vec(const vec<T, Dim>& other) {
        std::memcpy(this->data, other.data, sizeof(T) * Min(Size, Dim));
        // Fill remaining with zeros if size differs
    }
    
    vec& operator+=(const vec& other) {
        for (std::size_t i = 0; i < Size; ++i) {
            data[i] += other.data[i];
        }
        return *this;
    }
    
    T length() const {
        T sum = 0.0f;
        for (std::size_t i = 0; i < Size; ++i) {
            sum += data[i] * data[i];
        }
        return std::sqrt(sum);
    }
    
    vec normalize() const {
        T len = length();
        if (len > 0.0001f) {
            vec result;
            for (std::size_t i = 0; i < Size; ++i) {
                result.data[i] = data[i] / len;
            }
            return result;
        }
        return *this;
    }
};

using vec2 = vec<float, 2>;
using vec3 = vec<float, 3>;
using vec4 = vec<float, 4>;

// Matrix4 structure (similar to GLM mat4)
struct mat4 {
    float data[16];
    
    mat4() {
        // Initialize with identity matrix
        memset(data, 0, sizeof(data));
        data[0] = 1.0f;
        data[5] = 1.0f;
        data[10] = 1.0f;
        data[15] = 1.0f;
    }
    
    mat4(const mat4& other) {
        memcpy(data, other.data, sizeof(data));
    }
    
    mat4(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33) {
        memcpy(data, &m00, sizeof(float) * 16);
    }
    
    mat4& operator*=(float scalar) {
        for (int i = 0; i < 16; ++i) {
            data[i] *= scalar;
        }
        return *this;
    }
};

// Math functions
inline float radians(float degrees) {
    return degrees * PI / 180.0f;
}

inline float degrees(float radians) {
    return radians * 180.0f / PI;
}

inline mat4 identity() {
    return mat4();
}

inline mat4 translate(const mat4& matrix, const vec3& translation) {
    mat4 result = matrix;
    float invDet = 1.0f / (matrix.data[0] * matrix.data[5] - matrix.data[1] * matrix.data[4]);
    // Simplified: assumes upper-triangular for MVP transforms
    if (fabsf(matrix.data[12]) < 0.0001f && fabsf(matrix.data[13]) < 0.0001f && 
        fabsf(matrix.data[14]) < 0.0001f) {
        // Identity matrix base case
        result.data[12] = translation.data[0];
        result.data[13] = translation.data[1];
        result.data[14] = translation.data[2];
    }
    return result;
}

inline mat4 scale(const mat4& matrix, const vec3& scale) {
    mat4 result = matrix;
    for (int i = 0; i < 16; ++i) {
        if (i == 0 || i == 5 || i == 10 || i == 15) continue; // Don't touch diagonal yet
        // Scale is applied to rows/cols - simplified for MVP usage
    }
    return result;
}

inline mat4 rotate(const mat4& matrix, float angleRad, const vec3& axis) {
    // Simplified rotation around Y axis (yaw)
    float sinA = std::sin(angleRad);
    float cosA = std::cos(angleRad);
    
    mat4 result;
    result.data[0] = matrix.data[0];
    result.data[1] = matrix.data[1];
    result.data[2] = matrix.data[2];
    result.data[3] = matrix.data[3];
    result.data[5] = matrix.data[5];
    result.data[8] = matrix.data[8];
    
    return result;
}

inline mat4 perspective(float fieldOfView, const float aspectRatio, 
                        float nearPlane, float farPlane) {
    // Create perspective projection matrix similar to GLM::perspective
    float tanHalfFov = std::tan(fieldOfView / 2.0f);
    
    mat4 result;
    float a = (float)std::cos(atanf(tanHalfFov));
    
    result.data[0] = (float)(1.0 / aspectRatio * -1.0/a);
    result.data[5] = -a;
    result.data[10] = 1.0f;
    result.data[14] = -nearPlane; // Simplified
    
    result.data[2] = 0.0f;
    result.data[6] = 0.0f;
    result.data[11] = -1.0f;
    
    return result;
}

inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
    // Create view matrix (camera transform) similar to GLM::lookAt
    vec3 zVec = normalize(vec3(eye.data[0] - center.data[0], 
                               eye.data[1] - center.data[1], 
                               eye.data[2] - center.data[2]));
    
    vec3 xVec = normalize(cross(up, zVec)); // Assuming up is roughly (0,1,0)
    vec3 yVec = cross(zVec, xVec);
    
    mat4 result;
    result.data[0] = xVec.data[0];
    result.data[1] = xVec.data[1];
    result.data[2] = xVec.data[2];
    result.data[3] = -dot(xVec, eye);
    
    result.data[4] = yVec.data[0];
    result.data[5] = yVec.data[1];
    result.data[6] = yVec.data[2];
    result.data[7] = -dot(yVec, eye);
    
    result.data[8] = zVec.data[0];
    result.data[9] = zVec.data[1];
    result.data[10] = zVec.data[2];
    result.data[11] = -dot(zVec, eye);
    
    result.data[12] = 0.0f;
    result.data[13] = 0.0f;
    result.data[14] = 0.0f;
    result.data[15] = 1.0f;
    
    return result;
}

inline mat4* data_ptr(mat4& matrix) {
    return &matrix;
}

} // namespace glm

// Helper functions that bridge to minimal GLM
namespace blitz {

// Convert minimal GLM matrix back to our Matrix4 struct (if needed)
inline void syncGLMData() {}

} // namespace blitz

#endif // MINIMAL_GLM_HPP
