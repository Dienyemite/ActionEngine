#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include <string>

namespace action {

// Forward declarations
class Engine;
class Input;
class AssetManager;
class WorldManager;
class Renderer;
class ScriptSystem;
class PhysicsWorld;
struct RaycastHit;

/*
 * Script - Base class for all game scripts
 * 
 * Scripts define game behavior by inheriting from this class and
 * overriding lifecycle methods. Scripts are attached to entities
 * and receive callbacks during the game loop.
 * 
 * Example usage:
 * 
 *   class PlayerController : public Script {
 *   public:
 *       SCRIPT_CLASS(PlayerController)
 *       
 *       void OnStart() override {
 *           // Called once when script starts
 *           m_health = 100.0f;
 *       }
 *       
 *       void OnUpdate(float dt) override {
 *           // Called every frame
 *           if (GetInput()->IsKeyPressed(Key::Space)) {
 *               Jump();
 *           }
 *       }
 *       
 *   private:
 *       float m_health = 100.0f;
 *   };
 *   
 *   // Register in game code:
 *   REGISTER_SCRIPT(PlayerController)
 */
class Script {
public:
    Script() = default;
    virtual ~Script() = default;
    
    // Lifecycle callbacks - override these in your scripts
    virtual void OnCreate() {}                      // Called when script is created
    virtual void OnStart() {}                       // Called before first Update
    virtual void OnUpdate(float dt) {}              // Called every frame
    virtual void OnFixedUpdate(float fixed_dt) {}   // Called at fixed timestep (physics)
    virtual void OnLateUpdate(float dt) {}          // Called after all Updates
    virtual void OnDestroy() {}                     // Called when script/entity is destroyed
    
    // Collision callbacks (requires physics component)
    virtual void OnCollisionEnter(Entity other) {}
    virtual void OnCollisionStay(Entity other) {}
    virtual void OnCollisionExit(Entity other) {}
    
    // Trigger callbacks
    virtual void OnTriggerEnter(Entity other) {}
    virtual void OnTriggerExit(Entity other) {}
    
    // Script info
    virtual const char* GetTypeName() const = 0;
    
    // Entity this script is attached to
    Entity GetEntity() const { return m_entity; }
    
    // Check if script is enabled
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    // Transform helpers (shortcuts for common operations)
    vec3 GetPosition() const;
    void SetPosition(const vec3& pos);
    quat GetRotation() const;
    void SetRotation(const quat& rot);
    vec3 GetScale() const;
    void SetScale(const vec3& scale);
    
    // Forward direction
    vec3 GetForward() const;
    vec3 GetRight() const;
    vec3 GetUp() const;
    
    // Move/rotate helpers
    void Translate(const vec3& delta);
    void Rotate(const vec3& euler_degrees);
    void LookAt(const vec3& target);
    
    // Component access
    template<typename T>
    T* GetComponent() {
        if (m_ecs && m_entity != INVALID_ENTITY) {
            return m_ecs->GetComponent<T>(m_entity);
        }
        return nullptr;
    }
    
    template<typename T>
    bool HasComponent() {
        if (m_ecs && m_entity != INVALID_ENTITY) {
            return m_ecs->HasComponent<T>(m_entity);
        }
        return false;
    }
    
    template<typename T>
    T& AddComponent() {
        return m_ecs->AddComponent<T>(m_entity);
    }
    
    // Find other entities/scripts
    Entity FindEntityByName(const std::string& name);
    template<typename T>
    T* FindScriptOfType();
    
    // Engine system access
    Input* GetInput() const { return m_input; }
    AssetManager* GetAssets() const { return m_assets; }
    WorldManager* GetWorld() const { return m_world; }
    Renderer* GetRenderer() const { return m_renderer; }
    ECS* GetECS() const { return m_ecs; }
    PhysicsWorld* GetPhysics() const { return m_physics; }
    
    // Physics helpers (shortcuts for common operations)
    RaycastHit Raycast(const vec3& origin, const vec3& direction, float max_distance = 100.0f);
    std::vector<Entity> OverlapSphere(const vec3& center, float radius);
    
    // Logging
    void Log(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    
    // Instantiate prefab
    Entity Instantiate(const std::string& prefab_name, const vec3& position = {0,0,0});
    
    // Destroy entity
    void Destroy(Entity entity, float delay = 0.0f);
    void DestroySelf(float delay = 0.0f);
    
protected:
    friend class ScriptSystem;
    
    // Set by ScriptSystem when attaching
    void SetContext(Entity entity, ECS* ecs, Input* input, 
                    AssetManager* assets, WorldManager* world, Renderer* renderer,
                    PhysicsWorld* physics) {
        m_entity = entity;
        m_ecs = ecs;
        m_input = input;
        m_assets = assets;
        m_world = world;
        m_renderer = renderer;
        m_physics = physics;
    }
    
    Entity m_entity = INVALID_ENTITY;
    ECS* m_ecs = nullptr;
    Input* m_input = nullptr;
    AssetManager* m_assets = nullptr;
    WorldManager* m_world = nullptr;
    Renderer* m_renderer = nullptr;
    PhysicsWorld* m_physics = nullptr;
    
    bool m_enabled = true;
    bool m_started = false;
};

// Macro to declare script type name
#define SCRIPT_CLASS(ClassName) \
    const char* GetTypeName() const override { return #ClassName; } \
    static const char* StaticTypeName() { return #ClassName; }

} // namespace action
