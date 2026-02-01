/*
 * Jolt Physics Implementation for ActionEngine
 * 
 * Note: Jolt configuration is handled in CMakeLists.txt via FetchContent.
 * Do NOT define JPH_* macros here - they must match the library build.
 */

#include "jolt_physics.h"
#include "core/logging.h"
#include "physics/collision_shapes.h"

#include <cstdarg>
#include <algorithm>

namespace action {

// Jolt memory allocation callbacks
static void* JoltAlloc(size_t inSize) {
    return malloc(inSize);
}

static void JoltFree(void* inBlock) {
    free(inBlock);
}

static void* JoltAlignedAlloc(size_t inSize, size_t inAlignment) {
    return _aligned_malloc(inSize, inAlignment);
}

static void JoltAlignedFree(void* inBlock) {
    _aligned_free(inBlock);
}

// Jolt trace callback for debugging
static void JoltTrace(const char* inFmt, ...) {
    va_list args;
    va_start(args, inFmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFmt, args);
    va_end(args);
    LOG_INFO("[Jolt] {}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
// Jolt assert callback
static bool JoltAssertFailed(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
    LOG_ERROR("[Jolt Assert] {}:{}: ({}) {}", inFile, inLine, inExpression, inMessage ? inMessage : "");
    return true;  // Break into debugger
}
#endif

// ===== Contact Listener Implementation =====

JPH::ValidateResult JoltContactListener::OnContactValidate(
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    JPH::RVec3Arg inBaseOffset,
    const JPH::CollideShapeResult& inCollisionResult) {
    // Accept all contacts by default
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void JoltContactListener::OnContactAdded(
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    const JPH::ContactManifold& inManifold,
    JPH::ContactSettings& ioSettings) {
    if (!m_world) return;
    
    JoltContactInfo info;
    info.body_a = inBody1.GetID();
    info.body_b = inBody2.GetID();
    info.entity_a = m_world->GetEntity(info.body_a);
    info.entity_b = m_world->GetEntity(info.body_b);
    info.contact_point = JoltPhysics::FromJolt(inManifold.GetWorldSpaceContactPointOn1(0));
    info.normal = JoltPhysics::FromJolt(inManifold.mWorldSpaceNormal);
    info.penetration = inManifold.mPenetrationDepth;
    
    m_world->OnContactAdded(info);
}

void JoltContactListener::OnContactPersisted(
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    const JPH::ContactManifold& inManifold,
    JPH::ContactSettings& ioSettings) {
    if (!m_world) return;
    
    JoltContactInfo info;
    info.body_a = inBody1.GetID();
    info.body_b = inBody2.GetID();
    info.entity_a = m_world->GetEntity(info.body_a);
    info.entity_b = m_world->GetEntity(info.body_b);
    info.contact_point = JoltPhysics::FromJolt(inManifold.GetWorldSpaceContactPointOn1(0));
    info.normal = JoltPhysics::FromJolt(inManifold.mWorldSpaceNormal);
    info.penetration = inManifold.mPenetrationDepth;
    
    m_world->OnContactPersisted(info);
}

void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) {
    // We don't have easy access to entities here, so we skip detailed callback
}

// ===== Body Activation Listener Implementation =====

void JoltBodyActivationListener::OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) {
    // Could track active bodies for optimization
}

void JoltBodyActivationListener::OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) {
    // Could track sleeping bodies
}

// ===== JoltPhysics Implementation =====

JoltPhysics::~JoltPhysics() {
    Shutdown();
}

bool JoltPhysics::Initialize(ECS* ecs, const JoltPhysicsConfig& config) {
    m_ecs = ecs;
    m_config = config;
    
    LOG_INFO("[JoltPhysics] Initializing...");
    
    // Register allocation hooks
    JPH::RegisterDefaultAllocator();
    
    // Install trace and assert callbacks
    JPH::Trace = JoltTrace;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = JoltAssertFailed;
#endif
    
    // Create factory (needed for serialization)
    JPH::Factory::sInstance = new JPH::Factory();
    
    // Register physics types
    JPH::RegisterTypes();
    
    // Create temp allocator
    m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(config.temp_allocator_size);
    
    // Create job system
    m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        static_cast<int>(config.num_threads)
    );
    
