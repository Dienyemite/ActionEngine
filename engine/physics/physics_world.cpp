#include "physics_world.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace action {

bool PhysicsWorld::Initialize(ECS* ecs, float cell_size) {
    m_ecs = ecs;
    m_cell_size = cell_size;
    m_inv_cell_size = 1.0f / cell_size;
    
    LOG_INFO("[PhysicsWorld] Initialized with cell size: {}", cell_size);
    return true;
}

void PhysicsWorld::Shutdown() {
    m_cells.clear();
    m_colliders.clear();
    m_collision_events.clear();
    LOG_INFO("[PhysicsWorld] Shutdown");
}

void PhysicsWorld::UpdateSpatialHash() {
    // Clear all cells
    for (auto& [hash, cell] : m_cells) {
        cell.entities.clear();
    }
    
    // Re-insert all colliders
    for (Entity entity : m_colliders) {
        if (!m_ecs->IsAlive(entity)) continue;
        
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        AABB bounds = collider->GetWorldBounds(transform->position);
        auto cells = GetOverlappingCells(bounds);
        
        for (const ivec3& cell_coord : cells) {
            size_t hash = CellHash(cell_coord);
            m_cells[hash].entities.push_back(entity);
        }
    }
}

void PhysicsWorld::AddCollider(Entity entity) {
    // Check if already added
    auto it = std::find(m_colliders.begin(), m_colliders.end(), entity);
    if (it == m_colliders.end()) {
        m_colliders.push_back(entity);
    }
}

void PhysicsWorld::RemoveCollider(Entity entity) {
    m_colliders.erase(
        std::remove(m_colliders.begin(), m_colliders.end(), entity),
        m_colliders.end());
}

// ===== Raycast =====

RaycastHit PhysicsWorld::Raycast(const vec3& origin, const vec3& direction,
                                  float max_distance, CollisionLayer mask,
                                  Entity ignore) {
    RaycastHit closest;
    closest.distance = max_distance;
    
    vec3 dir = direction.normalized();
    
    // Get bounds for the ray
    AABB ray_bounds;
    ray_bounds.min.x = std::min(origin.x, origin.x + dir.x * max_distance);
    ray_bounds.min.y = std::min(origin.y, origin.y + dir.y * max_distance);
    ray_bounds.min.z = std::min(origin.z, origin.z + dir.z * max_distance);
    ray_bounds.max.x = std::max(origin.x, origin.x + dir.x * max_distance);
    ray_bounds.max.y = std::max(origin.y, origin.y + dir.y * max_distance);
    ray_bounds.max.z = std::max(origin.z, origin.z + dir.z * max_distance);
    
    auto candidates = GetNearbyEntities(ray_bounds, mask, ignore);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        float t;
        vec3 normal;
        bool hit = false;
        
        switch (collider->type) {
            case ColliderType::Sphere: {
                Sphere sphere = collider->GetWorldSphere(transform->position);
                hit = RayVsSphere(origin, dir, sphere, closest.distance, t, normal);
                break;
            }
            case ColliderType::Box: {
                AABB box = collider->GetWorldBounds(transform->position);
                hit = RayVsAABB(origin, dir, box, closest.distance, t, normal);
                break;
            }
            case ColliderType::Capsule: {
                Capsule capsule = collider->GetWorldCapsule(transform->position);
                hit = RayVsCapsule(origin, dir, capsule, closest.distance, t, normal);
                break;
            }
            default:
                break;
        }
        
        if (hit && t < closest.distance) {
            closest.hit = true;
            closest.distance = t;
            closest.point = origin + dir * t;
            closest.normal = normal;
            closest.entity = entity;
        }
    }
    
    return closest;
}

