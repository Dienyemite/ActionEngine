#include "post_process.h"
#include "renderer.h"
#include "core/logging.h"

namespace action {

PostProcessSystem::PostProcessSystem(ECS* ecs, Renderer* renderer)
    : m_ecs(ecs), m_renderer(renderer)
{
}

void PostProcessSystem::Update(float dt) {
    if (!m_ecs || !m_renderer) return;

    // ----------------------------------------------------------------
    // Gather active PostProcessComponents from the scene.
    // In a volume system, multiple overlapping volumes would be
    // blended by weight; here we take the first active component.
    // ----------------------------------------------------------------
    PostProcessSettings blended = m_renderer->GetPostProcessSettings(); // keep defaults

    bool found = false;
    m_ecs->ForEach<PostProcessComponent>([&](Entity /*entity*/, PostProcessComponent& comp) {
        if (!comp.active || found) return;
        found = true;

        float w = Clamp(comp.blend_weight, 0.0f, 1.0f);

        // Lerp numeric fields; bool fields take the new value when weight >= 0.5
        auto& s = comp.settings;
        auto& b = blended;

        b.auto_exposure      = (w >= 0.5f) ? s.auto_exposure : b.auto_exposure;
        b.exposure           = Lerp(b.exposure,           s.exposure,           w);
        b.exposure_min       = Lerp(b.exposure_min,       s.exposure_min,       w);
        b.exposure_max       = Lerp(b.exposure_max,       s.exposure_max,       w);
        b.adaptation_speed   = Lerp(b.adaptation_speed,   s.adaptation_speed,   w);

        b.tonemap            = (w >= 0.5f) ? s.tonemap : b.tonemap;
        b.gamma              = Lerp(b.gamma,              s.gamma,              w);

        b.bloom_enabled      = (w >= 0.5f) ? s.bloom_enabled : b.bloom_enabled;
        b.bloom_threshold    = Lerp(b.bloom_threshold,    s.bloom_threshold,    w);
        b.bloom_intensity    = Lerp(b.bloom_intensity,    s.bloom_intensity,    w);
        b.bloom_radius       = Lerp(b.bloom_radius,       s.bloom_radius,       w);

        b.motion_blur        = (w >= 0.5f) ? s.motion_blur : b.motion_blur;
        b.motion_blur_strength = Lerp(b.motion_blur_strength, s.motion_blur_strength, w);

        b.dof_enabled        = (w >= 0.5f) ? s.dof_enabled : b.dof_enabled;
        b.dof_focus_distance = Lerp(b.dof_focus_distance, s.dof_focus_distance, w);
        b.dof_aperture       = Lerp(b.dof_aperture,       s.dof_aperture,       w);
        b.dof_max_blur_radius = Lerp(b.dof_max_blur_radius, s.dof_max_blur_radius, w);

        b.vignette           = (w >= 0.5f) ? s.vignette : b.vignette;
        b.vignette_intensity = Lerp(b.vignette_intensity, s.vignette_intensity, w);
        b.vignette_radius    = Lerp(b.vignette_radius,    s.vignette_radius,    w);

        b.color_tint.x       = Lerp(b.color_tint.x,      s.color_tint.x,       w);
        b.color_tint.y       = Lerp(b.color_tint.y,      s.color_tint.y,       w);
        b.color_tint.z       = Lerp(b.color_tint.z,      s.color_tint.z,       w);
        b.saturation         = Lerp(b.saturation,         s.saturation,         w);
        b.contrast           = Lerp(b.contrast,           s.contrast,           w);
        b.brightness         = Lerp(b.brightness,         s.brightness,         w);
    });

    // Push blended settings (even if no component found — keeps renderer defaults)
    m_renderer->SetPostProcessSettings(blended);

    // Advance CPU-side eye adaptation
    m_renderer->UpdatePostProcess(dt);
}

} // namespace action
