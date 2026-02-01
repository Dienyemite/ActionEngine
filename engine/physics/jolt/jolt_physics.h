#pragma once

/*
 * Jolt Physics Integration for ActionEngine
 * 
 * This wraps Jolt Physics to work with our ECS and provides:
 * - Automatic body management (create/destroy with entities)
 * - Transform synchronization
 * - Collision callbacks to scripts
 * - Debug rendering
 * 
 * Jolt is used by Horizon Forbidden West and provides excellent
 * performance and stability.
 */

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include "physics/collision_shapes.h"
#include "jolt_layers.h"
#include "jolt_debug_renderer.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <memory>
#include <unordered_map>
#include <functional>

namespace action {

// Raycast result matching our existing API
struct JoltRaycastHit {
    vec3 point{0, 0, 0};
    vec3 normal{0, 1, 0};
    float distance = FLT_MAX;
    Entity entity = INVALID_ENTITY;
    JPH::BodyID body_id;
    bool hit = false;
    
    operator bool() const { return hit; }
};

// Collision contact info
struct JoltContactInfo {
    Entity entity_a = INVALID_ENTITY;
    Entity entity_b = INVALID_ENTITY;
    JPH::BodyID body_a;
    JPH::BodyID body_b;
    vec3 contact_point{0, 0, 0};
    vec3 normal{0, 0, 0};
    float penetration = 0.0f;
};

// Configuration for JoltPhysics
struct JoltPhysicsConfig {
    // Performance settings
    uint32_t max_bodies = 10240;
    uint32_t num_body_mutexes = 0;  // 0 = auto
    uint32_t max_body_pairs = 65536;
    uint32_t max_contact_constraints = 10240;
    
    // Simulation settings
    vec3 gravity{0, -25.0f, 0};     // Higher than real for weighty feel
    uint32_t collision_steps = 1;    // Sub-steps per physics update
    
    // Threading
    uint32_t num_threads = 4;
    
    // Memory
    uint32_t temp_allocator_size = 10 * 1024 * 1024;  // 10 MB
};

/*
 * ContactListener - Receives collision events from Jolt
 */
class JoltContactListener : public JPH::ContactListener {
public:
    void SetPhysicsWorld(class JoltPhysics* world) { m_world = world; }
    
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult) override;
    
    virtual void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override;
    
    virtual void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override;
    
    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
    
private:
    class JoltPhysics* m_world = nullptr;
};

/*
 * BodyActivationListener - Tracks which bodies are active
 */
class JoltBodyActivationListener : public JPH::BodyActivationListener {
public:
    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;
    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;
};

/*
 * JoltPhysics - Main physics world using Jolt
 */
class JoltPhysics {
public:
    JoltPhysics() = default;
    ~JoltPhysics();
    
    // Lifecycle
    bool Initialize(ECS* ecs, const JoltPhysicsConfig& config = {});
    void Shutdown();
    
    // Simulation
    void Update(float dt);
    
    // Sync transforms from ECS to Jolt (before simulation)
    void SyncToPhysics();
    
    // Sync transforms from Jolt to ECS (after simulation)
    void SyncFromPhysics();
    
    // ===== Body Management =====
    
    // Create a body for an entity (uses ColliderComponent if present)
    JPH::BodyID CreateBody(Entity entity, bool is_dynamic = true);
    
    // Create a body with explicit settings
    JPH::BodyID CreateBodyWithSettings(Entity entity, const JPH::BodyCreationSettings& settings);
    
    // Destroy body associated with entity
    void DestroyBody(Entity entity);
    
    // Get body ID for entity
    JPH::BodyID GetBodyID(Entity entity) const;
    
    // Get entity for body ID
    Entity GetEntity(JPH::BodyID body_id) const;
    
    // ===== Body Properties =====
    
    void SetPosition(Entity entity, const vec3& position);
    void SetRotation(Entity entity, const vec3& euler_degrees);
    void SetVelocity(Entity entity, const vec3& velocity);
    void SetAngularVelocity(Entity entity, const vec3& angular_velocity);
    
    vec3 GetPosition(Entity entity) const;
    vec3 GetVelocity(Entity entity) const;
    vec3 GetAngularVelocity(Entity entity) const;
    
    void AddForce(Entity entity, const vec3& force);
    void AddImpulse(Entity entity, const vec3& impulse);
    void AddTorque(Entity entity, const vec3& torque);
    