std::vector<RaycastHit> PhysicsWorld::RaycastAll(const vec3& origin, const vec3& direction,
                                                   float max_distance, CollisionLayer mask) {
    std::vector<RaycastHit> hits;
    vec3 dir = direction.normalized();
    
    AABB ray_bounds;
    ray_bounds.min.x = std::min(origin.x, origin.x + dir.x * max_distance);
    ray_bounds.min.y = std::min(origin.y, origin.y + dir.y * max_distance);
    ray_bounds.min.z = std::min(origin.z, origin.z + dir.z * max_distance);
    ray_bounds.max.x = std::max(origin.x, origin.x + dir.x * max_distance);
    ray_bounds.max.y = std::max(origin.y, origin.y + dir.y * max_distance);
    ray_bounds.max.z = std::max(origin.z, origin.z + dir.z * max_distance);
    
    auto candidates = GetNearbyEntities(ray_bounds, mask);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        float t;
        vec3 normal;
        bool hit = false;
        
        switch (collider->type) {
            case ColliderType::Sphere: {
                Sphere sphere = collider->GetWorldSphere(transform->position);
                hit = RayVsSphere(origin, dir, sphere, max_distance, t, normal);
                break;
            }
            case ColliderType::Box: {
                AABB box = collider->GetWorldBounds(transform->position);
                hit = RayVsAABB(origin, dir, box, max_distance, t, normal);
                break;
            }
            case ColliderType::Capsule: {
                Capsule capsule = collider->GetWorldCapsule(transform->position);
                hit = RayVsCapsule(origin, dir, capsule, max_distance, t, normal);
                break;
            }
            default:
                break;
        }
        
        if (hit) {
            RaycastHit result;
            result.hit = true;
            result.distance = t;
            result.point = origin + dir * t;
            result.normal = normal;
            result.entity = entity;
            hits.push_back(result);
        }
    }
    
    // Sort by distance
    std::sort(hits.begin(), hits.end(), [](const RaycastHit& a, const RaycastHit& b) {
        return a.distance < b.distance;
    });
    
    return hits;
}

// ===== Sphere Cast =====

SweepHit PhysicsWorld::SphereCast(const vec3& origin, float radius, const vec3& direction,
                                   float max_distance, CollisionLayer mask, Entity ignore) {
    SweepHit closest;
    vec3 dir = direction.normalized();
    
    // Expand bounds by radius
    AABB sweep_bounds;
    sweep_bounds.min.x = std::min(origin.x, origin.x + dir.x * max_distance) - radius;
    sweep_bounds.min.y = std::min(origin.y, origin.y + dir.y * max_distance) - radius;
    sweep_bounds.min.z = std::min(origin.z, origin.z + dir.z * max_distance) - radius;
    sweep_bounds.max.x = std::max(origin.x, origin.x + dir.x * max_distance) + radius;
    sweep_bounds.max.y = std::max(origin.y, origin.y + dir.y * max_distance) + radius;
    sweep_bounds.max.z = std::max(origin.z, origin.z + dir.z * max_distance) + radius;
    
    auto candidates = GetNearbyEntities(sweep_bounds, mask, ignore);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        // For sphere cast, we can expand the target shape by our radius
        // and do a ray cast against it
        float t;
        vec3 normal;
        bool hit = false;
        
        switch (collider->type) {
            case ColliderType::Sphere: {
                Sphere expanded;
                expanded.center = transform->position + collider->offset;
                expanded.radius = collider->radius + radius;
                hit = RayVsSphere(origin, dir, expanded, max_distance, t, normal);
                break;
            }
            case ColliderType::Box: {
                // Expand box and round corners (approximate with expanded AABB)
                AABB expanded = collider->GetWorldBounds(transform->position);
                expanded.min = expanded.min - vec3{radius, radius, radius};
                expanded.max = expanded.max + vec3{radius, radius, radius};
                hit = RayVsAABB(origin, dir, expanded, max_distance, t, normal);
                break;
            }
            case ColliderType::Capsule: {
                Capsule expanded = collider->GetWorldCapsule(transform->position);
                expanded.radius += radius;
                hit = RayVsCapsule(origin, dir, expanded, max_distance, t, normal);
                break;
            }
            default:
                break;
        }
        
        if (hit && t < closest.time * max_distance) {
            closest.hit = true;
            closest.time = t / max_distance;
            closest.point = origin + dir * t;
            closest.normal = normal;
            closest.entity = entity;
        }
    }
    
    return closest;
}