    // Create layer interfaces
    m_broad_phase_layer_interface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    m_object_vs_broadphase_layer_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    m_object_layer_pair_filter = std::make_unique<ObjectLayerPairFilterImpl>();
    
    // Create physics system
    m_physics_system = std::make_unique<JPH::PhysicsSystem>();
    m_physics_system->Init(
        config.max_bodies,
        config.num_body_mutexes,
        config.max_body_pairs,
        config.max_contact_constraints,
        *m_broad_phase_layer_interface,
        *m_object_vs_broadphase_layer_filter,
        *m_object_layer_pair_filter
    );
    
    // Set gravity
    m_physics_system->SetGravity(ToJolt(config.gravity));
    
    // Create and register listeners
    m_contact_listener = std::make_unique<JoltContactListener>();
    m_contact_listener->SetPhysicsWorld(this);
    m_physics_system->SetContactListener(m_contact_listener.get());
    
    m_activation_listener = std::make_unique<JoltBodyActivationListener>();
    m_physics_system->SetBodyActivationListener(m_activation_listener.get());
    
    LOG_INFO("[JoltPhysics] Initialized successfully");
    LOG_INFO("[JoltPhysics] Max bodies: {}, Gravity: ({}, {}, {})", 
             config.max_bodies, config.gravity.x, config.gravity.y, config.gravity.z);
    
    return true;
}

void JoltPhysics::Shutdown() {
    if (!m_physics_system) return;
    
    LOG_INFO("[JoltPhysics] Shutting down...");
    
    // Remove all bodies
    JPH::BodyInterface& body_interface = m_physics_system->GetBodyInterface();
    for (auto& [entity, body_id] : m_entity_to_body) {
        if (!body_id.IsInvalid()) {
            body_interface.RemoveBody(body_id);
            body_interface.DestroyBody(body_id);
        }
    }
    m_entity_to_body.clear();
    m_body_to_entity.clear();
    
    // Destroy systems in reverse order
    m_debug_renderer.reset();
    m_activation_listener.reset();
    m_contact_listener.reset();
    m_physics_system.reset();
    m_object_layer_pair_filter.reset();
    m_object_vs_broadphase_layer_filter.reset();
    m_broad_phase_layer_interface.reset();
    m_job_system.reset();
    m_temp_allocator.reset();
    
    // Unregister types and destroy factory
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    
    LOG_INFO("[JoltPhysics] Shutdown complete");
}

void JoltPhysics::Update(float dt) {
    if (!m_physics_system) return;
    
    // Clear previous frame contacts
    ClearContactsThisFrame();
    
    // Fixed timestep accumulator
    m_accumulator += dt;
    
    while (m_accumulator >= FIXED_TIMESTEP) {
        // Step the physics simulation
        m_physics_system->Update(
            FIXED_TIMESTEP,
            m_config.collision_steps,
            m_temp_allocator.get(),
            m_job_system.get()
        );
        
        m_accumulator -= FIXED_TIMESTEP;
    }
}

void JoltPhysics::SyncToPhysics() {
    if (!m_physics_system) return;
    
    JPH::BodyInterface& body_interface = m_physics_system->GetBodyInterface();
    
    for (auto& [entity, body_id] : m_entity_to_body) {
        if (body_id.IsInvalid()) continue;
        if (!m_ecs->IsAlive(entity)) continue;
        
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        if (!transform) continue;
        
        // Only sync kinematic bodies from ECS (dynamic bodies are controlled by physics)
        if (body_interface.GetMotionType(body_id) == JPH::EMotionType::Kinematic) {
            body_interface.SetPosition(body_id, ToJoltR(transform->position), JPH::EActivation::DontActivate);
            // Could also sync rotation here
        }
    }
}

void JoltPhysics::SyncFromPhysics() {
    if (!m_physics_system) return;
    
    JPH::BodyInterface& body_interface = m_physics_system->GetBodyInterface();
    
    for (auto& [entity, body_id] : m_entity_to_body) {
        if (body_id.IsInvalid()) continue;
        if (!m_ecs->IsAlive(entity)) continue;
        
        auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
        if (!transform) continue;
        
        // Only sync dynamic bodies to ECS
        if (body_interface.GetMotionType(body_id) == JPH::EMotionType::Dynamic) {
            JPH::RVec3 pos = body_interface.GetCenterOfMassPosition(body_id);
            transform->position = FromJolt(pos);
            
            // Could also sync rotation
            // JPH::Quat rot = body_interface.GetRotation(body_id);
        }
    }
}

