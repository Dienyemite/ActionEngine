#include "script_system.h"
#include "../gameplay/ecs/ecs.h"
#include "../core/logging.h"

namespace action {

void ScriptSystem::Initialize(ECS* ecs, Input* input, AssetManager* assets,
                               WorldManager* world, Renderer* renderer) {
    m_ecs = ecs;
    m_input = input;
    m_assets = assets;
    m_world = world;
    m_renderer = renderer;
    
    LOG_INFO("[ScriptSystem] Initialized");
}

void ScriptSystem::Shutdown() {
    // Call OnDestroy on all scripts
    m_ecs->ForEach<ScriptComponent>([](Entity entity, ScriptComponent& comp) {
        for (auto& script : comp.scripts) {
            if (script) {
                script->OnDestroy();
            }
        }
        comp.scripts.clear();
    });
    
    m_pending_start.clear();
    m_destroy_queue.clear();
    
    LOG_INFO("[ScriptSystem] Shutdown");
}

Script* ScriptSystem::AddScript(Entity entity, const std::string& type_name) {
    if (entity == INVALID_ENTITY) return nullptr;
    
    auto script = ScriptFactory::Instance().Create(type_name);
    if (!script) {
        LOG_WARN("[ScriptSystem] Unknown script type: {}", type_name);
        return nullptr;
    }
    
    // Ensure entity has ScriptComponent
    if (!m_ecs->HasComponent<ScriptComponent>(entity)) {
        m_ecs->EmplaceComponent<ScriptComponent>(entity);
    }
    
    auto* comp = m_ecs->GetComponent<ScriptComponent>(entity);
    Script* ptr = script.get();
    
    // Set context and call OnCreate
    script->SetContext(entity, m_ecs, m_input, m_assets, m_world, m_renderer);
    script->OnCreate();
    
    comp->scripts.push_back(std::move(script));
    m_pending_start.push_back(ptr);
    
    return ptr;
}

void ScriptSystem::RemoveScript(Entity entity, Script* script) {
    if (!m_ecs->HasComponent<ScriptComponent>(entity)) return;
    
    auto* comp = m_ecs->GetComponent<ScriptComponent>(entity);
    auto it = std::find_if(comp->scripts.begin(), comp->scripts.end(),
        [script](const std::unique_ptr<Script>& s) { return s.get() == script; });
    
    if (it != comp->scripts.end()) {
        (*it)->OnDestroy();
        comp->scripts.erase(it);
    }
    
    // Remove from pending start if present
    m_pending_start.erase(
        std::remove(m_pending_start.begin(), m_pending_start.end(), script),
        m_pending_start.end());
}

void ScriptSystem::RemoveAllScripts(Entity entity) {
    if (!m_ecs->HasComponent<ScriptComponent>(entity)) return;
    
    auto* comp = m_ecs->GetComponent<ScriptComponent>(entity);
    for (auto& script : comp->scripts) {
        script->OnDestroy();
        
        // Remove from pending start
        m_pending_start.erase(
            std::remove(m_pending_start.begin(), m_pending_start.end(), script.get()),
            m_pending_start.end());
    }
    comp->scripts.clear();
}

Script* ScriptSystem::GetScript(Entity entity, const std::string& type_name) {
    if (!m_ecs->HasComponent<ScriptComponent>(entity)) return nullptr;
    auto* comp = m_ecs->GetComponent<ScriptComponent>(entity);
    return comp->GetScript(type_name);
}

void ScriptSystem::Update(float dt) {
    StartPendingScripts();
    
    m_ecs->ForEach<ScriptComponent>([dt](Entity entity, ScriptComponent& comp) {
        for (auto& script : comp.scripts) {
            if (script && script->IsEnabled()) {
                script->OnUpdate(dt);
            }
        }
    });
    
    ProcessDestroyQueue();
}

void ScriptSystem::FixedUpdate(float fixed_dt) {
    m_ecs->ForEach<ScriptComponent>([fixed_dt](Entity entity, ScriptComponent& comp) {
        for (auto& script : comp.scripts) {
            if (script && script->IsEnabled()) {
                script->OnFixedUpdate(fixed_dt);
            }
        }
    });
}

void ScriptSystem::LateUpdate(float dt) {
    m_ecs->ForEach<ScriptComponent>([dt](Entity entity, ScriptComponent& comp) {
        for (auto& script : comp.scripts) {
            if (script && script->IsEnabled()) {
                script->OnLateUpdate(dt);
            }
        }
    });
}

const std::vector<std::string>& ScriptSystem::GetRegisteredScriptTypes() const {
    return ScriptFactory::Instance().GetRegisteredTypes();
}

void ScriptSystem::StartPendingScripts() {
    // Copy to avoid issues if OnStart adds more scripts
    std::vector<Script*> to_start = std::move(m_pending_start);
    m_pending_start.clear();
    
    for (Script* script : to_start) {
        if (script && script->IsEnabled()) {
            script->OnStart();
        }
    }
}

void ScriptSystem::ProcessDestroyQueue() {
    // Process entities queued for delayed destruction
    std::vector<std::pair<Entity, float>> remaining;
    
    for (auto& [entity, delay] : m_destroy_queue) {
        delay -= 1.0f / 60.0f; // Approximate frame time
        if (delay <= 0.0f) {
            // Destroy entity and all its scripts
            RemoveAllScripts(entity);
            m_ecs->DestroyEntity(entity);
        } else {
            remaining.push_back({entity, delay});
        }
    }
    
    m_destroy_queue = std::move(remaining);
}

} // namespace action