// ===== Capsule Cast =====

SweepHit PhysicsWorld::CapsuleCast(const Capsule& capsule, const vec3& direction,
                                    float max_distance, CollisionLayer mask, Entity ignore) {
    SweepHit closest;
    vec3 dir = direction.normalized();
    
    // Get sweep bounds
    AABB start_bounds = capsule.GetBounds();
    AABB end_bounds = capsule.GetBounds();
    end_bounds.min = end_bounds.min + dir * max_distance;
    end_bounds.max = end_bounds.max + dir * max_distance;
    
    AABB sweep_bounds;
    sweep_bounds.min.x = std::min(start_bounds.min.x, end_bounds.min.x);
    sweep_bounds.min.y = std::min(start_bounds.min.y, end_bounds.min.y);
    sweep_bounds.min.z = std::min(start_bounds.min.z, end_bounds.min.z);
    sweep_bounds.max.x = std::max(start_bounds.max.x, end_bounds.max.x);
    sweep_bounds.max.y = std::max(start_bounds.max.y, end_bounds.max.y);
    sweep_bounds.max.z = std::max(start_bounds.max.z, end_bounds.max.z);
    
    auto candidates = GetNearbyEntities(sweep_bounds, mask, ignore);
    
    // Binary search for collision time
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        // Use iterative approach to find first contact
        float lo = 0.0f, hi = closest.time;
        const int iterations = 8;
        
        for (int i = 0; i < iterations; ++i) {
            float mid = (lo + hi) * 0.5f;
            
            Capsule test_capsule = capsule;
            test_capsule.center = capsule.center + dir * (max_distance * mid);
            
            vec3 normal;
            float penetration;
            bool overlap = false;
            
            switch (collider->type) {
                case ColliderType::Sphere: {
                    Sphere sphere = collider->GetWorldSphere(transform->position);
                    overlap = SphereVsCapsule(sphere, test_capsule, normal, penetration);
                    normal = normal * -1.0f; // Flip for capsule perspective
                    break;
                }
                case ColliderType::Box: {
                    AABB box = collider->GetWorldBounds(transform->position);
                    overlap = CapsuleVsAABB(test_capsule, box, normal, penetration);
                    break;
                }
                case ColliderType::Capsule: {
                    Capsule other = collider->GetWorldCapsule(transform->position);
                    overlap = CapsuleVsCapsule(test_capsule, other, normal, penetration);
                    break;
                }
                default:
                    break;
            }
            
            if (overlap) {
                hi = mid;
                closest.hit = true;
                closest.time = mid;
                closest.normal = normal;
                closest.entity = entity;
            } else {
                lo = mid;
            }
        }
    }
    
    if (closest.hit) {
        closest.point = capsule.center + dir * (max_distance * closest.time);
    }
    
    return closest;
}

// ===== Overlap Queries =====

std::vector<Entity> PhysicsWorld::OverlapSphere(const vec3& center, float radius,
                                                  CollisionLayer mask) {
    std::vector<Entity> result;
    
    AABB bounds{
        center - vec3{radius, radius, radius},
        center + vec3{radius, radius, radius}
    };
    
    Sphere test_sphere{center, radius};
    auto candidates = GetNearbyEntities(bounds, mask);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        vec3 normal;
        float penetration;
        bool overlap = false;
        
        switch (collider->type) {
            case ColliderType::Sphere: {
                Sphere sphere = collider->GetWorldSphere(transform->position);
                overlap = SphereVsSphere(test_sphere, sphere, normal, penetration);
                break;
            }
            case ColliderType::Box: {
                AABB box = collider->GetWorldBounds(transform->position);
                overlap = SphereVsAABB(test_sphere, box, normal, penetration);
                break;
            }
            case ColliderType::Capsule: {
                Capsule capsule = collider->GetWorldCapsule(transform->position);
                overlap = SphereVsCapsule(test_sphere, capsule, normal, penetration);
                break;
            }
            default:
                break;
        }
        
        if (overlap) {
            result.push_back(entity);
        }
    }
    
    return result;
}

