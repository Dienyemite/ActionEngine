#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "collision_shapes.h"
#include "gameplay/ecs/ecs.h"
#include <vector>
#include <unordered_map>

namespace action {

/*
 * PhysicsWorld - Spatial acceleration for collision queries
 * 
 * Uses a simple grid-based spatial hash for fast broad-phase.
 * Good enough for action games with moderate entity counts.
 */

struct RaycastHit {
    vec3 point{0, 0, 0};
    vec3 normal{0, 1, 0};
    float distance = FLT_MAX;
    Entity entity = INVALID_ENTITY;
    bool hit = false;
    
    operator bool() const { return hit; }
};

struct SweepHit {
    vec3 point{0, 0, 0};
    vec3 normal{0, 1, 0};
    float time = 1.0f;          // 0-1, where along the sweep we hit
    Entity entity = INVALID_ENTITY;
    bool hit = false;
    
    operator bool() const { return hit; }
};

// Collision event for scripts
struct CollisionEvent {
    Entity entity_a = INVALID_ENTITY;
    Entity entity_b = INVALID_ENTITY;
    vec3 contact_point{0, 0, 0};
    vec3 normal{0, 0, 0};
    float penetration = 0.0f;
    bool is_trigger = false;
};

class PhysicsWorld {
public:
    PhysicsWorld() = default;
    ~PhysicsWorld() = default;
    
    bool Initialize(ECS* ecs, float cell_size = 4.0f);
    void Shutdown();
    
    // Rebuild spatial hash (call after entities move)
    void UpdateSpatialHash();
    
    // Add/remove colliders from world
    void AddCollider(Entity entity);
    void RemoveCollider(Entity entity);
    
    // ===== Queries =====
    
    // Raycast against all colliders
    RaycastHit Raycast(const vec3& origin, const vec3& direction, 
                       float max_distance = 1000.0f,
                       CollisionLayer mask = CollisionLayer::All,
                       Entity ignore = INVALID_ENTITY);
    
    // Raycast returning all hits
    std::vector<RaycastHit> RaycastAll(const vec3& origin, const vec3& direction,
                                        float max_distance = 1000.0f,
                                        CollisionLayer mask = CollisionLayer::All);
    
    // Sphere sweep (for projectiles, fast-moving objects)
    SweepHit SphereCast(const vec3& origin, float radius, const vec3& direction,
                        float max_distance,
                        CollisionLayer mask = CollisionLayer::All,
                        Entity ignore = INVALID_ENTITY);
    
    // Capsule sweep (for character controllers)
    SweepHit CapsuleCast(const Capsule& capsule, const vec3& direction,
                         float max_distance,
                         CollisionLayer mask = CollisionLayer::All,
                         Entity ignore = INVALID_ENTITY);
    
    // Overlap queries
    std::vector<Entity> OverlapSphere(const vec3& center, float radius,
                                       CollisionLayer mask = CollisionLayer::All);
    
    std::vector<Entity> OverlapBox(const AABB& box,
                                    CollisionLayer mask = CollisionLayer::All);
    
    std::vector<Entity> OverlapCapsule(const Capsule& capsule,
                                        CollisionLayer mask = CollisionLayer::All);
    
    // Check if a point is inside any collider
    Entity PointTest(const vec3& point, CollisionLayer mask = CollisionLayer::All);
    
    // ===== Collision Detection =====
    
    // Test two entities for collision
    bool TestCollision(Entity a, Entity b, CollisionEvent& out_event);
    
    // Get all collision events this frame
    const std::vector<CollisionEvent>& GetCollisionEvents() const { return m_collision_events; }
    
    // ===== Settings =====
    
    void SetGravity(const vec3& gravity) { m_gravity = gravity; }
    vec3 GetGravity() const { return m_gravity; }
    
private:
    // Spatial hash cell
    struct Cell {
        std::vector<Entity> entities;
    };
    
    // Convert position to cell coordinate
    ivec3 PositionToCell(const vec3& pos) const;
    
    // Get cell hash from coordinates
    size_t CellHash(const ivec3& cell) const;
    
    // Get all cells that an AABB overlaps
    std::vector<ivec3> GetOverlappingCells(const AABB& bounds) const;
    
    // Get potential colliders near a point/bounds
    std::vector<Entity> GetNearbyEntities(const AABB& bounds, 
                                           CollisionLayer mask,
                                           Entity ignore = INVALID_ENTITY);
    
    // Shape-specific collision tests
    bool RayVsSphere(const vec3& origin, const vec3& dir, const Sphere& sphere,
                     float max_dist, float& t, vec3& normal);
    
    bool RayVsAABB(const vec3& origin, const vec3& dir, const AABB& box,
                   float max_dist, float& t, vec3& normal);
    
    bool RayVsCapsule(const vec3& origin, const vec3& dir, const Capsule& capsule,
                      float max_dist, float& t, vec3& normal);
    
    bool SphereVsSphere(const Sphere& a, const Sphere& b, vec3& normal, float& penetration);
    bool SphereVsAABB(const Sphere& sphere, const AABB& box, vec3& normal, float& penetration);
    bool SphereVsCapsule(const Sphere& sphere, const Capsule& capsule, vec3& normal, float& penetration);
    bool CapsuleVsCapsule(const Capsule& a, const Capsule& b, vec3& normal, float& penetration);
    bool CapsuleVsAABB(const Capsule& capsule, const AABB& box, vec3& normal, float& penetration);
    
    // Closest point helpers
    vec3 ClosestPointOnSegment(const vec3& point, const vec3& a, const vec3& b);
    void ClosestPointsBetweenSegments(const vec3& a1, const vec3& a2,
                                       const vec3& b1, const vec3& b2,
                                       vec3& closest_a, vec3& closest_b);
    
    ECS* m_ecs = nullptr;
    
    // Spatial hash
    float m_cell_size = 4.0f;
    float m_inv_cell_size = 0.25f;
    std::unordered_map<size_t, Cell> m_cells;
    
    // All registered colliders
    std::vector<Entity> m_colliders;
    
    // Collision events from this frame
    std::vector<CollisionEvent> m_collision_events;
    
    // Global settings
    vec3 m_gravity{0, -20.0f, 0};  // Slightly higher than real gravity for snappy feel
};

} // namespace action
