#include "audio.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <algorithm>
#include <cmath>

namespace action {

AudioSystem::AudioSystem(ECS* ecs) : m_ecs(ecs) {
    LOG_INFO("AudioSystem initialized");
}

AudioClip* AudioSystem::LoadClip(const std::string& name, const std::string& path,
                                   float duration_sec, bool loop, SoundCategory category) {
    auto& clip       = m_clips[name];
    clip.name        = name;
    clip.file_path   = path;
    clip.duration_sec = duration_sec;
    clip.loop        = loop;
    clip.category    = category;
    clip.is_loaded   = true;
    LOG_INFO("AudioClip '{}' registered (path='{}')", name, path);
    return &clip;
}

AudioClip* AudioSystem::GetClip(const std::string& name) {
    auto it = m_clips.find(name);
    return (it != m_clips.end()) ? &it->second : nullptr;
}

void AudioSystem::SetCategoryVolume(SoundCategory cat, float vol) {
    m_category_vol[static_cast<u8>(cat)] = Clamp(vol, 0.0f, 1.0f);
}

float AudioSystem::GetCategoryVolume(SoundCategory cat) const {
    return m_category_vol[static_cast<u8>(cat)];
}

void AudioSystem::SetMasterVolume(float vol) {
    m_master_volume = Clamp(vol, 0.0f, 1.0f);
}

void AudioSystem::PlayMusic(const std::string& clip_name, float fade_in_sec) {
    if (clip_name == m_current_music) return;
    m_next_music     = clip_name;
    m_music_fade_tgt = 1.0f;
    m_music_fade_spd = (fade_in_sec > 0.0f) ? 1.0f / fade_in_sec : 999.0f;
    m_music_time     = 0.0f;
    LOG_INFO("AudioSystem: queuing music '{}' (fade in {:.2f}s)", clip_name, fade_in_sec);
}

void AudioSystem::StopMusic(float fade_out_sec) {
    m_music_fade_tgt = 0.0f;
    m_music_fade_spd = (fade_out_sec > 0.0f) ? 1.0f / fade_out_sec : 999.0f;
    LOG_INFO("AudioSystem: stopping music (fade out {:.2f}s)", fade_out_sec);
}

void AudioSystem::PlayOneShot(const std::string& clip_name, const vec3& world_pos, float volume) {
    AudioClip* clip = GetClip(clip_name);
    float dur = clip ? clip->duration_sec : 1.0f;
    m_oneshots.push_back({world_pos, clip_name, 0.0f, dur, volume});
}

float AudioSystem::CalcSpatialVolume(const vec3& source_pos, float min_dist, float max_dist) const {
    vec3  delta = {source_pos.x - m_listener_pos.x,
                   source_pos.y - m_listener_pos.y,
                   source_pos.z - m_listener_pos.z};
    float dist  = std::sqrt(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
    if (dist <= min_dist) return 1.0f;
    if (dist >= max_dist) return 0.0f;
    return 1.0f - (dist - min_dist) / (max_dist - min_dist);
}

float AudioSystem::CalcSpatialPan(const vec3& source_pos) const {
    vec3 to = {source_pos.x - m_listener_pos.x,
               source_pos.y - m_listener_pos.y,
               source_pos.z - m_listener_pos.z};
    float len = std::sqrt(to.x*to.x + to.y*to.y + to.z*to.z);
    if (len < 1e-4f) return 0.0f;
    to.x /= len; to.y /= len; to.z /= len;
    return to.x * m_listener_right.x + to.y * m_listener_right.y + to.z * m_listener_right.z;
}

void AudioSystem::UpdateListener() {
    // Find the active AudioListenerComponent attached to an entity.
    // Full impl would read a TransformComponent; we record whatever position
    // game code has set on the component and use it.
    m_ecs->ForEach<AudioListenerComponent>([&](Entity /*e*/, AudioListenerComponent& lc) {
        if (!lc.active) return;
        // Listener position/orientation updated by game code setting m_listener_pos etc.
        // Nothing additional to compute here until a universal TransformComponent exists.
        (void)lc;
    });
}

void AudioSystem::TickMusicFade(float dt) {
    if (std::fabs(m_music_fade_val - m_music_fade_tgt) < 1e-4f) return;
    float delta = m_music_fade_spd * dt;
    if (m_music_fade_val < m_music_fade_tgt) {
        m_music_fade_val = std::min(m_music_fade_tgt, m_music_fade_val + delta);
        if (!m_next_music.empty()) {
            m_current_music = m_next_music;
            m_next_music.clear();
        }
    } else {
        m_music_fade_val = std::max(m_music_fade_tgt, m_music_fade_val - delta);
        if (m_music_fade_val <= 0.0f) m_current_music.clear();
    }
}

void AudioSystem::UpdateSoundComponents(float dt) {
    m_ecs->ForEach<SoundComponent>([&](Entity /*e*/, SoundComponent& sc) {
        AudioClip* clip = GetClip(sc.clip_name);
        if (!clip) { sc.is_playing = false; return; }

        if (sc.play_on_start && !sc.is_playing) {
            sc.is_playing    = true;
            sc.playback_time = 0.0f;
        }
        if (!sc.is_playing) return;

        sc.playback_time += dt;
        if (sc.playback_time >= clip->duration_sec) {
            if (sc.loop) sc.playback_time = std::fmod(sc.playback_time, clip->duration_sec);
            else         { sc.is_playing = false; sc.playback_time = 0.0f; return; }
        }

        float cat_vol = m_category_vol[static_cast<u8>(sc.category)];
        if (sc.spatial) {
            // Listener position is unknown without TransformComponent; use 1.0 as fallback.
            sc.current_volume = sc.volume * cat_vol * m_master_volume;
            sc.current_pan    = 0.0f;
        } else {
            sc.current_volume = sc.volume * cat_vol * m_master_volume;
            sc.current_pan    = 0.0f;
        }
    });
}

void AudioSystem::Update(float dt) {
    UpdateListener();
    TickMusicFade(dt);
    UpdateSoundComponents(dt);

    // Age one-shots
    m_oneshots.erase(
        std::remove_if(m_oneshots.begin(), m_oneshots.end(),
            [dt](OneShotEntry& e) {
                e.age += dt;
                return e.age >= e.duration;
            }),
        m_oneshots.end());

    if (!m_current_music.empty())
        m_music_time += dt;
}

} // namespace action
