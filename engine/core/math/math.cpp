#include "math.h"
#include "../logging.h"

namespace action {

mat4 mat4::translate(const vec3& t) {
    mat4 m;
    m.columns[3] = {t.x, t.y, t.z, 1.0f};
    return m;
}

mat4 mat4::scale(const vec3& s) {
    mat4 m;
    m.columns[0].x = s.x;
    m.columns[1].y = s.y;
    m.columns[2].z = s.z;
    return m;
}

mat4 mat4::rotate(const quat& q) {
    mat4 m;
    
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;
    
    m.columns[0] = {1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f};
    m.columns[1] = {2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f};
    m.columns[2] = {2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f};
    m.columns[3] = {0.0f, 0.0f, 0.0f, 1.0f};
    
    return m;
}

mat4 mat4::perspective(float fov, float aspect, float near_plane, float far_plane) {
    mat4 m;
    
    float tan_half_fov = std::tan(fov * 0.5f);
    
    m.columns[0] = {1.0f / (aspect * tan_half_fov), 0.0f, 0.0f, 0.0f};
    m.columns[1] = {0.0f, -1.0f / tan_half_fov, 0.0f, 0.0f};  // Negative for Vulkan Y-flip
    m.columns[2] = {0.0f, 0.0f, far_plane / (near_plane - far_plane), -1.0f};
    m.columns[3] = {0.0f, 0.0f, (near_plane * far_plane) / (near_plane - far_plane), 0.0f};
    
    return m;
}

mat4 mat4::look_at(const vec3& eye, const vec3& target, const vec3& up) {
    vec3 f = normalize(target - eye);
    vec3 r = normalize(cross(f, up));
    vec3 u = cross(r, f);
    
    mat4 m;
    m.columns[0] = {r.x, u.x, -f.x, 0.0f};
    m.columns[1] = {r.y, u.y, -f.y, 0.0f};
    m.columns[2] = {r.z, u.z, -f.z, 0.0f};
    m.columns[3] = {-dot(r, eye), -dot(u, eye), dot(f, eye), 1.0f};
    
    return m;
}

mat4 mat4::operator*(const mat4& o) const {
    mat4 result;
    
#ifdef __SSE__
    // SSE-optimized 4x4 matrix multiply
    // Each column of result = this * column of o
    for (int c = 0; c < 4; ++c) {
        __m128 col = _mm_loadu_ps(&o.columns[c].x);
        
        // Broadcast each component and multiply-add
        __m128 x = _mm_shuffle_ps(col, col, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 y = _mm_shuffle_ps(col, col, _MM_SHUFFLE(1, 1, 1, 1));
        __m128 z = _mm_shuffle_ps(col, col, _MM_SHUFFLE(2, 2, 2, 2));
        __m128 w = _mm_shuffle_ps(col, col, _MM_SHUFFLE(3, 3, 3, 3));
        
        __m128 c0 = _mm_loadu_ps(&columns[0].x);
        __m128 c1 = _mm_loadu_ps(&columns[1].x);
        __m128 c2 = _mm_loadu_ps(&columns[2].x);
        __m128 c3 = _mm_loadu_ps(&columns[3].x);
        
        __m128 res = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(c0, x), _mm_mul_ps(c1, y)),
            _mm_add_ps(_mm_mul_ps(c2, z), _mm_mul_ps(c3, w))
        );
        
        _mm_storeu_ps(&result.columns[c].x, res);
    }
#else
    // Scalar fallback
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += (&columns[k].x)[r] * (&o.columns[c].x)[k];
            }
            (&result.columns[c].x)[r] = sum;
        }
    }
#endif
    
    return result;
}

vec4 mat4::operator*(const vec4& v) const {
    return {
        columns[0].x * v.x + columns[1].x * v.y + columns[2].x * v.z + columns[3].x * v.w,
        columns[0].y * v.x + columns[1].y * v.y + columns[2].y * v.z + columns[3].y * v.w,
        columns[0].z * v.x + columns[1].z * v.y + columns[2].z * v.z + columns[3].z * v.w,
        columns[0].w * v.x + columns[1].w * v.y + columns[2].w * v.z + columns[3].w * v.w
    };
}

mat4 mat4::transpose() const {
    mat4 result;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            (&result.columns[r].x)[c] = (&columns[c].x)[r];
        }
    }
    return result;
}