std::vector<Entity> PhysicsWorld::OverlapBox(const AABB& box, CollisionLayer mask) {
    std::vector<Entity> result;
    auto candidates = GetNearbyEntities(box, mask);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        AABB entity_bounds = collider->GetWorldBounds(transform->position);
        if (box.intersects(entity_bounds)) {
            result.push_back(entity);
        }
    }
    
    return result;
}

std::vector<Entity> PhysicsWorld::OverlapCapsule(const Capsule& capsule, CollisionLayer mask) {
    std::vector<Entity> result;
    AABB bounds = capsule.GetBounds();
    auto candidates = GetNearbyEntities(bounds, mask);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        vec3 normal;
        float penetration;
        bool overlap = false;
        
        switch (collider->type) {
            case ColliderType::Sphere: {
                Sphere sphere = collider->GetWorldSphere(transform->position);
                overlap = SphereVsCapsule(sphere, capsule, normal, penetration);
                break;
            }
            case ColliderType::Box: {
                AABB box = collider->GetWorldBounds(transform->position);
                overlap = CapsuleVsAABB(capsule, box, normal, penetration);
                break;
            }
            case ColliderType::Capsule: {
                Capsule other = collider->GetWorldCapsule(transform->position);
                overlap = CapsuleVsCapsule(capsule, other, normal, penetration);
                break;
            }
            default:
                break;
        }
        
        if (overlap) {
            result.push_back(entity);
        }
    }
    
    return result;
}

Entity PhysicsWorld::PointTest(const vec3& point, CollisionLayer mask) {
    AABB bounds{point, point};
    auto candidates = GetNearbyEntities(bounds, mask);
    
    for (Entity entity : candidates) {
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
        if (!transform || !collider) continue;
        
        vec3 world_center = transform->position + collider->offset;
        
        switch (collider->type) {
            case ColliderType::Sphere: {
                if ((point - world_center).length_sq() <= collider->radius * collider->radius) {
                    return entity;
                }
                break;
            }
            case ColliderType::Box: {
                AABB box = collider->GetWorldBounds(transform->position);
                if (box.contains(point)) {
                    return entity;
                }
                break;
            }
            case ColliderType::Capsule: {
                Capsule capsule = collider->GetWorldCapsule(transform->position);
                vec3 closest = ClosestPointOnSegment(point, capsule.GetBottom(), capsule.GetTop());
                if ((point - closest).length_sq() <= capsule.radius * capsule.radius) {
                    return entity;
                }
                break;
            }
            default:
                break;
        }
    }
    
    return INVALID_ENTITY;
}

// ===== Collision Detection =====

