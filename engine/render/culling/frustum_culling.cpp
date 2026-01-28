#include "frustum_culling.h"
#include "core/math/math.h"

namespace action {

void FrustumCuller::SetFrustum(const Frustum& frustum, const vec3& camera_pos) {
    m_frustum = frustum;
    m_camera_pos = camera_pos;
}

bool FrustumCuller::IsVisible(const AABB& bounds) const {
    return m_frustum.intersects(bounds);
}

bool FrustumCuller::IsVisible(const Sphere& bounds) const {
    return m_frustum.intersects(bounds);
}

bool FrustumCuller::IsVisible(const vec3& position, float radius) const {
    return m_frustum.intersects(Sphere{position, radius});
}

void FrustumCuller::CullAABBs(const AABB* bounds, u32 count, 
                               std::vector<CullResult>& results) const {
    results.clear();
    results.reserve(count);
    
    for (u32 i = 0; i < count; ++i) {
        CullResult result;
        result.object_index = i;
        result.visible = m_frustum.intersects(bounds[i]);
        result.distance_sq = distance_sq(bounds[i].center(), m_camera_pos);
        results.push_back(result);
    }
}

void FrustumCuller::CullSpheres(const Sphere* bounds, u32 count,
                                 std::vector<CullResult>& results) const {
    results.clear();
    results.reserve(count);
    
    for (u32 i = 0; i < count; ++i) {
        CullResult result;
        result.object_index = i;
        result.visible = m_frustum.intersects(bounds[i]);
        result.distance_sq = distance_sq(bounds[i].center, m_camera_pos);
        results.push_back(result);
    }
}

void FrustumCuller::CullWithDistance(const AABB* bounds, u32 count,
                                      float max_distance,
                                      std::vector<CullResult>& results) const {
    results.clear();
    results.reserve(count);
    
    float max_dist_sq = max_distance * max_distance;
    
    for (u32 i = 0; i < count; ++i) {
        float dist_sq = distance_sq(bounds[i].center(), m_camera_pos);
        
        // Early distance rejection
        if (dist_sq > max_dist_sq) {
            continue;
        }
        
        // Frustum test
        if (m_frustum.intersects(bounds[i])) {
            CullResult result;
            result.object_index = i;
            result.visible = true;
            result.distance_sq = dist_sq;
            results.push_back(result);
        }
    }
}

// Occlusion Culler

bool OcclusionCuller::Initialize(u32 width, u32 height) {
    m_width = width;
    m_height = height;
    m_depth_buffer.resize(width * height, 1.0f);
    return true;
}

void OcclusionCuller::Shutdown() {
    m_depth_buffer.clear();
    m_width = 0;
    m_height = 0;
}

void OcclusionCuller::RasterizeOccluder(const vec3* vertices, u32 vertex_count,
                                         const u32* indices, u32 index_count,
                                         const mat4& mvp) {
    // TODO: Implement software rasterization for occluders
    // This is optional for GTX 660 - only enable if needed
    (void)vertices;
    (void)vertex_count;
    (void)indices;
    (void)index_count;
    (void)mvp;
}

bool OcclusionCuller::IsOccluded(const AABB& bounds, const mat4& mvp) const {
    // TODO: Test AABB against depth buffer
    (void)bounds;
    (void)mvp;
    return false; // Default: not occluded
}

void OcclusionCuller::Clear() {
    std::fill(m_depth_buffer.begin(), m_depth_buffer.end(), 1.0f);
}

} // namespace action
