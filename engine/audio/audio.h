#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace action {

// Legacy stub kept for binary compatibility
class Audio {
public:
    bool Initialize() { return true; }
    void Shutdown()   {}
    void Update(float) {}
};

// -------------------------------------------------------------------

enum class SoundCategory : u8 {
    Music   = 0,
    SFX     = 1,
    Voice   = 2,
    Ambient = 3,
    COUNT   = 4
};

struct AudioClip {
    std::string   name;
    std::string   file_path;
    float         duration_sec  = 1.0f;
    bool          loop          = false;
    SoundCategory category      = SoundCategory::SFX;
    float         base_volume   = 1.0f;
    bool          is_loaded     = false;
};

// Place on entities that emit sound
struct SoundComponent {
    std::string   clip_name;
    float         volume        = 1.0f;
    float         pitch         = 1.0f;
    float         min_distance  = 1.0f;    // distance inside which volume = 1
    float         max_distance  = 50.0f;   // distance at which volume = 0
    bool          play_on_start = false;
    bool          loop          = false;
    bool          spatial       = true;
    SoundCategory category      = SoundCategory::SFX;
    // runtime fields:
    bool          is_playing    = false;
    float         playback_time = 0.0f;
    float         current_volume = 0.0f;
    float         current_pan   = 0.0f;    // -1 left .. +1 right
};

// One active listener (typically the player/camera entity)
struct AudioListenerComponent {
    bool  active       = true;
    float volume_scale = 1.0f;
};

class AudioSystem : public System {
public:
    explicit AudioSystem(ECS* ecs);

    // Clip registry
    AudioClip* LoadClip(const std::string& name, const std::string& path,
                        float duration_sec = 1.0f, bool loop = false,
                        SoundCategory category = SoundCategory::SFX);
    AudioClip* GetClip(const std::string& name);

    // Per-category and master volume [0,1]
    void  SetCategoryVolume(SoundCategory cat, float vol);
    float GetCategoryVolume(SoundCategory cat) const;
    void  SetMasterVolume(float vol);
    float GetMasterVolume() const { return m_master_volume; }

    // Music helpers (cross-fade support)
    void PlayMusic(const std::string& clip_name, float fade_in_sec = 1.0f);
    void StopMusic(float fade_out_sec = 1.0f);

    // Fire-and-forget positional sound
    void PlayOneShot(const std::string& clip_name, const vec3& world_pos,
                     float volume = 1.0f);

    void Update(float dt) override;

private:
    struct OneShotEntry {
        vec3        position;
        std::string clip_name;
        float       age;
        float       duration;
        float       volume;
    };

    void UpdateListener();
    void TickMusicFade(float dt);
    void UpdateSoundComponents(float dt);

    float CalcSpatialVolume(const vec3& source_pos, float min_dist, float max_dist) const;
    float CalcSpatialPan   (const vec3& source_pos) const;

    ECS* m_ecs = nullptr;

    std::unordered_map<std::string, AudioClip> m_clips;
    float m_category_vol[static_cast<u8>(SoundCategory::COUNT)] = {1,1,1,1};
    float m_master_volume = 1.0f;

    // Music state
    std::string m_current_music;
    std::string m_next_music;
    float m_music_fade_val   = 0.0f;  // 0=silent .. 1=full volume
    float m_music_fade_tgt   = 0.0f;
    float m_music_fade_spd   = 1.0f;
    float m_music_time       = 0.0f;

    std::vector<OneShotEntry> m_oneshots;

    vec3 m_listener_pos     = {0,0,0};
    vec3 m_listener_forward = {0,0,1};
    vec3 m_listener_right   = {1,0,0};
};

} // namespace action
