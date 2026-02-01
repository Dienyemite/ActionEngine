#pragma once

/*
 * Jolt Physics Layer Definitions
 * 
 * Object layers define collision filtering. Bodies on the same layer
 * can be configured to collide or not.
 * 
 * Broadphase layers are a coarser grouping for spatial partitioning.
 */

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace action {

// Object layers - fine-grained collision control
namespace Layers {
    static constexpr JPH::ObjectLayer STATIC      = 0;  // Static world geometry
    static constexpr JPH::ObjectLayer DYNAMIC     = 1;  // Dynamic physics objects
    static constexpr JPH::ObjectLayer PLAYER      = 2;  // Player character
    static constexpr JPH::ObjectLayer ENEMY       = 3;  // Enemy characters
    static constexpr JPH::ObjectLayer PROJECTILE  = 4;  // Bullets, arrows, etc.
    static constexpr JPH::ObjectLayer TRIGGER     = 5;  // Trigger volumes (no physics response)
    static constexpr JPH::ObjectLayer DEBRIS      = 6;  // Physics debris (simplified collision)
    static constexpr JPH::ObjectLayer NUM_LAYERS  = 7;
}

// Broadphase layer values (use inline functions to create BroadPhaseLayer objects)
namespace BroadPhaseLayers {
    static constexpr JPH::uint8 NON_MOVING_VALUE = 0;  // Static geometry
    static constexpr JPH::uint8 MOVING_VALUE     = 1;  // All moving objects
    static constexpr JPH::uint NUM_LAYERS        = 2;
    
    inline JPH::BroadPhaseLayer NON_MOVING() { return JPH::BroadPhaseLayer(NON_MOVING_VALUE); }
    inline JPH::BroadPhaseLayer MOVING()     { return JPH::BroadPhaseLayer(MOVING_VALUE); }
}

/*
 * ObjectLayerPairFilter - Defines which object layers can collide
 */
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::STATIC:
                // Static collides with everything except other statics and triggers
                return inObject2 != Layers::STATIC && inObject2 != Layers::TRIGGER;
                
            case Layers::DYNAMIC:
                // Dynamic collides with everything except triggers
                return inObject2 != Layers::TRIGGER;
                
            case Layers::PLAYER:
                // Player collides with static, dynamic, enemy, projectile
                return inObject2 == Layers::STATIC || 
                       inObject2 == Layers::DYNAMIC ||
                       inObject2 == Layers::ENEMY ||
                       inObject2 == Layers::PROJECTILE ||
                       inObject2 == Layers::DEBRIS;
                       
            case Layers::ENEMY:
                // Enemy collides with static, dynamic, player, other enemies
                return inObject2 == Layers::STATIC ||
                       inObject2 == Layers::DYNAMIC ||
                       inObject2 == Layers::PLAYER ||
                       inObject2 == Layers::ENEMY ||
                       inObject2 == Layers::PROJECTILE ||
                       inObject2 == Layers::DEBRIS;
                       
            case Layers::PROJECTILE:
                // Projectile collides with static, dynamic, player, enemy
                return inObject2 == Layers::STATIC ||
                       inObject2 == Layers::DYNAMIC ||
                       inObject2 == Layers::PLAYER ||
                       inObject2 == Layers::ENEMY;
                       
            case Layers::TRIGGER:
                // Triggers don't collide with anything (queries only)
                return false;
                
            case Layers::DEBRIS:
                // Debris collides with static and dynamic only (performance)
                return inObject2 == Layers::STATIC ||
                       inObject2 == Layers::DYNAMIC ||
                       inObject2 == Layers::PLAYER ||
                       inObject2 == Layers::ENEMY;
                       
            default:
                return true;
        }
    }
};

/*
 * BroadPhaseLayerInterface - Maps object layers to broadphase layers
 */
class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        // Map object layers to broadphase layers
        m_object_to_broadphase[Layers::STATIC]     = BroadPhaseLayers::NON_MOVING();
        m_object_to_broadphase[Layers::DYNAMIC]    = BroadPhaseLayers::MOVING();
        m_object_to_broadphase[Layers::PLAYER]     = BroadPhaseLayers::MOVING();
        m_object_to_broadphase[Layers::ENEMY]      = BroadPhaseLayers::MOVING();
        m_object_to_broadphase[Layers::PROJECTILE] = BroadPhaseLayers::MOVING();
        m_object_to_broadphase[Layers::TRIGGER]    = BroadPhaseLayers::NON_MOVING();
        m_object_to_broadphase[Layers::DEBRIS]     = BroadPhaseLayers::MOVING();
    }
    
    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }
    
    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return m_object_to_broadphase[inLayer];
    }
    
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch (static_cast<JPH::uint8>(inLayer)) {
            case BroadPhaseLayers::NON_MOVING_VALUE: return "NON_MOVING";
            case BroadPhaseLayers::MOVING_VALUE:     return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif
    
private:
    JPH::BroadPhaseLayer m_object_to_broadphase[Layers::NUM_LAYERS];
};

/*
 * ObjectVsBroadPhaseLayerFilter - Defines which broadphase layers to check
 */
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::STATIC:
                // Static only collides with moving objects
                return inLayer2 == BroadPhaseLayers::MOVING();
                
            case Layers::DYNAMIC:
            case Layers::PLAYER:
            case Layers::ENEMY:
            case Layers::PROJECTILE:
            case Layers::DEBRIS:
                // Moving objects collide with everything
                return true;
                
            case Layers::TRIGGER:
                // Triggers don't use broadphase collision
                return false;
                
            default:
                return true;
        }
    }
};

} // namespace action
