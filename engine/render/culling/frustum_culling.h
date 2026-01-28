#pragma once

#include "core/types.h"
#include <vector>

namespace action {

/*
 * Frustum Culling System
 * 
 * CPU-based frustum culling for visible object determination
 * - AABB vs Frustum tests
 * - Sphere vs Frustum tests (faster, less accurate)
 * - Batch processing for efficiency
 */

struct CullResult {
    u32 object_index;
    bool visible;
    float distance_sq;  // For sorting
};

class FrustumCuller {
public:
    FrustumCuller() = default;
    
    // Set the culling frustum (call once per frame)
    void SetFrustum(const Frustum& frustum, const vec3& camera_pos);
    
    // Single object culling
    bool IsVisible(const AABB& bounds) const;
    bool IsVisible(const Sphere& bounds) const;
    bool IsVisible(const vec3& position, float radius) const;
    
    // Batch culling (more efficient for many objects)
    void CullAABBs(const AABB* bounds, u32 count, std::vector<CullResult>& results) const;
    void CullSpheres(const Sphere* bounds, u32 count, std::vector<CullResult>& results) const;
    
    // Cull with distance check
    void CullWithDistance(const AABB* bounds, u32 count, 
                          float max_distance, 
                          std::vector<CullResult>& results) const;
    
    // Get camera position
    const vec3& GetCameraPosition() const { return m_camera_pos; }
    
private:
    Frustum m_frustum;
    vec3 m_camera_pos;
};

/*
 * Occlusion Culling (Optional, for complex scenes)
 * 
 * Software rasterized depth buffer for CPU occlusion testing
 * Only enable if draw call count is still too high after frustum culling
 */

class OcclusionCuller {
public:
    OcclusionCuller() = default;
    
    bool Initialize(u32 width, u32 height);
    void Shutdown();
    
    // Rasterize occluders (large objects)
    void RasterizeOccluder(const vec3* vertices, u32 vertex_count,
                           const u32* indices, u32 index_count,
                           const mat4& mvp);
    
    // Test visibility against depth buffer
    bool IsOccluded(const AABB& bounds, const mat4& mvp) const;
    
    // Reset for new frame
    void Clear();
    
private:
    std::vector<float> m_depth_buffer;
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace action
