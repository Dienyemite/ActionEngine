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
    m_free_list.clear();
    m_generations.clear();
    m_next_index = 0;
    
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
    u32 index;
    u32 generation;

    if (!m_free_list.empty()) {
        // Recycle an index. The generation was already incremented in DestroyEntity.
        Entity recycled = m_free_list.back();
        m_free_list.pop_back();
        index      = EntityIndex(recycled);
        generation = EntityGeneration(recycled);   // already bumped
    } else {
        // Brand-new index slot
        index      = m_next_index++;
        generation = 0;
        m_generations.push_back(0);
    }

    Entity entity = MakeEntity(index, generation);
    return entity;
}

void ECS::DestroyEntity(Entity entity) {
    if (!IsAlive(entity)) return;

    // Remove all components from every pool
    for (auto& pool : m_pools) {
        if (pool) pool->Remove(entity);
    }

    // Increment the generation for this index, invalidating any surviving copies
    // of the old handle.  Wrap within ENTITY_GENERATION_BITS bits.
    u32 index = EntityIndex(entity);
    u32 next_gen = (EntityGeneration(entity) + 1) & ((1u << ENTITY_GENERATION_BITS) - 1);
    m_generations[index] = next_gen;

    // Push the NEXT-generation handle onto the free list so CreateEntity issues
    // the correct generation when this index is reused.
    m_free_list.push_back(MakeEntity(index, next_gen));
}

bool ECS::IsAlive(Entity entity) const {
    if (entity == INVALID_ENTITY) return false;
    u32 index = EntityIndex(entity);
    if (index >= m_generations.size()) return false;
    // Entity is live iff its generation matches the current generation for that index
    return EntityGeneration(entity) == m_generations[index];
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
