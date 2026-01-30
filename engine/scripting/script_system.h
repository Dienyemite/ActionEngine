#pragma once

#include "script.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <typeindex>

namespace action {

// Forward declarations
class Engine;
class Input;
class AssetManager;
class WorldManager;
class Renderer;
class ECS;

/*
 * ScriptComponent - Holds scripts attached to an entity
 */
struct ScriptComponent {
    std::vector<std::unique_ptr<Script>> scripts;
    
    // Make move-only (unique_ptr is not copyable)
    ScriptComponent() = default;
    ScriptComponent(ScriptComponent&&) = default;
    ScriptComponent& operator=(ScriptComponent&&) = default;
    ScriptComponent(const ScriptComponent&) = delete;
    ScriptComponent& operator=(const ScriptComponent&) = delete;
    
    template<typename T>
    T* GetScript() {
        for (auto& script : scripts) {
            if (T* casted = dynamic_cast<T*>(script.get())) {
                return casted;
            }
        }
        return nullptr;
    }
    
    Script* GetScript(const std::string& type_name) {
        for (auto& script : scripts) {
            if (script->GetTypeName() == type_name) {
                return script.get();
            }
        }
        return nullptr;
    }
};

/*
 * ScriptFactory - Creates script instances by type name
 */
class ScriptFactory {
public:
    using CreateFunc = std::function<std::unique_ptr<Script>()>;
    
    static ScriptFactory& Instance() {
        static ScriptFactory instance;
        return instance;
    }
    
    // Register a script type
    template<typename T>
    void Register() {
        m_creators[T::StaticTypeName()] = []() {
            return std::make_unique<T>();
        };
        m_type_names.push_back(T::StaticTypeName());
    }
    
    // Create a script by type name
    std::unique_ptr<Script> Create(const std::string& type_name) {
        auto it = m_creators.find(type_name);
        if (it != m_creators.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    // Get all registered script type names
    const std::vector<std::string>& GetRegisteredTypes() const {
        return m_type_names;
    }
    
    bool IsRegistered(const std::string& type_name) const {
        return m_creators.find(type_name) != m_creators.end();
    }
    
private:
    ScriptFactory() = default;
    std::unordered_map<std::string, CreateFunc> m_creators;
    std::vector<std::string> m_type_names;
};

// Macro to register a script type (call in game initialization)
#define REGISTER_SCRIPT(ClassName) \
    action::ScriptFactory::Instance().Register<ClassName>()

/*
 * ScriptSystem - Manages script lifecycle and execution
 * 
 * Responsibilities:
 * - Attach/detach scripts to entities
 * - Call script lifecycle methods (OnStart, OnUpdate, etc.)
 * - Provide scripts with access to engine systems
 */
class ScriptSystem {
public:
    ScriptSystem() = default;
    ~ScriptSystem() = default;
    
    // Initialize with engine references
    void Initialize(ECS* ecs, Input* input, AssetManager* assets, 
                    WorldManager* world, Renderer* renderer);
    void Shutdown();
    
    // Attach a script to an entity
    template<typename T>
    T* AddScript(Entity entity) {
        if (entity == INVALID_ENTITY) return nullptr;
        
        // Ensure entity has ScriptComponent
        if (!m_ecs->HasComponent<ScriptComponent>(entity)) {
            m_ecs->EmplaceComponent<ScriptComponent>(entity);
        }
        
        auto* comp = m_ecs->GetComponent<ScriptComponent>(entity);
        auto script = std::make_unique<T>();
        T* ptr = script.get();
        
        // Set context
        script->SetContext(entity, m_ecs, m_input, m_assets, m_world, m_renderer);
        script->OnCreate();
        
        comp->scripts.push_back(std::move(script));
        m_pending_start.push_back(ptr);
        
        return ptr;
    }
    
    // Add script by type name (for editor/serialization)
    Script* AddScript(Entity entity, const std::string& type_name);
    
    // Remove a specific script from entity
    void RemoveScript(Entity entity, Script* script);
    
    // Remove all scripts from entity
    void RemoveAllScripts(Entity entity);
    
    // Get script from entity
    template<typename T>
    T* GetScript(Entity entity) {
        if (!m_ecs->HasComponent<ScriptComponent>(entity)) return nullptr;
        auto* comp = m_ecs->GetComponent<ScriptComponent>(entity);
        return comp->GetScript<T>();
    }
    
    // Get script by type name
    Script* GetScript(Entity entity, const std::string& type_name);
    
    // Update all scripts
    void Update(float dt);
    void FixedUpdate(float fixed_dt);
    void LateUpdate(float dt);
    
    // Get registered script types (for editor dropdown)
    const std::vector<std::string>& GetRegisteredScriptTypes() const;
    
private:
    void StartPendingScripts();
    void ProcessDestroyQueue();
    
    ECS* m_ecs = nullptr;
    Input* m_input = nullptr;
    AssetManager* m_assets = nullptr;
    WorldManager* m_world = nullptr;
    Renderer* m_renderer = nullptr;
    
    // Scripts that need OnStart called
    std::vector<Script*> m_pending_start;
    
    // Entities queued for destruction
    std::vector<std::pair<Entity, float>> m_destroy_queue;
};

} // namespace action
