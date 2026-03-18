#include "cinematic.h"
#include "render/renderer.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <algorithm>
#include <cmath>

namespace action {

CinematicSystem::CinematicSystem(ECS* ecs, Renderer* renderer)
    : m_ecs(ecs), m_renderer(renderer)
{
    LOG_INFO("CinematicSystem initialized");
}

bool CinematicSystem::IsAnyPlaying() const {
    bool found = false;
    m_ecs->ForEach<CinematicComponent>([&](Entity, CinematicComponent& cc) {
        if (cc.playing && !cc.paused) found = true;
    });
    return found;
}

void CinematicSystem::PlayCinematic(Entity entity) {
    CinematicComponent* cc = m_ecs->GetComponent<CinematicComponent>(entity);
    if (!cc) return;
    cc->Play();
    if (cc->clip.on_start) cc->clip.on_start();
    LOG_INFO("CinematicSystem: playing '{}'", cc->clip.name);
}

void CinematicSystem::StopAll() {
    m_ecs->ForEach<CinematicComponent>([](Entity, CinematicComponent& cc) {
        cc.Stop();
    });
}

// Cubic smoothstep between two floats
static float CubicLerp(float a, float b, float t) {
    float s = t * t * (3.0f - 2.0f * t);
    return a + (b - a) * s;
}

// Apply ease exponent then cubic smoothstep
static float EasedT(float t, float ease) {
    float et = (ease != 1.0f) ? std::pow(t, ease) : t;
    return et * et * (3.0f - 2.0f * et);
}

vec3 CinematicSystem::SampleCamPos(const CinematicClip& clip, float t) const {
    if (clip.camera_track.empty()) return m_renderer->GetCamera().position;
    if (clip.camera_track.size() == 1) return clip.camera_track[0].position;

    // Find bracketing keyframes
    const auto& track = clip.camera_track;
    if (t <= track.front().time) return track.front().position;
    if (t >= track.back().time)  return track.back().position;

    for (u32 i = 1; i < static_cast<u32>(track.size()); ++i) {
        if (t <= track[i].time) {
            const CameraKeyframe& a = track[i - 1];
            const CameraKeyframe& b = track[i];
            float span = b.time - a.time;
            float local_t = (span > 1e-6f) ? (t - a.time) / span : 1.0f;
            float s = EasedT(local_t, (a.ease + b.ease) * 0.5f);
            return {CubicLerp(a.position.x, b.position.x, s),
                    CubicLerp(a.position.y, b.position.y, s),
                    CubicLerp(a.position.z, b.position.z, s)};
        }
    }
    return track.back().position;
}

vec3 CinematicSystem::SampleCamTarget(const CinematicClip& clip, float t) const {
    if (clip.camera_track.empty()) return m_renderer->GetCamera().forward + m_renderer->GetCamera().position;
    if (clip.camera_track.size() == 1) return clip.camera_track[0].look_target;

    const auto& track = clip.camera_track;
    if (t <= track.front().time) return track.front().look_target;
    if (t >= track.back().time)  return track.back().look_target;

    for (u32 i = 1; i < static_cast<u32>(track.size()); ++i) {
        if (t <= track[i].time) {
            const CameraKeyframe& a = track[i - 1];
            const CameraKeyframe& b = track[i];
            float span = b.time - a.time;
            float local_t = (span > 1e-6f) ? (t - a.time) / span : 1.0f;
            float s = EasedT(local_t, (a.ease + b.ease) * 0.5f);
            return {CubicLerp(a.look_target.x, b.look_target.x, s),
                    CubicLerp(a.look_target.y, b.look_target.y, s),
                    CubicLerp(a.look_target.z, b.look_target.z, s)};
        }
    }
    return track.back().look_target;
}

float CinematicSystem::SampleCamFOV(const CinematicClip& clip, float t) const {
    if (clip.camera_track.empty()) return 75.0f;
    if (clip.camera_track.size() == 1) return clip.camera_track[0].fov;

    const auto& track = clip.camera_track;
    if (t <= track.front().time) return track.front().fov;
    if (t >= track.back().time)  return track.back().fov;

    for (u32 i = 1; i < static_cast<u32>(track.size()); ++i) {
        if (t <= track[i].time) {
            const CameraKeyframe& a = track[i - 1];
            const CameraKeyframe& b = track[i];
            float span = b.time - a.time;
            float local_t = (span > 1e-6f) ? (t - a.time) / span : 1.0f;
            return CubicLerp(a.fov, b.fov, local_t);
        }
    }
    return track.back().fov;
}

void CinematicSystem::ApplyCameraAtTime(const CinematicClip& clip, float t) {
    if (!m_renderer) return;
    Camera& cam = m_renderer->GetCamera();

    cam.position = SampleCamPos(clip, t);

    vec3 target = SampleCamTarget(clip, t);
    vec3 dir    = {target.x - cam.position.x,
                   target.y - cam.position.y,
                   target.z - cam.position.z};
    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (len > 1e-6f) cam.forward = {dir.x/len, dir.y/len, dir.z/len};

    float fov_deg = SampleCamFOV(clip, t);
    cam.fov = Radians(fov_deg);
}

void CinematicSystem::FireDialogueEvents(CinematicComponent& cc, float prev_time, float new_time) {
    for (const DialogueLine& dl : cc.clip.dialogue) {
        if (prev_time < dl.start_time && new_time >= dl.start_time)
            LOG_INFO("[Cinematic '{}'] <{}> {}", cc.clip.name, dl.speaker, dl.text);
    }
}

void CinematicSystem::Update(float dt) {
    m_ecs->ForEach<CinematicComponent>([&](Entity /*e*/, CinematicComponent& cc) {
        if (!cc.playing || cc.paused) return;

        float prev_time   = cc.playback_time;
        cc.playback_time += dt * cc.playback_speed;

        // Fire dialogue events that crossed the playhead
        FireDialogueEvents(cc, prev_time, cc.playback_time);

        if (cc.IsDone()) {
            cc.playing = false;
            if (cc.clip.on_end) cc.clip.on_end();
            LOG_INFO("CinematicSystem: '{}' finished", cc.clip.name);
        } else if (cc.clip.loop && cc.playback_time > cc.clip.duration) {
            cc.playback_time = std::fmod(cc.playback_time, cc.clip.duration);
        }

        if (cc.override_camera)
            ApplyCameraAtTime(cc.clip, cc.playback_time);
    });
}

} // namespace action
