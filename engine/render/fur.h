#pragma once

/*
 * Feature 2.3 — Shell/Fin Fur Rendering
 *
 * Implements multi-pass shell fur.  Each FurComponent entity contributes
 * N RenderObjects to the render list (one per shell), offset along world-up
 * with a colour gradient from root→tip.
 *
 * A dedicated fur_vert.spv shader would offset vertices along per-vertex
 * normals (proper shell fur); until it is compiled the system uses a
 * uniform world-Y translation per shell as a visual stand-in.
 *
 * Wind animation state is advanced in Update(); fur shells are gathered
 * into the RenderList via GatherFurShells() called from Engine::Render().
 */

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include "renderer.h"

namespace action {

struct FurConfig {
    u32   shell_count     = 16;
    float fur_length      = 0.12f;    // World-space total fur depth (m)
    float gravity         = 0.35f;    // Tip droop factor [0, 1]
    float wind_strength   = 0.05f;
    vec3  wind_dir        = {1.0f, 0.0f, 0.0f};
    vec4  root_color      = {0.40f, 0.32f, 0.22f, 1.0f};
    vec4  tip_color       = {0.90f, 0.82f, 0.65f, 0.0f};
};

struct FurComponent {
    FurConfig config;
    MeshHandle base_mesh;                      // Mesh for all shell passes
    mat4       entity_transform = mat4::identity(); // Set per-frame by game code
    bool       enabled          = true;
    float      wind_phase       = 0.0f;       // Runtime: wind animation phase
};

class FurSystem : public System {
public:
    FurSystem(ECS* ecs, Renderer* renderer);

    // Advance wind phase on all fur components
    void Update(float dt) override;

    // Append shell RenderObjects to the render list — call from Engine::Render()
    void GatherFurShells(const Camera& camera, RenderList& list);

private:
    ECS*      m_ecs      = nullptr;
    Renderer* m_renderer = nullptr;
    float     m_time     = 0.0f;
};

} // namespace action
