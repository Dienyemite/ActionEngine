#pragma once

#include "../types.h"
#include <cmath>
#include <algorithm>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace action {

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = PI * 2.0f;
constexpr float HALF_PI = PI * 0.5f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;
constexpr float EPSILON = 1e-6f;

// Utility functions
inline float Radians(float degrees) { return degrees * DEG_TO_RAD; }
inline float Degrees(float radians) { return radians * RAD_TO_DEG; }

inline float Clamp(float x, float min, float max) {
    return std::max(min, std::min(max, x));
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float SmoothStep(float edge0, float edge1, float x) {
    float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Vec3 implementations
inline float vec3::length() const {
    return std::sqrt(length_sq());
}

inline vec3 vec3::normalized() const {
    float len = length();
    if (len < EPSILON) return {0, 0, 0};
    return *this * (1.0f / len);
}

inline vec3 cross(const vec3& a, const vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float dot(const vec3& a, const vec3& b) {
    return a.dot(b);
}

inline vec3 normalize(const vec3& v) {
    return v.normalized();
}

inline float length(const vec3& v) {
    return v.length();
}

inline float distance(const vec3& a, const vec3& b) {
    return (b - a).length();
}

inline float distance_sq(const vec3& a, const vec3& b) {
    return (b - a).length_sq();
}

// Quaternion implementations
inline quat quat::from_axis_angle(const vec3& axis, float angle) {
    float half_angle = angle * 0.5f;
    float s = std::sin(half_angle);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half_angle)};
}

inline quat quat::from_euler(float pitch, float yaw, float roll) {
    // Convert euler angles (radians) to quaternion
    // Order: YXZ (Yaw-Pitch-Roll) - common for games
    float cy = std::cos(yaw * 0.5f);
    float sy = std::sin(yaw * 0.5f);
    float cp = std::cos(pitch * 0.5f);
    float sp = std::sin(pitch * 0.5f);
    float cr = std::cos(roll * 0.5f);
    float sr = std::sin(roll * 0.5f);

    return quat{
        cy * sp * cr + sy * cp * sr,  // x
        sy * cp * cr - cy * sp * sr,  // y
        cy * cp * sr - sy * sp * cr,  // z
        cy * cp * cr + sy * sp * sr   // w
    };
}

inline quat quat::operator*(const quat& o) const {
    return {
        w * o.x + x * o.w + y * o.z - z * o.y,
        w * o.y - x * o.z + y * o.w + z * o.x,
        w * o.z + x * o.y - y * o.x + z * o.w,
        w * o.w - x * o.x - y * o.y - z * o.z
    };
}

inline vec3 quat::operator*(const vec3& v) const {
    vec3 u{x, y, z};
    float s = w;
    return u * 2.0f * dot(u, v) + v * (s * s - dot(u, u)) + cross(u, v) * 2.0f * s;
}

inline quat quat::normalized() const {
    float len = std::sqrt(x*x + y*y + z*z + w*w);
    if (len < EPSILON) return identity();
    float inv = 1.0f / len;
    return {x * inv, y * inv, z * inv, w * inv};
}

inline quat slerp(const quat& a, const quat& b, float t) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    
    quat b2 = b;
    if (dot < 0.0f) {
        b2 = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    
    if (dot > 0.9995f) {
        // Linear interpolation for small angles
        return quat{
            Lerp(a.x, b2.x, t),
            Lerp(a.y, b2.y, t),
            Lerp(a.z, b2.z, t),
            Lerp(a.w, b2.w, t)
        }.normalized();
    }
    
    float theta = std::acos(dot);
    float sin_theta = std::sin(theta);
    float wa = std::sin((1.0f - t) * theta) / sin_theta;
    float wb = std::sin(t * theta) / sin_theta;
    
    return {
        a.x * wa + b2.x * wb,
        a.y * wa + b2.y * wb,
        a.z * wa + b2.z * wb,
        a.w * wa + b2.w * wb
    };
}

// AABB implementations
inline bool AABB::contains(const vec3& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

inline bool AABB::intersects(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x &&
           min.y <= other.max.y && max.y >= other.min.y &&
           min.z <= other.max.z && max.z >= other.min.z;
}

inline void AABB::expand(const vec3& point) {
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
}

inline void AABB::expand(const AABB& other) {
    expand(other.min);
    expand(other.max);
}

// Sphere implementations
inline bool Sphere::contains(const vec3& point) const {
    return distance_sq(center, point) <= radius * radius;
}

inline bool Sphere::intersects(const Sphere& other) const {
    float r = radius + other.radius;
    return distance_sq(center, other.center) <= r * r;
}

// Ray-AABB intersection using slab method
inline bool Ray::intersects(const AABB& aabb, float& t_min) const {
    float t_near = -FLT_MAX;
    float t_far = FLT_MAX;
    
    for (int i = 0; i < 3; ++i) {
        float origin_i = (&origin.x)[i];
        float dir_i = (&direction.x)[i];
        float min_i = (&aabb.min.x)[i];
        float max_i = (&aabb.max.x)[i];
        
        if (std::abs(dir_i) < EPSILON) {
            // Ray is parallel to slab, check if origin is inside
            if (origin_i < min_i || origin_i > max_i) {
                return false;
            }
        } else {
            float inv_d = 1.0f / dir_i;
            float t1 = (min_i - origin_i) * inv_d;
            float t2 = (max_i - origin_i) * inv_d;
            
            if (t1 > t2) std::swap(t1, t2);
            
            t_near = std::max(t_near, t1);
            t_far = std::min(t_far, t2);
            
            if (t_near > t_far || t_far < 0) {
                return false;
            }
        }
    }
    
    // If t_near < 0, the ray origin is inside the AABB.
    // Return 0 distance (we're already overlapping) so close objects
    // aren't penalized by using the far exit distance.
    t_min = t_near > 0 ? t_near : 0.0f;
    return true;
}

} // namespace action
