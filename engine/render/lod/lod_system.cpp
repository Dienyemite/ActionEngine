#include "lod_system.h"
#include "core/math/math.h"

namespace action {

u32 LODSystem::CalculateLOD(const vec3& object_pos,
                             const vec3& camera_pos,
                             const LODChain& lod_chain,
                             float object_radius) const {
    if (lod_chain.lod_count == 0) return 0;
    
    float dist_sq = distance_sq(object_pos, camera_pos);
    float dist = std::sqrt(dist_sq);
    
    // Apply LOD bias
    dist /= m_config.lod_bias;
    
    // Find appropriate LOD level
    for (u32 i = 0; i < lod_chain.lod_count; ++i) {
        // Apply hysteresis to prevent popping
        float threshold = lod_chain.distances[i];
        if (i > 0) {
            threshold *= (1.0f + m_config.hysteresis);
        }
        
        if (dist <= threshold) {
            return i;
        }
    }
    
    // Beyond all LOD levels - use lowest or cull
    return lod_chain.lod_count - 1;
}

u32 LODSystem::CalculateLODPredictive(const vec3& object_pos,
                                        const vec3& camera_pos,
                                        const vec3& camera_velocity,
                                        const LODChain& lod_chain,
                                        float object_radius,
                                        float prediction_time) const {
    // Predict future camera position
    vec3 predicted_pos = camera_pos + camera_velocity * prediction_time;
    
    // Calculate LOD for both current and predicted position
    u32 current_lod = CalculateLOD(object_pos, camera_pos, lod_chain, object_radius);
    u32 predicted_lod = CalculateLOD(object_pos, predicted_pos, lod_chain, object_radius);
    
    // Use the higher detail (lower LOD number) to ensure smooth streaming
    return std::min(current_lod, predicted_lod);
}

void LODSystem::CalculateLODBatch(const vec3* object_positions,
                                   const LODChain* lod_chains,
                                   const float* object_radii,
                                   u32 count,
                                   const vec3& camera_pos,
                                   u32* out_lod_levels) const {
    // TODO: SIMD optimization
    // For now, simple loop
    for (u32 i = 0; i < count; ++i) {
        out_lod_levels[i] = CalculateLOD(object_positions[i], 
                                          camera_pos, 
                                          lod_chains[i], 
                                          object_radii[i]);
    }
}

bool LODSystem::ShouldCull(const vec3& object_pos,
                            const vec3& camera_pos,
                            float max_distance) const {
    float dist_sq = distance_sq(object_pos, camera_pos);
    float adjusted_max = max_distance * m_config.lod_bias;
    return dist_sq > (adjusted_max * adjusted_max);
}

} // namespace action
