#include "fur.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <cmath>

namespace action {

FurSystem::FurSystem(ECS* ecs, Renderer* renderer)
    : m_ecs(ecs), m_renderer(renderer)
{
    LOG_INFO("FurSystem initialized ({} max shells per entity)", 32);
}

void FurSystem::Update(float dt) {
    m_time += dt;
    m_ecs->ForEach<FurComponent>([&](Entity /*e*/, FurComponent& fur) {
        if (!fur.enabled) return;
        // Advance wind phase at 0.5 Hz
        fur.wind_phase = m_time * TWO_PI * 0.5f;
    });
}

void FurSystem::GatherFurShells(const Camera& camera, RenderList& list) {
    (void)camera; // Used in the future for distance-based LOD (shell count reduction)

    m_ecs->ForEach<FurComponent>([&](Entity /*e*/, FurComponent& fur) {
        if (!fur.enabled || !fur.base_mesh.is_valid()) return;

        const FurConfig& cfg = fur.config;
        const u32 shells = cfg.shell_count;
        if (shells == 0) return;

        float wind_x = std::sin(fur.wind_phase) * cfg.wind_strength * cfg.wind_dir.x;
        float wind_z = std::cos(fur.wind_phase * 0.7f) * cfg.wind_strength * cfg.wind_dir.z;

        for (u32 s = 0; s < shells; ++s) {
            float t      = static_cast<float>(s) / static_cast<float>(shells);
            float offset = t * cfg.fur_length;

            // Gravity droop: quadratic sag on the tip (t=1)
            float grav   = t * t * cfg.gravity * cfg.fur_length;

            mat4 shell_xform = fur.entity_transform;
            // Offset along world-up (Y) minus gravity sag + wind
            shell_xform.columns[3].y += offset - grav;
            shell_xform.columns[3].x += t * wind_x;
            shell_xform.columns[3].z += t * wind_z;

            // Root-to-tip colour gradient; alpha encodes shell index for future fur shader
            vec4 color;
            color.x = Lerp(cfg.root_color.x, cfg.tip_color.x, t);
            color.y = Lerp(cfg.root_color.y, cfg.tip_color.y, t);
            color.z = Lerp(cfg.root_color.z, cfg.tip_color.z, t);
            color.w = Lerp(cfg.root_color.w, cfg.tip_color.w, t);

            RenderObject ro;
            ro.mesh      = fur.base_mesh;
            ro.transform = shell_xform;
            ro.color     = color;
            ro.lod_level = 0;
            list.opaque.push_back(ro);
            ++list.total_draw_calls;
        }
    });
}

} // namespace action
