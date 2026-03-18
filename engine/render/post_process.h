#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"

namespace action {

// Forward declarations
class Renderer;

// -------------------------------------------------------------------------
// Tonemapping operators
// -------------------------------------------------------------------------
enum class TonemapOperator : u8 {
    Reinhard   = 0,  // Simple Reinhard (smooth roll-off)
    ACES       = 1,  // ACES filmic (default — high contrast, cinematic)
    Uncharted2 = 2,  // Uncharted 2 filmic (rich shadows)
    Linear     = 3,  // No tonemapping (raw HDR → LDR clip)
};

// -------------------------------------------------------------------------
// PostProcessSettings — full post-process parameter set for one camera
// -------------------------------------------------------------------------
struct PostProcessSettings {
    // ---- HDR / Auto-exposure ----
    bool  auto_exposure      = true;
    float exposure           = 1.0f;   // Manual EV (used when auto_exposure = false)
    float exposure_min       = 0.1f;   // Adaptation clamp (bright night scene)
    float exposure_max       = 10.0f;  // Adaptation clamp (dark day scene)
    float adaptation_speed   = 2.0f;   // EMA speed (seconds-to-adapt)

    // ---- Tonemapping ----
    TonemapOperator tonemap  = TonemapOperator::ACES;
    float           gamma    = 2.2f;   // sRGB gamma correction

    // ---- Bloom ----
    bool  bloom_enabled      = false;
    float bloom_threshold    = 0.8f;   // Luminance threshold for bright pixels
    float bloom_intensity    = 0.3f;   // Additive contribution
    float bloom_radius       = 4.0f;   // Blur kernel size (pixels)

    // ---- Motion Blur ----
    bool  motion_blur        = false;
    float motion_blur_strength = 0.5f;
    int   motion_blur_samples  = 8;

    // ---- Depth of Field ----
    bool  dof_enabled        = false;
    float dof_focus_distance = 10.0f;  // World units
    float dof_aperture       = 2.8f;   // f-stop (lower = shallower DoF)
    float dof_max_blur_radius = 8.0f;  // Maximum CoC radius (pixels)

    // ---- Vignette ----
    bool  vignette           = true;
    float vignette_intensity = 0.4f;   // 0 = off, 1 = black corners
    float vignette_radius    = 0.75f;  // Normalised radius where falloff starts

    // ---- Color Grading ----
    vec3  color_tint         = {1.0f, 1.0f, 1.0f};  // Per-channel multiplier
    float saturation         = 1.0f;   // 0 = greyscale, 1 = natural, >1 = vivid
    float contrast           = 1.0f;   // 0 = flat, 1 = natural, >1 = punchy
    float brightness         = 0.0f;   // Additive offset [-1, +1]
};

// -------------------------------------------------------------------------
// PostProcessComponent — attach to an entity that represents a camera or
// scene volume to override post-process settings in that region.
// -------------------------------------------------------------------------
struct PostProcessComponent {
    PostProcessSettings settings;
    bool active = true;     // Only the first active component is applied each frame
    float blend_weight = 1.0f;  // For smooth volume blending (0 = off, 1 = fully on)
};

// -------------------------------------------------------------------------
// PostProcessSystem — reads PostProcessComponents, blends settings, and
// pushes the result to Renderer::SetPostProcessSettings() each frame.
// -------------------------------------------------------------------------
class PostProcessSystem : public System {
public:
    PostProcessSystem(ECS* ecs, Renderer* renderer);

    void Update(float dt) override;

private:
    ECS*      m_ecs      = nullptr;
    Renderer* m_renderer = nullptr;
};

} // namespace action
