#pragma once

#include "core/types.h"
#include "gameplay/ecs/ecs.h"
#include <vector>

namespace action {

/*
 * Simple Physics System
 * 
 * Basic collision detection and response for action games
 * - Swept collision for fast-moving objects
 * - Simple rigid body dynamics
 * - Raycasting for gameplay
 */

struct RaycastHit {
    vec3 point;
    vec3 normal;
    float distance;
    Entity entity = INVALID_ENTITY;
    bool hit = false;
};

struct CollisionInfo {
    Entity entity_a = INVALID_ENTITY;
    Entity entity_b = INVALID_ENTITY;
    vec3 contact_point;
    vec3 normal;
    float penetration;
};

class Physics {
public:
    bool Initialize();
    void Shutdown();
    
    void Update(float dt);
    
    // Raycasting
    RaycastHit Raycast(const vec3& origin, const vec3& direction, float max_distance);
    std::vector<RaycastHit> RaycastAll(const vec3& origin, const vec3& direction, float max_distance);
    
    // Sphere/box casts
    RaycastHit SphereCast(const vec3& origin, float radius, const vec3& direction, float max_distance);
    
    // Overlap tests
    std::vector<Entity> OverlapSphere(const vec3& center, float radius);
    std::vector<Entity> OverlapBox(const AABB& box);
    
private:
    // Collision detection
    bool TestAABBAABB(const AABB& a, const AABB& b, CollisionInfo& info);
    bool TestSphereSphere(const Sphere& a, const Sphere& b, CollisionInfo& info);
    bool TestSphereAABB(const Sphere& sphere, const AABB& box, CollisionInfo& info);
    
    // Swept collision for fast objects
    bool SweptAABB(const AABB& a, const vec3& velocity, const AABB& b, 
                   float& t_hit, vec3& normal);
};

} // namespace action