mat4 mat4::inverse() const {
    // Optimized 4x4 matrix inverse using cofactor expansion
    const float* src = &columns[0].x;
    float inv[16];
    
    inv[0] = src[5] * src[10] * src[15] - src[5] * src[11] * src[14] - src[9] * src[6] * src[15] +
             src[9] * src[7] * src[14] + src[13] * src[6] * src[11] - src[13] * src[7] * src[10];
    
    inv[4] = -src[4] * src[10] * src[15] + src[4] * src[11] * src[14] + src[8] * src[6] * src[15] -
              src[8] * src[7] * src[14] - src[12] * src[6] * src[11] + src[12] * src[7] * src[10];
    
    inv[8] = src[4] * src[9] * src[15] - src[4] * src[11] * src[13] - src[8] * src[5] * src[15] +
             src[8] * src[7] * src[13] + src[12] * src[5] * src[11] - src[12] * src[7] * src[9];
    
    inv[12] = -src[4] * src[9] * src[14] + src[4] * src[10] * src[13] + src[8] * src[5] * src[14] -
               src[8] * src[6] * src[13] - src[12] * src[5] * src[10] + src[12] * src[6] * src[9];
    
    inv[1] = -src[1] * src[10] * src[15] + src[1] * src[11] * src[14] + src[9] * src[2] * src[15] -
              src[9] * src[3] * src[14] - src[13] * src[2] * src[11] + src[13] * src[3] * src[10];
    
    inv[5] = src[0] * src[10] * src[15] - src[0] * src[11] * src[14] - src[8] * src[2] * src[15] +
             src[8] * src[3] * src[14] + src[12] * src[2] * src[11] - src[12] * src[3] * src[10];
    
    inv[9] = -src[0] * src[9] * src[15] + src[0] * src[11] * src[13] + src[8] * src[1] * src[15] -
              src[8] * src[3] * src[13] - src[12] * src[1] * src[11] + src[12] * src[3] * src[9];
    
    inv[13] = src[0] * src[9] * src[14] - src[0] * src[10] * src[13] - src[8] * src[1] * src[14] +
              src[8] * src[2] * src[13] + src[12] * src[1] * src[10] - src[12] * src[2] * src[9];
    
    inv[2] = src[1] * src[6] * src[15] - src[1] * src[7] * src[14] - src[5] * src[2] * src[15] +
             src[5] * src[3] * src[14] + src[13] * src[2] * src[7] - src[13] * src[3] * src[6];
    
    inv[6] = -src[0] * src[6] * src[15] + src[0] * src[7] * src[14] + src[4] * src[2] * src[15] -
              src[4] * src[3] * src[14] - src[12] * src[2] * src[7] + src[12] * src[3] * src[6];
    
    inv[10] = src[0] * src[5] * src[15] - src[0] * src[7] * src[13] - src[4] * src[1] * src[15] +
              src[4] * src[3] * src[13] + src[12] * src[1] * src[7] - src[12] * src[3] * src[5];
    
    inv[14] = -src[0] * src[5] * src[14] + src[0] * src[6] * src[13] + src[4] * src[1] * src[14] -
               src[4] * src[2] * src[13] - src[12] * src[1] * src[6] + src[12] * src[2] * src[5];
    
    inv[3] = -src[1] * src[6] * src[11] + src[1] * src[7] * src[10] + src[5] * src[2] * src[11] -
              src[5] * src[3] * src[10] - src[9] * src[2] * src[7] + src[9] * src[3] * src[6];
    
    inv[7] = src[0] * src[6] * src[11] - src[0] * src[7] * src[10] - src[4] * src[2] * src[11] +
             src[4] * src[3] * src[10] + src[8] * src[2] * src[7] - src[8] * src[3] * src[6];
    
    inv[11] = -src[0] * src[5] * src[11] + src[0] * src[7] * src[9] + src[4] * src[1] * src[11] -
               src[4] * src[3] * src[9] - src[8] * src[1] * src[7] + src[8] * src[3] * src[5];
    
    inv[15] = src[0] * src[5] * src[10] - src[0] * src[6] * src[9] - src[4] * src[1] * src[10] +
              src[4] * src[2] * src[9] + src[8] * src[1] * src[6] - src[8] * src[2] * src[5];
    
    float det = src[0] * inv[0] + src[1] * inv[4] + src[2] * inv[8] + src[3] * inv[12];
    
    mat4 result;
    if (std::abs(det) > EPSILON) {
        float inv_det = 1.0f / det;
        for (int i = 0; i < 16; ++i) {
            (&result.columns[0].x)[i] = inv[i] * inv_det;
        }
    } else {
        // Singular matrix: cannot be inverted.  Log a warning and return identity
        // so callers receive a safe fallback rather than garbled transform data.
        LOG_WARN("mat4::inverse(): matrix is singular (|det| = {:.6f}), returning identity", std::abs(det));
    }
    
    return result;
}

// Frustum
Frustum Frustum::from_view_proj(const mat4& vp) {
    Frustum f;
    const float* m = &vp.columns[0].x;
    
    // Left plane
    f.planes[0] = {m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]};
    // Right plane
    f.planes[1] = {m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]};
    // Bottom plane
    f.planes[2] = {m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]};
    // Top plane
    f.planes[3] = {m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]};
    // Near plane
    f.planes[4] = {m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]};
    // Far plane
    f.planes[5] = {m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]};
    
    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(f.planes[i].x * f.planes[i].x + 
                              f.planes[i].y * f.planes[i].y + 
                              f.planes[i].z * f.planes[i].z);
        if (len > EPSILON) {
            float inv = 1.0f / len;
            f.planes[i].x *= inv;
            f.planes[i].y *= inv;
            f.planes[i].z *= inv;
            f.planes[i].w *= inv;
        }
    }
    
    return f;
}

bool Frustum::contains(const vec3& point) const {
    for (int i = 0; i < 6; ++i) {
        float d = planes[i].x * point.x + planes[i].y * point.y + 
                  planes[i].z * point.z + planes[i].w;
        if (d < 0) return false;
    }
    return true;
}

bool Frustum::intersects(const AABB& aabb) const {
    for (int i = 0; i < 6; ++i) {
        vec3 positive;
        positive.x = planes[i].x > 0 ? aabb.max.x : aabb.min.x;
        positive.y = planes[i].y > 0 ? aabb.max.y : aabb.min.y;
        positive.z = planes[i].z > 0 ? aabb.max.z : aabb.min.z;
        
        float d = planes[i].x * positive.x + planes[i].y * positive.y + 
                  planes[i].z * positive.z + planes[i].w;
        if (d < 0) return false;
    }
    return true;
}

bool Frustum::intersects(const Sphere& sphere) const {
    for (int i = 0; i < 6; ++i) {
        float d = planes[i].x * sphere.center.x + planes[i].y * sphere.center.y + 
                  planes[i].z * sphere.center.z + planes[i].w;
        if (d < -sphere.radius) return false;
    }
    return true;
}

} // namespace action
