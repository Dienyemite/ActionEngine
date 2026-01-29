#pragma once

#include "../types.h"
#include <vector>

namespace action {

/*
 * Sparse Set - Core data structure for ECS
 * 
 * Properties:
 * - O(1) insert, remove, contains
 * - Dense iteration (cache-friendly)
 * - No holes in dense array
 * 
 * Used for:
 * - Entity storage
 * - Component storage per type
 */

class SparseSet {
public:
    static constexpr u32 INVALID = UINT32_MAX;
    static constexpr size_t PAGE_SIZE = 4096;  // 4K entries per page
    
    SparseSet() = default;
    ~SparseSet() = default;
    
    // Add entity to set, returns dense index
    u32 Insert(u32 entity) {
        EnsureCapacity(entity);
        
        u32 dense_index = static_cast<u32>(m_dense.size());
        m_dense.push_back(entity);
        SetSparse(entity, dense_index);
        
        return dense_index;
    }
    
    // Remove entity from set
    void Remove(u32 entity) {
        if (!Contains(entity)) return;
        
        u32 dense_index = GetSparse(entity);
        u32 last_entity = m_dense.back();
        
        // Swap with last element
        m_dense[dense_index] = last_entity;
        SetSparse(last_entity, dense_index);
        
        // Remove last element
        m_dense.pop_back();
        SetSparse(entity, INVALID);
    }
    
    // Check if entity is in set
    bool Contains(u32 entity) const {
        if (entity >= m_sparse_pages.size() * PAGE_SIZE) return false;
        
        u32 page = entity / PAGE_SIZE;
        if (page >= m_sparse_pages.size() || !m_sparse_pages[page]) return false;
        
        u32 index = entity % PAGE_SIZE;
        u32 dense_index = m_sparse_pages[page][index];
        
        return dense_index != INVALID && 
               dense_index < m_dense.size() && 
               m_dense[dense_index] == entity;
    }
    
    // Get dense index for entity (returns INVALID if not found)
    // More efficient than Contains() + GetDenseIndex() separately
    u32 GetDenseIndex(u32 entity) const {
        if (entity >= m_sparse_pages.size() * PAGE_SIZE) return INVALID;
        
        u32 page = entity / PAGE_SIZE;
        if (page >= m_sparse_pages.size() || !m_sparse_pages[page]) return INVALID;
        
        u32 index = entity % PAGE_SIZE;
        u32 dense_index = m_sparse_pages[page][index];
        
        // Validate the mapping (sparse -> dense -> sparse must be consistent)
        if (dense_index != INVALID && 
            dense_index < m_dense.size() && 
            m_dense[dense_index] == entity) {
            return dense_index;
        }
        return INVALID;
    }
    
    // Direct access to dense array
    const std::vector<u32>& GetDense() const { return m_dense; }
    u32 Size() const { return static_cast<u32>(m_dense.size()); }
    bool Empty() const { return m_dense.empty(); }
    
    // Iteration
    auto begin() { return m_dense.begin(); }
    auto end() { return m_dense.end(); }
    auto begin() const { return m_dense.begin(); }
    auto end() const { return m_dense.end(); }
    
    void Clear() {
        m_dense.clear();
        for (auto& page : m_sparse_pages) {
            if (page) {
                std::fill(page.get(), page.get() + PAGE_SIZE, INVALID);
            }
        }
    }
    
private:
    void EnsureCapacity(u32 entity) {
        u32 page = entity / PAGE_SIZE;
        
        if (page >= m_sparse_pages.size()) {
            m_sparse_pages.resize(page + 1);
        }
        
        if (!m_sparse_pages[page]) {
            m_sparse_pages[page] = std::make_unique<u32[]>(PAGE_SIZE);
            std::fill(m_sparse_pages[page].get(), 
                      m_sparse_pages[page].get() + PAGE_SIZE, INVALID);
        }
    }
    
    u32 GetSparse(u32 entity) const {
        u32 page = entity / PAGE_SIZE;
        u32 index = entity % PAGE_SIZE;
        return m_sparse_pages[page][index];
    }
    
    void SetSparse(u32 entity, u32 value) {
        u32 page = entity / PAGE_SIZE;
        u32 index = entity % PAGE_SIZE;
        m_sparse_pages[page][index] = value;
    }
    
    std::vector<u32> m_dense;
    std::vector<std::unique_ptr<u32[]>> m_sparse_pages;
};

/*
 * Typed Sparse Set - Stores components alongside entities
 * 
 * Components are stored in dense array parallel to entity array
 * Enables cache-friendly iteration over components
 */
template<typename T>
class TypedSparseSet : public SparseSet {
public:
    // Insert entity with component
    T& Insert(u32 entity, const T& component) {
        u32 dense_index = SparseSet::Insert(entity);
        
        if (dense_index >= m_components.size()) {
            m_components.resize(dense_index + 1);
        }
        
        m_components[dense_index] = component;
        return m_components[dense_index];
    }
    
    // Insert entity with in-place construction
    template<typename... Args>
    T& Emplace(u32 entity, Args&&... args) {
        u32 dense_index = SparseSet::Insert(entity);
        
        if (dense_index >= m_components.size()) {
            m_components.resize(dense_index + 1);
        }
        
        m_components[dense_index] = T(std::forward<Args>(args)...);
        return m_components[dense_index];
    }
    
    // Remove entity and component
    void Remove(u32 entity) {
        if (!Contains(entity)) return;
        
        u32 dense_index = GetDenseIndex(entity);
        
        // Swap component with last
        if (dense_index < m_components.size() - 1) {
            m_components[dense_index] = std::move(m_components.back());
        }
        m_components.pop_back();
        
        SparseSet::Remove(entity);
    }
    
    // Get component for entity
    T* Get(u32 entity) {
        if (!Contains(entity)) return nullptr;
        return &m_components[GetDenseIndex(entity)];
    }
    
    const T* Get(u32 entity) const {
        if (!Contains(entity)) return nullptr;
        return &m_components[GetDenseIndex(entity)];
    }
    
    // Direct access to component array (for iteration)
    std::span<T> GetComponents() { return m_components; }
    std::span<const T> GetComponents() const { return m_components; }
    
    void Clear() {
        SparseSet::Clear();
        m_components.clear();
    }
    
private:
    std::vector<T> m_components;
};

} // namespace action
