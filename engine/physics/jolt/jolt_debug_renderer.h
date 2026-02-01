#pragma once

/*
 * Jolt Debug Renderer
 * 
 * Renders physics shapes, contacts, and constraints for debugging.
 * This is a placeholder - full implementation requires integration
 * with the render system.
 */

#include "core/types.h"
#include "core/math/math.h"

namespace action {

class JoltDebugRenderer {
public:
    JoltDebugRenderer() = default;
    ~JoltDebugRenderer() = default;
    
    void Initialize() {}
    void Shutdown() {}
    
    // Draw primitives (to be integrated with Renderer)
    void DrawLine(const vec3& from, const vec3& to, uint32_t color) {}
    void DrawTriangle(const vec3& v0, const vec3& v1, const vec3& v2, uint32_t color) {}
    void DrawBox(const vec3& min, const vec3& max, uint32_t color) {}
    void DrawSphere(const vec3& center, float radius, uint32_t color) {}
    void DrawCapsule(const vec3& base, const vec3& tip, float radius, uint32_t color) {}
    
    // Flush all primitives to the GPU
    void Flush() {}
    
    // Settings
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    
private:
    bool m_enabled = false;
};

} // namespace action