JPH::RefConst<JPH::Shape> JoltPhysics::CreateShapeFromCollider(const ColliderComponent& collider) {
    switch (collider.type) {
        case ColliderType::Sphere: {
            return new JPH::SphereShape(collider.radius);
        }
        case ColliderType::Box: {
            return new JPH::BoxShape(ToJolt(collider.half_extents));
        }
        case ColliderType::Capsule: {
            // Jolt capsule is defined by half-height and radius
            float half_height = (collider.height - 2.0f * collider.radius) * 0.5f;
            if (half_height < 0.001f) half_height = 0.001f;
            return new JPH::CapsuleShape(half_height, collider.radius);
        }
        default:
            LOG_WARN("[JoltPhysics] Unknown collider type, using sphere");
            return new JPH::SphereShape(0.5f);
    }
}

JPH::BodyID JoltPhysics::CreateBody(Entity entity, bool is_dynamic) {
    if (!m_physics_system) return JPH::BodyID();
    
    auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
    auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
    
    if (!transform || !collider) {
        LOG_WARN("[JoltPhysics] Cannot create body - missing Transform or Collider component");
        return JPH::BodyID();
    }
    
    // Create shape from collider
    JPH::RefConst<JPH::Shape> shape = CreateShapeFromCollider(*collider);
    
    // Determine object layer
    JPH::ObjectLayer layer = Layers::DYNAMIC;
    if (!is_dynamic) {
        layer = Layers::STATIC;
    } else if ((static_cast<uint32_t>(collider->layer) & static_cast<uint32_t>(CollisionLayer::Player)) != 0) {
        layer = Layers::PLAYER;
    } else if ((static_cast<uint32_t>(collider->layer) & static_cast<uint32_t>(CollisionLayer::Enemy)) != 0) {
        layer = Layers::ENEMY;
    }
    
    // Create body settings
    JPH::BodyCreationSettings body_settings(
        shape,
        ToJoltR(transform->position),
        JPH::Quat::sIdentity(),  // TODO: Support rotation
        is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        layer
    );
    
    // Set physics properties
    if (is_dynamic) {
        body_settings.mFriction = 0.5f;
        body_settings.mRestitution = 0.3f;
        body_settings.mLinearDamping = 0.05f;      // Slight damping for weighty feel
        body_settings.mAngularDamping = 0.05f;
        body_settings.mGravityFactor = 1.0f;
    }
    
    // Create the body
    JPH::BodyInterface& body_interface = m_physics_system->GetBodyInterface();
    JPH::BodyID body_id = body_interface.CreateAndAddBody(body_settings, 
        is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    
    if (body_id.IsInvalid()) {
        LOG_ERROR("[JoltPhysics] Failed to create body for entity {}", entity);
        return body_id;
    }
    
    // Store mapping
    m_entity_to_body[entity] = body_id;
    m_body_to_entity[body_id.GetIndex()] = entity;
    
    // Store entity as user data
    body_interface.SetUserData(body_id, static_cast<JPH::uint64>(entity));
    
    LOG_INFO("[JoltPhysics] Created {} body for entity {}", 
             is_dynamic ? "dynamic" : "static", entity);
    
    return body_id;
}

JPH::BodyID JoltPhysics::CreateBodyWithSettings(Entity entity, const JPH::BodyCreationSettings& settings) {
    if (!m_physics_system) return JPH::BodyID();
    
    JPH::BodyInterface& body_interface = m_physics_system->GetBodyInterface();
    JPH::BodyID body_id = body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    
    if (!body_id.IsInvalid()) {
        m_entity_to_body[entity] = body_id;
        m_body_to_entity[body_id.GetIndex()] = entity;
        body_interface.SetUserData(body_id, static_cast<JPH::uint64>(entity));
    }
    
    return body_id;
}

void JoltPhysics::DestroyBody(Entity entity) {
    auto it = m_entity_to_body.find(entity);
    if (it == m_entity_to_body.end()) return;
    
    JPH::BodyID body_id = it->second;
    if (!body_id.IsInvalid()) {
        JPH::BodyInterface& body_interface = m_physics_system->GetBodyInterface();
        body_interface.RemoveBody(body_id);
        body_interface.DestroyBody(body_id);
        
        m_body_to_entity.erase(body_id.GetIndex());
    }
    
    m_entity_to_body.erase(it);
}

JPH::BodyID JoltPhysics::GetBodyID(Entity entity) const {
    auto it = m_entity_to_body.find(entity);
    return (it != m_entity_to_body.end()) ? it->second : JPH::BodyID();
}

Entity JoltPhysics::GetEntity(JPH::BodyID body_id) const {
    auto it = m_body_to_entity.find(body_id.GetIndex());
    return (it != m_body_to_entity.end()) ? it->second : INVALID_ENTITY;
}

// ===== Body Properties =====

void JoltPhysics::SetPosition(Entity entity, const vec3& position) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetPosition(body_id, ToJoltR(position), JPH::EActivation::Activate);
}