bool PhysicsWorld::TestCollision(Entity a, Entity b, CollisionEvent& out_event) {
    auto* transform_a = m_ecs->GetComponent<TransformComponent>(a);
    auto* collider_a = m_ecs->GetComponent<ColliderComponent>(a);
    auto* transform_b = m_ecs->GetComponent<TransformComponent>(b);
    auto* collider_b = m_ecs->GetComponent<ColliderComponent>(b);
    
    if (!transform_a || !collider_a || !transform_b || !collider_b) return false;
    
    // Check layer masks
    if (!HasLayer(collider_a->mask, collider_b->layer) ||
        !HasLayer(collider_b->mask, collider_a->layer)) {
        return false;
    }
    
    vec3 normal;
    float penetration;
    bool hit = false;
    
    // Test based on shape combinations
    if (collider_a->type == ColliderType::Sphere && collider_b->type == ColliderType::Sphere) {
        Sphere sa = collider_a->GetWorldSphere(transform_a->position);
        Sphere sb = collider_b->GetWorldSphere(transform_b->position);
        hit = SphereVsSphere(sa, sb, normal, penetration);
    }
    else if (collider_a->type == ColliderType::Sphere && collider_b->type == ColliderType::Box) {
        Sphere sphere = collider_a->GetWorldSphere(transform_a->position);
        AABB box = collider_b->GetWorldBounds(transform_b->position);
        hit = SphereVsAABB(sphere, box, normal, penetration);
    }
    else if (collider_a->type == ColliderType::Box && collider_b->type == ColliderType::Sphere) {
        Sphere sphere = collider_b->GetWorldSphere(transform_b->position);
        AABB box = collider_a->GetWorldBounds(transform_a->position);
        hit = SphereVsAABB(sphere, box, normal, penetration);
        normal = normal * -1.0f;
    }
    else if (collider_a->type == ColliderType::Box && collider_b->type == ColliderType::Box) {
        AABB box_a = collider_a->GetWorldBounds(transform_a->position);
        AABB box_b = collider_b->GetWorldBounds(transform_b->position);
        if (box_a.intersects(box_b)) {
            // Simple penetration calculation
            hit = true;
            // Find minimum penetration axis
            vec3 overlap;
            overlap.x = std::min(box_a.max.x, box_b.max.x) - std::max(box_a.min.x, box_b.min.x);
            overlap.y = std::min(box_a.max.y, box_b.max.y) - std::max(box_a.min.y, box_b.min.y);
            overlap.z = std::min(box_a.max.z, box_b.max.z) - std::max(box_a.min.z, box_b.min.z);
            
            if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
                penetration = overlap.x;
                normal = box_a.center().x < box_b.center().x ? vec3{-1, 0, 0} : vec3{1, 0, 0};
            } else if (overlap.y <= overlap.z) {
                penetration = overlap.y;
                normal = box_a.center().y < box_b.center().y ? vec3{0, -1, 0} : vec3{0, 1, 0};
            } else {
                penetration = overlap.z;
                normal = box_a.center().z < box_b.center().z ? vec3{0, 0, -1} : vec3{0, 0, 1};
            }
        }
    }
    else if (collider_a->type == ColliderType::Capsule || collider_b->type == ColliderType::Capsule) {
        // Handle capsule combinations
        if (collider_a->type == ColliderType::Capsule && collider_b->type == ColliderType::Capsule) {
            Capsule ca = collider_a->GetWorldCapsule(transform_a->position);
            Capsule cb = collider_b->GetWorldCapsule(transform_b->position);
            hit = CapsuleVsCapsule(ca, cb, normal, penetration);
        }
        else if (collider_a->type == ColliderType::Capsule && collider_b->type == ColliderType::Sphere) {
            Capsule capsule = collider_a->GetWorldCapsule(transform_a->position);
            Sphere sphere = collider_b->GetWorldSphere(transform_b->position);
            hit = SphereVsCapsule(sphere, capsule, normal, penetration);
            normal = normal * -1.0f;
        }
        else if (collider_a->type == ColliderType::Sphere && collider_b->type == ColliderType::Capsule) {
            Sphere sphere = collider_a->GetWorldSphere(transform_a->position);
            Capsule capsule = collider_b->GetWorldCapsule(transform_b->position);
            hit = SphereVsCapsule(sphere, capsule, normal, penetration);
        }
        else if (collider_a->type == ColliderType::Capsule && collider_b->type == ColliderType::Box) {
            Capsule capsule = collider_a->GetWorldCapsule(transform_a->position);
            AABB box = collider_b->GetWorldBounds(transform_b->position);
            hit = CapsuleVsAABB(capsule, box, normal, penetration);
        }
        else if (collider_a->type == ColliderType::Box && collider_b->type == ColliderType::Capsule) {
            Capsule capsule = collider_b->GetWorldCapsule(transform_b->position);
            AABB box = collider_a->GetWorldBounds(transform_a->position);
            hit = CapsuleVsAABB(capsule, box, normal, penetration);
            normal = normal * -1.0f;
        }
    }
    
    if (hit) {
        out_event.entity_a = a;
        out_event.entity_b = b;
        out_event.normal = normal;
        out_event.penetration = penetration;
        out_event.is_trigger = collider_a->is_trigger || collider_b->is_trigger;
        
        // Approximate contact point
        vec3 center_a = transform_a->position + collider_a->offset;
        out_event.contact_point = center_a + normal * (penetration * 0.5f);
    }
    
    return hit;
}

