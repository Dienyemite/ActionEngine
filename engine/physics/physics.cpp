#include "physics.h"
#include "core/logging.h"
#include "core/math/math.h"

namespace action {

bool Physics::Initialize() {
    LOG_INFO("Physics initialized");
    return true;
}

void Physics::Shutdown() {
    LOG_INFO("Physics shutdown");
}

void Physics::Update(float dt) {
    (void)dt;
    // TODO: Physics step
}

RaycastHit Physics::Raycast(const vec3& origin, const vec3& direction, float max_distance) {
    RaycastHit result;
    result.distance = max_distance;
    
    // TODO: Implement actual raycasting against world geometry
    (void)origin;
    (void)direction;
    
    return result;
}

std::vector<RaycastHit> Physics::RaycastAll(const vec3& origin, const vec3& direction, float max_distance) {
    (void)origin;
    (void)direction;
    (void)max_distance;
    return {};
}

RaycastHit Physics::SphereCast(const vec3& origin, float radius, const vec3& direction, float max_distance) {
    (void)origin;
    (void)radius;
    (void)direction;
    (void)max_distance;
    return {};
}

std::vector<Entity> Physics::OverlapSphere(const vec3& center, float radius) {
    (void)center;
    (void)radius;
    return {};
}

std::vector<Entity> Physics::OverlapBox(const AABB& box) {
    (void)box;
    return {};
}

bool Physics::TestAABBAABB(const AABB& a, const AABB& b, CollisionInfo& info) {
    if (!a.intersects(b)) return false;
    
    // Calculate penetration
    vec3 overlap;
    overlap.x = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    overlap.y = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    overlap.z = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
    
    // Find minimum penetration axis
    if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
        info.penetration = overlap.x;
        info.normal = a.center().x < b.center().x ? vec3{-1, 0, 0} : vec3{1, 0, 0};
    } else if (overlap.y <= overlap.z) {
        info.penetration = overlap.y;
        info.normal = a.center().y < b.center().y ? vec3{0, -1, 0} : vec3{0, 1, 0};
    } else {
        info.penetration = overlap.z;
        info.normal = a.center().z < b.center().z ? vec3{0, 0, -1} : vec3{0, 0, 1};
    }
    
    return true;
}

bool Physics::TestSphereSphere(const Sphere& a, const Sphere& b, CollisionInfo& info) {
    vec3 delta = b.center - a.center;
    float dist_sq = delta.length_sq();
    float radius_sum = a.radius + b.radius;
    
    if (dist_sq > radius_sum * radius_sum) return false;
    
    float dist = std::sqrt(dist_sq);
    info.penetration = radius_sum - dist;
    info.normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    info.contact_point = a.center + info.normal * a.radius;
    
    return true;
}

bool Physics::TestSphereAABB(const Sphere& sphere, const AABB& box, CollisionInfo& info) {
    // Find closest point on AABB to sphere center
    vec3 closest;
    closest.x = Clamp(sphere.center.x, box.min.x, box.max.x);
    closest.y = Clamp(sphere.center.y, box.min.y, box.max.y);
    closest.z = Clamp(sphere.center.z, box.min.z, box.max.z);
    
    vec3 delta = sphere.center - closest;
    float dist_sq = delta.length_sq();
    
    if (dist_sq > sphere.radius * sphere.radius) return false;
    
    float dist = std::sqrt(dist_sq);
    info.penetration = sphere.radius - dist;
    info.normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    info.contact_point = closest;
    
    return true;
}

bool Physics::SweptAABB(const AABB& a, const vec3& velocity, const AABB& b,
                         float& t_hit, vec3& normal) {
    // Expand B by A's extents
    AABB expanded;
    vec3 a_half = a.extents();
    expanded.min = b.min - a_half;
    expanded.max = b.max + a_half;
    
    // Ray-AABB intersection from A's center
    vec3 origin = a.center();
    
    float t_min = 0.0f;
    float t_max = 1.0f;
    normal = {0, 0, 0};
    
    for (int i = 0; i < 3; ++i) {
        float vel = (&velocity.x)[i];
        float min_b = (&expanded.min.x)[i];
        float max_b = (&expanded.max.x)[i];
        float pos = (&origin.x)[i];
        
        if (std::abs(vel) < EPSILON) {
            if (pos < min_b || pos > max_b) return false;
        } else {
            float inv_vel = 1.0f / vel;
            float t1 = (min_b - pos) * inv_vel;
            float t2 = (max_b - pos) * inv_vel;
            
            bool flip = t1 > t2;
            if (flip) std::swap(t1, t2);
            
            if (t1 > t_min) {
                t_min = t1;
                normal = {0, 0, 0};
                (&normal.x)[i] = flip ? 1.0f : -1.0f;
            }
            
            t_max = std::min(t_max, t2);
            
            if (t_min > t_max) return false;
        }
    }
    
    t_hit = t_min;
    return t_min < 1.0f && t_min >= 0.0f;
}

} // namespace action