void JoltPhysics::SetRotation(Entity entity, const vec3& euler_degrees) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    
    // Convert euler to quaternion
    JPH::Quat rot = JPH::Quat::sEulerAngles(JPH::Vec3(
        euler_degrees.x * DEG_TO_RAD,
        euler_degrees.y * DEG_TO_RAD,
        euler_degrees.z * DEG_TO_RAD
    ));
    m_physics_system->GetBodyInterface().SetRotation(body_id, rot, JPH::EActivation::Activate);
}

void JoltPhysics::SetVelocity(Entity entity, const vec3& velocity) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetLinearVelocity(body_id, ToJolt(velocity));
}

void JoltPhysics::SetAngularVelocity(Entity entity, const vec3& angular_velocity) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetAngularVelocity(body_id, ToJolt(angular_velocity));
}

vec3 JoltPhysics::GetPosition(Entity entity) const {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return {0, 0, 0};
    return FromJolt(m_physics_system->GetBodyInterface().GetCenterOfMassPosition(body_id));
}

vec3 JoltPhysics::GetVelocity(Entity entity) const {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return {0, 0, 0};
    return FromJolt(m_physics_system->GetBodyInterface().GetLinearVelocity(body_id));
}

vec3 JoltPhysics::GetAngularVelocity(Entity entity) const {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return {0, 0, 0};
    return FromJolt(m_physics_system->GetBodyInterface().GetAngularVelocity(body_id));
}

void JoltPhysics::AddForce(Entity entity, const vec3& force) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().AddForce(body_id, ToJolt(force));
}

void JoltPhysics::AddImpulse(Entity entity, const vec3& impulse) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().AddImpulse(body_id, ToJolt(impulse));
}

void JoltPhysics::AddTorque(Entity entity, const vec3& torque) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().AddTorque(body_id, ToJolt(torque));
}

void JoltPhysics::SetMotionType(Entity entity, JPH::EMotionType type) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetMotionType(body_id, type, JPH::EActivation::Activate);
}

void JoltPhysics::SetFriction(Entity entity, float friction) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetFriction(body_id, friction);
}

void JoltPhysics::SetRestitution(Entity entity, float restitution) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetRestitution(body_id, restitution);
}

void JoltPhysics::SetGravityFactor(Entity entity, float factor) {
    JPH::BodyID body_id = GetBodyID(entity);
    if (body_id.IsInvalid()) return;
    m_physics_system->GetBodyInterface().SetGravityFactor(body_id, factor);
}

// ===== Queries =====

JoltRaycastHit JoltPhysics::Raycast(const vec3& origin, const vec3& direction, 
                                     float max_distance, JPH::ObjectLayer layer_filter) {
    JoltRaycastHit result;
    if (!m_physics_system) return result;
    
    JPH::RRayCast ray(ToJoltR(origin), ToJolt(direction.normalized() * max_distance));
    JPH::RayCastResult hit;
    
    // Use broadphase for fast query
    if (m_physics_system->GetNarrowPhaseQuery().CastRay(ray, hit)) {
        result.hit = true;
        result.distance = hit.mFraction * max_distance;
        result.point = origin + direction.normalized() * result.distance;
        result.body_id = hit.mBodyID;
        result.entity = GetEntity(hit.mBodyID);
        
        // Get normal at hit point
        JPH::BodyLockRead lock(m_physics_system->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            result.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJoltR(result.point)));
        }
    }
    
    return result;
}