    void SetMotionType(Entity entity, JPH::EMotionType type);
    void SetFriction(Entity entity, float friction);
    void SetRestitution(Entity entity, float restitution);
    void SetGravityFactor(Entity entity, float factor);
    
    // ===== Queries =====
    
    JoltRaycastHit Raycast(const vec3& origin, const vec3& direction, 
                           float max_distance = 1000.0f,
                           JPH::ObjectLayer layer_filter = Layers::STATIC);
    
    std::vector<JoltRaycastHit> RaycastAll(const vec3& origin, const vec3& direction,
                                            float max_distance = 1000.0f);
    
    std::vector<Entity> OverlapSphere(const vec3& center, float radius,
                                       JPH::ObjectLayer layer_filter = Layers::DYNAMIC);
    
    std::vector<Entity> OverlapBox(const vec3& center, const vec3& half_extents,
                                    JPH::ObjectLayer layer_filter = Layers::DYNAMIC);
    
    // ===== Settings =====
    
    void SetGravity(const vec3& gravity);
    vec3 GetGravity() const;
    
    // ===== Collision Callbacks =====
    
    using CollisionCallback = std::function<void(const JoltContactInfo&)>;
    void SetContactAddedCallback(CollisionCallback callback) { m_contact_added_callback = callback; }
    void SetContactPersistedCallback(CollisionCallback callback) { m_contact_persisted_callback = callback; }
    void SetContactRemovedCallback(CollisionCallback callback) { m_contact_removed_callback = callback; }
    
    // ===== Debug =====
    
    void SetDebugRendererEnabled(bool enabled) { m_debug_render_enabled = enabled; }
    bool IsDebugRendererEnabled() const { return m_debug_render_enabled; }
    
    // Access Jolt internals (for advanced use)
    JPH::PhysicsSystem* GetPhysicsSystem() { return m_physics_system.get(); }
    JPH::BodyInterface& GetBodyInterface();
    const JPH::BodyInterface& GetBodyInterface() const;
    
    // Get collision events this frame
    const std::vector<JoltContactInfo>& GetContactsThisFrame() const { return m_contacts_this_frame; }
    void ClearContactsThisFrame() { m_contacts_this_frame.clear(); }
    
    // Called by contact listener
    void OnContactAdded(const JoltContactInfo& info);
    void OnContactPersisted(const JoltContactInfo& info);
    void OnContactRemoved(Entity a, Entity b);
    
    // Convert between coordinate systems (public for listeners)
    static JPH::Vec3 ToJolt(const vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    static JPH::RVec3 ToJoltR(const vec3& v) { return JPH::RVec3(v.x, v.y, v.z); }
    static vec3 FromJolt(const JPH::Vec3& v) { return vec3{v.GetX(), v.GetY(), v.GetZ()}; }
    // Note: When not using double precision, RVec3 == Vec3, so we don't need a separate overload
    
private:
    // Helper to create Jolt shape from ColliderComponent
    JPH::RefConst<JPH::Shape> CreateShapeFromCollider(const ColliderComponent& collider);
    
    ECS* m_ecs = nullptr;
    JoltPhysicsConfig m_config;
    
    // Jolt systems
    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_job_system;
    std::unique_ptr<JPH::PhysicsSystem> m_physics_system;
    
    // Layer interfaces
    std::unique_ptr<BroadPhaseLayerInterfaceImpl> m_broad_phase_layer_interface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_object_vs_broadphase_layer_filter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_object_layer_pair_filter;
    
    // Listeners
    std::unique_ptr<JoltContactListener> m_contact_listener;
    std::unique_ptr<JoltBodyActivationListener> m_activation_listener;
    
    // Entity <-> Body mapping
    std::unordered_map<Entity, JPH::BodyID> m_entity_to_body;
    std::unordered_map<uint32_t, Entity> m_body_to_entity;  // Key is BodyID index
    
    // Collision events
    std::vector<JoltContactInfo> m_contacts_this_frame;
    CollisionCallback m_contact_added_callback;
    CollisionCallback m_contact_persisted_callback;
    CollisionCallback m_contact_removed_callback;
    
    // Debug
    bool m_debug_render_enabled = false;
    std::unique_ptr<JoltDebugRenderer> m_debug_renderer;
    
    // Timing
    float m_accumulator = 0.0f;
    static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
};

} // namespace action
