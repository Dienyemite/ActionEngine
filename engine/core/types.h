#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <span>
#include <optional>
#include <variant>
#include <functional>

namespace action {

// Basic types
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = size_t;
using isize = ptrdiff_t;

// Byte literals for memory sizes
constexpr size_t operator""_KB(unsigned long long x) { return x * 1024; }
constexpr size_t operator""_MB(unsigned long long x) { return x * 1024 * 1024; }
constexpr size_t operator""_GB(unsigned long long x) { return x * 1024 * 1024 * 1024; }

// SIMD-aligned math types (SSE compatible)
struct alignas(16) vec2 {
    float x, y;
    float _pad[2];
    
    vec2() : x(0), y(0), _pad{0, 0} {}
    vec2(float x_, float y_) : x(x_), y(y_), _pad{0, 0} {}
    
    vec2 operator+(const vec2& o) const { return {x + o.x, y + o.y}; }
    vec2 operator-(const vec2& o) const { return {x - o.x, y - o.y}; }
    vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct alignas(16) vec3 {
    float x, y, z;
    float _pad;
    
    vec3() : x(0), y(0), z(0), _pad(0) {}
    vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_), _pad(0) {}
    
    vec3 operator+(const vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(const vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator-() const { return {-x, -y, -z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    
    float dot(const vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    vec3 cross(const vec3& o) const { 
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; 
    }
    float length_sq() const { return dot(*this); }
    float length() const;
    vec3 normalized() const;
    vec3 Normalized() const { return normalized(); }  // Alias for compatibility
};

struct alignas(16) vec4 {
    float x, y, z, w;
    
    vec4() : x(0), y(0), z(0), w(0) {}
    vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    vec4(const vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    
    vec4 operator+(const vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
};

struct alignas(16) quat {
    float x, y, z, w;
    
    quat() : x(0), y(0), z(0), w(1) {}
    quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    
    static quat identity() { return {0, 0, 0, 1}; }
    static quat from_axis_angle(const vec3& axis, float angle);
    static quat from_euler(float pitch, float yaw, float roll);  // Radians
    
    quat operator*(const quat& o) const;
    vec3 operator*(const vec3& v) const;
    
    quat normalized() const;
    quat conjugate() const { return {-x, -y, -z, w}; }
};

// 3x3 matrix (for 2D transforms)
struct mat3 {
    float m[3][3];
    
    mat3() {
        m[0][0] = 1; m[0][1] = 0; m[0][2] = 0;
        m[1][0] = 0; m[1][1] = 1; m[1][2] = 0;
        m[2][0] = 0; m[2][1] = 0; m[2][2] = 1;
    }
    
    static mat3 Identity() { return {}; }
    
    mat3 operator*(const mat3& o) const {
        mat3 result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = 0;
                for (int k = 0; k < 3; ++k) {
                    result.m[i][j] += m[i][k] * o.m[k][j];
                }
            }
        }
        return result;
    }
};

// Column-major 4x4 matrix
struct alignas(16) mat4 {
    union {
        vec4 columns[4];
        float m[4][4];  // Row-major access: m[row][column]
    };
    
    mat4() {
        columns[0] = {1, 0, 0, 0};
        columns[1] = {0, 1, 0, 0};
        columns[2] = {0, 0, 1, 0};
        columns[3] = {0, 0, 0, 1};
    }
    
    static mat4 identity() { return {}; }
    static mat4 Identity() { return {}; }  // Alias
    static mat4 translate(const vec3& t);
    static mat4 scale(const vec3& s);
    static mat4 rotate(const quat& q);
    static mat4 perspective(float fov, float aspect, float near, float far);
    static mat4 look_at(const vec3& eye, const vec3& target, const vec3& up);
    
    mat4 operator*(const mat4& o) const;
    vec4 operator*(const vec4& v) const;
    
    mat4 inverse() const;
    mat4 Inverse() const { return inverse(); }  // Alias
    mat4 transpose() const;
};

// Axis-aligned bounding box
struct AABB {
    vec3 min;
    vec3 max;
    
    AABB() : min(FLT_MAX, FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
    AABB(const vec3& min_, const vec3& max_) : min(min_), max(max_) {}
    
    vec3 center() const { return (min + max) * 0.5f; }
    vec3 extents() const { return (max - min) * 0.5f; }
    
    bool contains(const vec3& point) const;
    bool intersects(const AABB& other) const;
    void expand(const vec3& point);
    void expand(const AABB& other);
};

// Bounding sphere
struct Sphere {
    vec3 center;
    float radius;
    
    Sphere() : center(), radius(0) {}
    Sphere(const vec3& c, float r) : center(c), radius(r) {}
    
    bool contains(const vec3& point) const;
    bool intersects(const Sphere& other) const;
};

// Standard vertex format for 3D meshes
struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 color;     // Vertex color (or material tint)
    vec3 tangent;   // For normal mapping
    
    Vertex() : position{0,0,0}, normal{0,1,0}, uv{0,0}, color{1,1,1}, tangent{1,0,0} {}
};

// Frustum for culling (6 planes)
struct Frustum {
    vec4 planes[6]; // left, right, bottom, top, near, far (normal.xyz, distance)
    
    static Frustum from_view_proj(const mat4& view_proj);
    
    bool contains(const vec3& point) const;
    bool intersects(const AABB& aabb) const;
    bool intersects(const Sphere& sphere) const;
};

// Handle types for resources
template<typename T>
struct Handle {
    u32 index = UINT32_MAX;
    u32 generation = 0;
    
    bool is_valid() const { return index != UINT32_MAX; }
    bool operator==(const Handle& o) const { return index == o.index && generation == o.generation; }
    bool operator!=(const Handle& o) const { return !(*this == o); }
};

// Forward declarations for handles
struct Mesh;
struct Texture;
struct Material;
struct Shader;

using MeshHandle = Handle<Mesh>;
using TextureHandle = Handle<Texture>;
using MaterialHandle = Handle<Material>;
using ShaderHandle = Handle<Shader>;

// Result type for error handling
template<typename T, typename E = std::string>
using Result = std::variant<T, E>;

template<typename T, typename E>
bool is_ok(const Result<T, E>& r) { return std::holds_alternative<T>(r); }

template<typename T, typename E>
T& get_value(Result<T, E>& r) { return std::get<T>(r); }

template<typename T, typename E>
const E& get_error(const Result<T, E>& r) { return std::get<E>(r); }

} // namespace action