std::vector<JoltRaycastHit> JoltPhysics::RaycastAll(const vec3& origin, const vec3& direction,
                                                     float max_distance) {
    std::vector<JoltRaycastHit> results;
    if (!m_physics_system) return results;
    
    JPH::RayCast ray(ToJolt(origin), ToJolt(direction.normalized() * max_distance));
    
    // Collector for all hits
    JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> collector;
    m_physics_system->GetBroadPhaseQuery().CastRay(ray, collector);
    
    for (const auto& hit : collector.mHits) {
        JoltRaycastHit result;
        result.hit = true;
        result.distance = hit.mFraction * max_distance;
        result.point = origin + direction.normalized() * result.distance;
        result.body_id = hit.mBodyID;
        result.entity = GetEntity(hit.mBodyID);
        results.push_back(result);
    }
    
    // Sort by distance
    std::sort(results.begin(), results.end(), 
              [](const JoltRaycastHit& a, const JoltRaycastHit& b) {
                  return a.distance < b.distance;
              });
    
    return results;
}

std::vector<Entity> JoltPhysics::OverlapSphere(const vec3& center, float radius,
                                                JPH::ObjectLayer layer_filter) {
    std::vector<Entity> results;
    if (!m_physics_system) return results;
    
    // Create sphere shape for query
    JPH::SphereShape sphere(radius);
    
    // Collector for overlapping bodies
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    
    m_physics_system->GetBroadPhaseQuery().CollideAABox(
        JPH::AABox(ToJolt(center - vec3{radius, radius, radius}), 
                   ToJolt(center + vec3{radius, radius, radius})),
        collector
    );
    
    for (const auto& body_id : collector.mHits) {
        Entity entity = GetEntity(body_id);
        if (entity != INVALID_ENTITY) {
            results.push_back(entity);
        }
    }
    
    return results;
}

std::vector<Entity> JoltPhysics::OverlapBox(const vec3& center, const vec3& half_extents,
                                             JPH::ObjectLayer layer_filter) {
    std::vector<Entity> results;
    if (!m_physics_system) return results;
    
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    
    m_physics_system->GetBroadPhaseQuery().CollideAABox(
        JPH::AABox(ToJolt(center - half_extents), ToJolt(center + half_extents)),
        collector
    );
    
    for (const auto& body_id : collector.mHits) {
        Entity entity = GetEntity(body_id);
        if (entity != INVALID_ENTITY) {
            results.push_back(entity);
        }
    }
    
    return results;
}

// ===== Settings =====

void JoltPhysics::SetGravity(const vec3& gravity) {
    if (m_physics_system) {
        m_physics_system->SetGravity(ToJolt(gravity));
    }
}

vec3 JoltPhysics::GetGravity() const {
    if (m_physics_system) {
        return FromJolt(m_physics_system->GetGravity());
    }
    return {0, -9.81f, 0};
}

JPH::BodyInterface& JoltPhysics::GetBodyInterface() {
    return m_physics_system->GetBodyInterface();
}

const JPH::BodyInterface& JoltPhysics::GetBodyInterface() const {
    return m_physics_system->GetBodyInterface();
}

// ===== Collision Callbacks =====

void JoltPhysics::OnContactAdded(const JoltContactInfo& info) {
    m_contacts_this_frame.push_back(info);
    if (m_contact_added_callback) {
        m_contact_added_callback(info);
    }
}

void JoltPhysics::OnContactPersisted(const JoltContactInfo& info) {
    if (m_contact_persisted_callback) {
        m_contact_persisted_callback(info);
    }
}

void JoltPhysics::OnContactRemoved(Entity a, Entity b) {
    if (m_contact_removed_callback) {
        JoltContactInfo info;
        info.entity_a = a;
        info.entity_b = b;
        m_contact_removed_callback(info);
    }
}

} // namespace action
