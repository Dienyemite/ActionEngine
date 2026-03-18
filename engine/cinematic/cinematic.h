#pragma once
#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include <string>
#include <vector>
#include <functional>

namespace action {

struct CameraKeyframe {
    float time        = 0.0f;
    vec3  position    = {0,0,0};
    vec3  look_target = {0,0,1};
    float fov         = 75.0f;
    float ease        = 1.0f;   // exponent for SmoothStep
};

struct ActorTrack {
    Entity      entity         = INVALID_ENTITY;
    std::string animation_clip;
    float       start_time     = 0.0f;
    float       blend_time     = 0.2f;
};

struct DialogueLine {
    float       start_time     = 0.0f;
    float       end_time       = 1.0f;
    std::string text;
    std::string speaker;
};

struct CinematicClip {
    std::string                   name;
    float                         duration      = 10.0f;
    bool                          loop          = false;
    bool                          skip_allowed  = true;
    std::vector<CameraKeyframe>   camera_track;
    std::vector<ActorTrack>       actor_tracks;
    std::vector<DialogueLine>     dialogue;
    std::function<void()>         on_start;
    std::function<void()>         on_end;
};

struct CinematicComponent {
    CinematicClip clip;
    bool          playing         = false;
    bool          paused          = false;
    float         playback_time   = 0.0f;
    float         playback_speed  = 1.0f;
    bool          override_camera = true;

    void Play()  { playing = true;  paused = false; playback_time = 0.0f; }
    void Pause() { paused  = true; }
    void Stop()  { playing = false; paused = false; playback_time = 0.0f; }
    void Skip()  { if (clip.skip_allowed) playback_time = clip.duration; }
    bool IsDone() const { return !clip.loop && playback_time >= clip.duration; }
};

class Renderer; // forward declaration

class CinematicSystem : public System {
public:
    CinematicSystem(ECS* ecs, Renderer* renderer);

    bool IsAnyPlaying() const;
    void PlayCinematic(Entity entity);
    void StopAll();

    void Update(float dt) override;

private:
    vec3  SampleCamPos   (const CinematicClip& clip, float t) const;
    vec3  SampleCamTarget(const CinematicClip& clip, float t) const;
    float SampleCamFOV   (const CinematicClip& clip, float t) const;
    void  ApplyCameraAtTime(const CinematicClip& clip, float t);
    void  FireDialogueEvents(CinematicComponent& cc, float prev_time, float new_time);

    ECS*      m_ecs      = nullptr;
    Renderer* m_renderer = nullptr;
};

} // namespace action
