#include "ecs.h"
#include "core/logging.h"
#include "core/profiler.h"

namespace action {

bool ECS::Initialize() {
    LOG_INFO("ECS initialized");
    return true;
}

void ECS::Shutdown() {
    m_systems.clear();
    m_pools.clear();
    m_entities.clear();
    m_free_list.clear();
    m_alive.clear();
    
    LOG_INFO("ECS shutdown");
}

void ECS::Update(float dt) {
    PROFILE_SCOPE("ECS::Update");
    
    for (auto& system : m_systems) {
        if (system->IsEnabled()) {
            system->Update(dt);
        }
    }
}

Entity ECS::CreateEntity() {
    Entity entity;
    
    if (!m_free_list.empty()) {
        entity = m_free_list.back();
        m_free_list.pop_back();
        m_alive[entity] = true;
    } else {
        entity = m_next_entity++;
        m_entities.push_back(entity);
        m_alive.push_back(true);
    }
    
    return entity;
}

void ECS::DestroyEntity(Entity entity) {
    if (entity >= m_alive.size() || !m_alive[entity]) {
        return;
    }
    
    // Remove all components
    for (auto& pool : m_pools) {
        if (pool) {
            pool->Remove(entity);
        }
    }
    
    m_alive[entity] = false;
    m_free_list.push_back(entity);
}

bool ECS::IsAlive(Entity entity) const {
    if (entity >= m_alive.size()) return false;
    return m_alive[entity];
}

vec3 ECS::GetPlayerPosition() const {
    if (m_player_entity == INVALID_ENTITY) {
        return {0, 0, 0};
    }
    
    if (auto* transform = GetComponent<TransformComponent>(m_player_entity)) {
        return transform->position;
    }
    
    return {0, 0, 0};
}

vec3 ECS::GetPlayerVelocity() const {
    if (m_player_entity == INVALID_ENTITY) {
        return {0, 0, 0};
    }
    
    if (auto* velocity = GetComponent<VelocityComponent>(m_player_entity)) {
        return velocity->linear;
    }
    
    return {0, 0, 0};
}

} // namespace action
