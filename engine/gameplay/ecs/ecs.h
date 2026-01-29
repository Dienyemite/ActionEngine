#pragma once

#include "core/types.h"
#include "core/containers/sparse_set.h"
#include <memory>
#include <functional>
#include <vector>
#include <atomic>

namespace action {

/*
 * Entity Component System (ECS)
 * 
 * Data-oriented design for cache efficiency on i5-6600K
 * 
 * Features:
 * - Sparse set based component storage
 * - Fast iteration over components
 * - Simple entity management
 * - System registration and execution
 */

// Compile-time type ID generator (replaces slow std::type_index hash)
class TypeIDGenerator {
public:
    template<typename T>
    static u32 GetID() {
        static const u32 id = s_next_id++;
        return id;
    }
    
    static u32 Count() { return s_next_id.load(); }
    
private:
    static inline std::atomic<u32> s_next_id{0};
};

// Entity is just an ID
using Entity = u32;
constexpr Entity INVALID_ENTITY = UINT32_MAX;

// Component storage base
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(Entity entity) = 0;
    virtual bool Has(Entity entity) const = 0;
};

// Typed component pool
template<typename T>
class ComponentPool : public IComponentPool {
public:
    T& Add(Entity entity, const T& component = T{}) {
        return m_data.Insert(entity, component);
    }
    
    template<typename... Args>
    T& Emplace(Entity entity, Args&&... args) {
        return m_data.Emplace(entity, std::forward<Args>(args)...);
    }
    
    void Remove(Entity entity) override {
        m_data.Remove(entity);
    }
    
    bool Has(Entity entity) const override {
        return m_data.Contains(entity);
    }
    
    T* Get(Entity entity) {
        return m_data.Get(entity);
    }
    
    const T* Get(Entity entity) const {
        return m_data.Get(entity);
    }
    
    // Iteration
    auto begin() { return m_data.GetComponents().begin(); }
    auto end() { return m_data.GetComponents().end(); }
    
    const std::vector<Entity>& GetEntities() const { return m_data.GetDense(); }
    std::span<T> GetComponents() { return m_data.GetComponents(); }
    u32 Size() const { return m_data.Size(); }
    
private:
    TypedSparseSet<T> m_data;
};

// System base class
class System {
public:
    virtual ~System() = default;
    virtual void Update(float dt) = 0;
    
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    
protected:
    bool m_enabled = true;
};

// ECS world
class ECS {
public:
    ECS() = default;
    ~ECS() = default;
    
    bool Initialize();
    void Shutdown();
    void Update(float dt);
    
    // Entity management
    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    bool IsAlive(Entity entity) const;
    
    // Component management
    template<typename T>
    T& AddComponent(Entity entity, const T& component = T{}) {
        return GetOrCreatePool<T>()->Add(entity, component);
    }
    
    template<typename T, typename... Args>
    T& EmplaceComponent(Entity entity, Args&&... args) {
        return GetOrCreatePool<T>()->Emplace(entity, std::forward<Args>(args)...);
    }
    
    template<typename T>
    void RemoveComponent(Entity entity) {
        if (auto* pool = GetPool<T>()) {
            pool->Remove(entity);
        }
    }
    
    template<typename T>
    T* GetComponent(Entity entity) {
        if (auto* pool = GetPool<T>()) {
            return pool->Get(entity);
        }
        return nullptr;
    }
    
    template<typename T>
    const T* GetComponent(Entity entity) const {
        if (auto* pool = GetPool<T>()) {
            return pool->Get(entity);
        }
        return nullptr;
    }
    
    template<typename T>
    bool HasComponent(Entity entity) const {
        if (auto* pool = GetPool<T>()) {
            return pool->Has(entity);
        }
        return false;
    }
    
    // Iteration over entities with specific components
    // Uses template functor instead of std::function to avoid heap allocation
    template<typename... Components, typename Func>
    void ForEach(Func&& func) {
        // Get the smallest pool for iteration (reduces iterations)
        auto* first_pool = GetPool<std::tuple_element_t<0, std::tuple<Components...>>>();
        if (!first_pool) return;
        
        for (Entity entity : first_pool->GetEntities()) {
            if ((HasComponent<Components>(entity) && ...)) {
                func(entity, *GetComponent<Components>(entity)...);
            }
        }
    }
    
    // System management
    template<typename T, typename... Args>
    T* AddSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        m_systems.push_back(std::move(system));
        return ptr;
    }
    
    // Player helpers (for world streaming)
    vec3 GetPlayerPosition() const;
    vec3 GetPlayerVelocity() const;
    void SetPlayerEntity(Entity entity) { m_player_entity = entity; }
    Entity GetPlayerEntity() const { return m_player_entity; }
    
private:
    template<typename T>
    ComponentPool<T>* GetPool() {
        const u32 id = TypeIDGenerator::GetID<T>();
        if (id < m_pools.size() && m_pools[id]) {
            return static_cast<ComponentPool<T>*>(m_pools[id].get());
        }
        return nullptr;
    }
    
    template<typename T>
    const ComponentPool<T>* GetPool() const {
        const u32 id = TypeIDGenerator::GetID<T>();
        if (id < m_pools.size() && m_pools[id]) {
            return static_cast<const ComponentPool<T>*>(m_pools[id].get());
        }
        return nullptr;
    }
    
    template<typename T>
    ComponentPool<T>* GetOrCreatePool() {
        const u32 id = TypeIDGenerator::GetID<T>();
        if (id >= m_pools.size()) {
            m_pools.resize(id + 1);
        }
        if (!m_pools[id]) {
            m_pools[id] = std::make_unique<ComponentPool<T>>();
        }
        return static_cast<ComponentPool<T>*>(m_pools[id].get());
    }
    
    // Entity storage
    std::vector<Entity> m_entities;
    std::vector<Entity> m_free_list;
    std::vector<u8> m_alive;  // u8 instead of bool for cache efficiency
    Entity m_next_entity = 0;
    
    // Component pools - vector indexed by compile-time type ID (faster than unordered_map)
    std::vector<std::unique_ptr<IComponentPool>> m_pools;
    
    // Systems
    std::vector<std::unique_ptr<System>> m_systems;
    
    // Special entities
    Entity m_player_entity = INVALID_ENTITY;
};

// Common components
struct TransformComponent {
    vec3 position{0, 0, 0};
    quat rotation = quat::identity();
    vec3 scale{1, 1, 1};
    
    mat4 GetMatrix() const {
        return mat4::translate(position) * mat4::rotate(rotation) * mat4::scale(scale);
    }
};

struct VelocityComponent {
    vec3 linear{0, 0, 0};
    vec3 angular{0, 0, 0};
};

struct RenderComponent {
    MeshHandle mesh;
    MaterialHandle material;
    u8 lod_level = 0;
    bool visible = true;
    bool cast_shadow = true;
};

struct BoundsComponent {
    AABB local_bounds;
    AABB world_bounds;
    float radius = 1.0f;
};

struct TagComponent {
    std::string name;
    u32 tags = 0;  // Bitfield for quick filtering
};

// Tag bits
namespace Tags {
    constexpr u32 Player = 1 << 0;
    constexpr u32 Enemy = 1 << 1;
    constexpr u32 Prop = 1 << 2;
    constexpr u32 Static = 1 << 3;
    constexpr u32 Dynamic = 1 << 4;
    constexpr u32 Trigger = 1 << 5;
}

} // namespace action