// ===== Spatial Hash Helpers =====

ivec3 PhysicsWorld::PositionToCell(const vec3& pos) const {
    return ivec3{
        static_cast<i32>(std::floor(pos.x * m_inv_cell_size)),
        static_cast<i32>(std::floor(pos.y * m_inv_cell_size)),
        static_cast<i32>(std::floor(pos.z * m_inv_cell_size))
    };
}

size_t PhysicsWorld::CellHash(const ivec3& cell) const {
    // Simple spatial hash
    size_t h = 0;
    h ^= std::hash<i32>{}(cell.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<i32>{}(cell.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<i32>{}(cell.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

std::vector<ivec3> PhysicsWorld::GetOverlappingCells(const AABB& bounds) const {
    std::vector<ivec3> cells;
    
    ivec3 min_cell = PositionToCell(bounds.min);
    ivec3 max_cell = PositionToCell(bounds.max);
    
    for (i32 x = min_cell.x; x <= max_cell.x; ++x) {
        for (i32 y = min_cell.y; y <= max_cell.y; ++y) {
            for (i32 z = min_cell.z; z <= max_cell.z; ++z) {
                cells.push_back({x, y, z});
            }
        }
    }
    
    return cells;
}

std::vector<Entity> PhysicsWorld::GetNearbyEntities(const AABB& bounds,
                                                      CollisionLayer mask,
                                                      Entity ignore) {
    std::vector<Entity> result;
    
    auto cells = GetOverlappingCells(bounds);
    
    for (const ivec3& cell_coord : cells) {
        size_t hash = CellHash(cell_coord);
        auto it = m_cells.find(hash);
        if (it == m_cells.end()) continue;
        
        for (Entity entity : it->second.entities) {
            if (entity == ignore) continue;
            
            // Check if already in result
            if (std::find(result.begin(), result.end(), entity) != result.end()) continue;
            
            // Check layer
            if (auto* collider = m_ecs->GetComponent<ColliderComponent>(entity)) {
                if (HasLayer(mask, collider->layer)) {
                    result.push_back(entity);
                }
            }
        }
    }
    
    return result;
}

// ===== Ray vs Shape Tests =====

bool PhysicsWorld::RayVsSphere(const vec3& origin, const vec3& dir, const Sphere& sphere,
                                float max_dist, float& t, vec3& normal) {
    vec3 oc = origin - sphere.center;
    float a = dot(dir, dir);
    float b = 2.0f * dot(oc, dir);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) return false;
    
    float sqrt_d = std::sqrt(discriminant);
    t = (-b - sqrt_d) / (2.0f * a);
    
    if (t < 0) {
        t = (-b + sqrt_d) / (2.0f * a);
        if (t < 0) return false;
    }
    
    if (t > max_dist) return false;
    
    vec3 hit_point = origin + dir * t;
    normal = (hit_point - sphere.center).normalized();
    
    return true;
}

bool PhysicsWorld::RayVsAABB(const vec3& origin, const vec3& dir, const AABB& box,
                              float max_dist, float& t, vec3& normal) {
    float t_min = 0.0f;
    float t_max = max_dist;
    normal = {0, 0, 0};
    
    for (int i = 0; i < 3; ++i) {
        float d = (&dir.x)[i];
        float o = (&origin.x)[i];
        float min_b = (&box.min.x)[i];
        float max_b = (&box.max.x)[i];
        
        if (std::abs(d) < EPSILON) {
            if (o < min_b || o > max_b) return false;
        } else {
            float inv_d = 1.0f / d;
            float t1 = (min_b - o) * inv_d;
            float t2 = (max_b - o) * inv_d;
            
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
    
    t = t_min;
    return true;
}

bool PhysicsWorld::RayVsCapsule(const vec3& origin, const vec3& dir, const Capsule& capsule,
                                 float max_dist, float& t, vec3& normal) {
    vec3 top = capsule.GetTop();
    vec3 bottom = capsule.GetBottom();
    
    // Test against infinite cylinder first
    vec3 d = top - bottom;
    vec3 m = origin - bottom;
    vec3 n = dir;
    
    float md = dot(m, d);
    float nd = dot(n, d);
    float dd = dot(d, d);
    
    float nn = dot(n, n);
    float mn = dot(m, n);
    float mm = dot(m, m);
    float r2 = capsule.radius * capsule.radius;
    
    float a = dd * nn - nd * nd;
    float b = dd * mn - nd * md;
    float c = dd * (mm - r2) - md * md;
    
    if (std::abs(a) > EPSILON) {
        float discriminant = b * b - a * c;
        if (discriminant < 0) {
            // Check hemisphere caps
            Sphere top_sphere{top, capsule.radius};
            Sphere bottom_sphere{bottom, capsule.radius};
            
            float t_top, t_bottom;
            vec3 n_top, n_bottom;
            bool hit_top = RayVsSphere(origin, dir, top_sphere, max_dist, t_top, n_top);
            bool hit_bottom = RayVsSphere(origin, dir, bottom_sphere, max_dist, t_bottom, n_bottom);
            
            if (hit_top && (!hit_bottom || t_top < t_bottom)) {
                t = t_top;
                normal = n_top;
                return true;
            }
            if (hit_bottom) {
                t = t_bottom;
                normal = n_bottom;
                return true;
            }
            return false;
        }
        
        float sqrt_d = std::sqrt(discriminant);
        t = (-b - sqrt_d) / a;
        
        if (t < 0 || t > max_dist) {
            t = (-b + sqrt_d) / a;
            if (t < 0 || t > max_dist) return false;
        }
        
        // Check if hit is within cylinder segment
        float y = md + t * nd;
        if (y >= 0 && y <= dd) {
            vec3 hit_point = origin + dir * t;
            vec3 axis_point = bottom + d * (y / dd);
            normal = (hit_point - axis_point).normalized();
            return true;
        }
        
        // Check hemisphere caps
        Sphere cap = (y < 0) ? Sphere{bottom, capsule.radius} : Sphere{top, capsule.radius};
        return RayVsSphere(origin, dir, cap, max_dist, t, normal);
    }
    
    return false;
}

// ===== Shape vs Shape Tests =====

bool PhysicsWorld::SphereVsSphere(const Sphere& a, const Sphere& b, vec3& normal, float& penetration) {
    vec3 delta = b.center - a.center;
    float dist_sq = delta.length_sq();
    float radius_sum = a.radius + b.radius;
    
    if (dist_sq > radius_sum * radius_sum) return false;
    
    float dist = std::sqrt(dist_sq);
    penetration = radius_sum - dist;
    normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    
    return true;
}

bool PhysicsWorld::SphereVsAABB(const Sphere& sphere, const AABB& box, vec3& normal, float& penetration) {
    // Find closest point on box to sphere center
    vec3 closest;
    closest.x = Clamp(sphere.center.x, box.min.x, box.max.x);
    closest.y = Clamp(sphere.center.y, box.min.y, box.max.y);
    closest.z = Clamp(sphere.center.z, box.min.z, box.max.z);
    
    vec3 delta = sphere.center - closest;
    float dist_sq = delta.length_sq();
    
    if (dist_sq > sphere.radius * sphere.radius) return false;
    
    float dist = std::sqrt(dist_sq);
    penetration = sphere.radius - dist;
    normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    
    return true;
}

bool PhysicsWorld::SphereVsCapsule(const Sphere& sphere, const Capsule& capsule,
                                    vec3& normal, float& penetration) {
    vec3 closest = ClosestPointOnSegment(sphere.center, capsule.GetBottom(), capsule.GetTop());
    
    vec3 delta = sphere.center - closest;
    float dist_sq = delta.length_sq();
    float radius_sum = sphere.radius + capsule.radius;
    
    if (dist_sq > radius_sum * radius_sum) return false;
    
    float dist = std::sqrt(dist_sq);
    penetration = radius_sum - dist;
    normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    
    return true;
}

bool PhysicsWorld::CapsuleVsCapsule(const Capsule& a, const Capsule& b,
                                     vec3& normal, float& penetration) {
    vec3 closest_a, closest_b;
    ClosestPointsBetweenSegments(a.GetBottom(), a.GetTop(),
                                  b.GetBottom(), b.GetTop(),
                                  closest_a, closest_b);
    
    vec3 delta = closest_b - closest_a;
    float dist_sq = delta.length_sq();
    float radius_sum = a.radius + b.radius;
    
    if (dist_sq > radius_sum * radius_sum) return false;
    
    float dist = std::sqrt(dist_sq);
    penetration = radius_sum - dist;
    normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    
    return true;
}

bool PhysicsWorld::CapsuleVsAABB(const Capsule& capsule, const AABB& box,
                                  vec3& normal, float& penetration) {
    // Find closest point on capsule segment to box
    vec3 top = capsule.GetTop();
    vec3 bottom = capsule.GetBottom();
    
    // Sample several points along capsule and find closest
    vec3 closest_on_capsule = bottom;
    vec3 closest_on_box;
    float best_dist_sq = FLT_MAX;
    
    for (int i = 0; i <= 4; ++i) {
        float t = i / 4.0f;
        vec3 point = bottom + (top - bottom) * t;
        
        vec3 box_closest;
        box_closest.x = Clamp(point.x, box.min.x, box.max.x);
        box_closest.y = Clamp(point.y, box.min.y, box.max.y);
        box_closest.z = Clamp(point.z, box.min.z, box.max.z);
        
        float dist_sq = (point - box_closest).length_sq();
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            closest_on_capsule = point;
            closest_on_box = box_closest;
        }
    }
    
    // Refine: find closest point on segment to the best box point
    closest_on_capsule = ClosestPointOnSegment(closest_on_box, bottom, top);
    closest_on_box.x = Clamp(closest_on_capsule.x, box.min.x, box.max.x);
    closest_on_box.y = Clamp(closest_on_capsule.y, box.min.y, box.max.y);
    closest_on_box.z = Clamp(closest_on_capsule.z, box.min.z, box.max.z);
    
    vec3 delta = closest_on_capsule - closest_on_box;
    float dist_sq = delta.length_sq();
    
    if (dist_sq > capsule.radius * capsule.radius) return false;
    
    float dist = std::sqrt(dist_sq);
    penetration = capsule.radius - dist;
    normal = dist > EPSILON ? delta * (1.0f / dist) : vec3{0, 1, 0};
    
    return true;
}

// ===== Helpers =====

vec3 PhysicsWorld::ClosestPointOnSegment(const vec3& point, const vec3& a, const vec3& b) {
    vec3 ab = b - a;
    float t = dot(point - a, ab) / dot(ab, ab);
    t = Clamp(t, 0.0f, 1.0f);
    return a + ab * t;
}

void PhysicsWorld::ClosestPointsBetweenSegments(const vec3& a1, const vec3& a2,
                                                  const vec3& b1, const vec3& b2,
                                                  vec3& closest_a, vec3& closest_b) {
    vec3 d1 = a2 - a1;
    vec3 d2 = b2 - b1;
    vec3 r = a1 - b1;
    
    float a = dot(d1, d1);
    float e = dot(d2, d2);
    float f = dot(d2, r);
    
    float s, t;
    
    if (a < EPSILON && e < EPSILON) {
        s = t = 0.0f;
    } else if (a < EPSILON) {
        s = 0.0f;
        t = Clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = dot(d1, r);
        if (e < EPSILON) {
            t = 0.0f;
            s = Clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = dot(d1, d2);
            float denom = a * e - b * b;
            
            if (denom != 0.0f) {
                s = Clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            
            t = (b * s + f) / e;
            
            if (t < 0.0f) {
                t = 0.0f;
                s = Clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = Clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    
    closest_a = a1 + d1 * s;
    closest_b = b1 + d2 * t;
}

} // namespace action
