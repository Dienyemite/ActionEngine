#pragma once

#include "core/types.h"
#include <vector>

namespace action {

/*
 * LOD (Level of Detail) System
 * 
 * Aggressive LOD for GTX 660 optimization:
 * - 5 LOD levels per mesh
 * - Distance-based + screen-size based selection
 * - Smooth transitions (optional dithering)
 * - LOD bias for quality scaling
 */

// LOD level configuration
struct LODLevel {
    float max_distance;        // Maximum distance for this LOD
    float screen_size;         // Minimum screen-space size
    float triangle_reduction;  // Percentage of original triangles
    u32 mesh_index;            // Index into mesh LOD array
};

// Default LOD distances (meters) for medium-quality assets
// Optimized for GTX 660 + 1080p
struct LODConfig {
    // Per-category distances
    struct {
        float lod0 = 15.0f;    // Full detail
        float lod1 = 40.0f;    // 50% triangles
        float lod2 = 100.0f;   // 25% triangles
        float lod3 = 300.0f;   // 10% triangles
        float lod4 = 600.0f;   // 5% or billboard
    } character;
    
    struct {
        float lod0 = 20.0f;
        float lod1 = 50.0f;
        float lod2 = 150.0f;
        float lod3 = 400.0f;
        float lod4 = 800.0f;
    } prop;
    
    struct {
        float lod0 = 50.0f;
        float lod1 = 100.0f;
        float lod2 = 250.0f;
        float lod3 = 500.0f;
        float lod4 = 1000.0f;
    } environment;
    
    float lod_bias = 1.0f;     // Global LOD distance multiplier
    float hysteresis = 0.1f;   // Prevents LOD popping (10% buffer)
};

// LOD chain for a single mesh asset
struct LODChain {
    static constexpr u32 MAX_LODS = 5;
    
    MeshHandle lods[MAX_LODS];
    u32 triangle_counts[MAX_LODS];
    float distances[MAX_LODS];  // Transition distances
    u32 lod_count = 0;
    
    // Add LOD level
    void AddLOD(MeshHandle mesh, u32 triangles, float distance) {
        if (lod_count < MAX_LODS) {
            lods[lod_count] = mesh;
            triangle_counts[lod_count] = triangles;
            distances[lod_count] = distance;
            lod_count++;
        }
    }
};

class LODSystem {
public:
    LODSystem() = default;
    
    void SetConfig(const LODConfig& config) { m_config = config; }
    const LODConfig& GetConfig() const { return m_config; }
    
    // Calculate LOD level for an object
    // Returns: LOD level (0 = highest detail, 4 = lowest)
    u32 CalculateLOD(const vec3& object_pos, 
                     const vec3& camera_pos,
                     const LODChain& lod_chain,
                     float object_radius) const;
    
    // Calculate LOD with velocity prediction (for fast movement)
    u32 CalculateLODPredictive(const vec3& object_pos,
                                const vec3& camera_pos,
                                const vec3& camera_velocity,
                                const LODChain& lod_chain,
                                float object_radius,
                                float prediction_time) const;
    
    // Batch LOD calculation (SIMD optimized)
    void CalculateLODBatch(const vec3* object_positions,
                           const LODChain* lod_chains,
                           const float* object_radii,
                           u32 count,
                           const vec3& camera_pos,
                           u32* out_lod_levels) const;
    
    // Check if object should be culled (beyond max LOD distance)
    bool ShouldCull(const vec3& object_pos,
                    const vec3& camera_pos,
                    float max_distance) const;
    
private:
    LODConfig m_config;
    
    // Previous frame LOD levels for hysteresis
    mutable std::vector<u32> m_prev_lods;
};

// LOD triangle budgets (per frame, for GTX 660)
struct LODBudgets {
    static constexpr u32 TOTAL_TRIANGLES = 800000;
    
    // Budget allocation
    static constexpr u32 TERRAIN = 400000;     // 50%
    static constexpr u32 CHARACTERS = 100000;  // 12.5%
    static constexpr u32 PROPS = 200000;       // 25%
    static constexpr u32 PARTICLES = 50000;    // 6.25%
    static constexpr u32 RESERVE = 50000;      // 6.25%
    
    // Per-object limits (at LOD0)
    static constexpr u32 HERO_CHARACTER = 15000;
    static constexpr u32 NPC = 8000;
    static constexpr u32 PROP_LARGE = 5000;
    static constexpr u32 PROP_MEDIUM = 2000;
    static constexpr u32 PROP_SMALL = 500;
};

} // namespace action
