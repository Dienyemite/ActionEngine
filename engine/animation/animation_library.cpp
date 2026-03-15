#include "animation_library.h"
#include "core/logging.h"

namespace action {

// ---------------------------------------------------------------------------
// AddSkeleton
// ---------------------------------------------------------------------------
SkeletonHandle AnimationLibrary::AddSkeleton(Skeleton skeleton) {
    const std::string name = skeleton.name;

    SkeletonHandle handle;
    handle.index      = m_next_skeleton_handle++;
    handle.generation = 1;

    m_skeleton_handles[handle.index] = name;
    m_skeletons[name] = std::move(skeleton);

    LOG_DEBUG("AnimationLibrary: registered skeleton '{}' ({} bones)",
              name, m_skeletons[name].bones.size());
    return handle;
}

// ---------------------------------------------------------------------------
// AddClip
// ---------------------------------------------------------------------------
AnimationClipHandle AnimationLibrary::AddClip(AnimationClip clip) {
    const std::string name = clip.name;

    AnimationClipHandle handle;
    handle.index      = m_next_clip_handle++;
    handle.generation = 1;

    m_clip_handles[handle.index] = name;
    m_clips[name] = std::move(clip);

    LOG_DEBUG("AnimationLibrary: registered clip '{}' ({:.2f}s)",
              name, m_clips[name].duration);
    return handle;
}

// ---------------------------------------------------------------------------
// AddSkinnedMesh
// ---------------------------------------------------------------------------
void AnimationLibrary::AddSkinnedMesh(SkinnedMeshData data) {
    const std::string mesh_name = data.mesh_name;
    m_skinned_meshes[mesh_name] = std::move(data);
    LOG_DEBUG("AnimationLibrary: registered skinned mesh '{}'", mesh_name);
}

// ---------------------------------------------------------------------------
// Lookup by name
// ---------------------------------------------------------------------------
const Skeleton* AnimationLibrary::GetSkeleton(const std::string& name) const {
    auto it = m_skeletons.find(name);
    return it != m_skeletons.end() ? &it->second : nullptr;
}

Skeleton* AnimationLibrary::GetSkeleton(const std::string& name) {
    auto it = m_skeletons.find(name);
    return it != m_skeletons.end() ? &it->second : nullptr;
}

const Skeleton* AnimationLibrary::GetSkeleton(SkeletonHandle handle) const {
    auto it = m_skeleton_handles.find(handle.index);
    if (it == m_skeleton_handles.end()) return nullptr;
    return GetSkeleton(it->second);
}

const AnimationClip* AnimationLibrary::GetClip(const std::string& name) const {
    auto it = m_clips.find(name);
    return it != m_clips.end() ? &it->second : nullptr;
}

AnimationClip* AnimationLibrary::GetClip(const std::string& name) {
    auto it = m_clips.find(name);
    return it != m_clips.end() ? &it->second : nullptr;
}

const AnimationClip* AnimationLibrary::GetClip(AnimationClipHandle handle) const {
    auto it = m_clip_handles.find(handle.index);
    if (it == m_clip_handles.end()) return nullptr;
    return GetClip(it->second);
}

SkinnedMeshData* AnimationLibrary::GetSkinnedMesh(const std::string& mesh_name) {
    auto it = m_skinned_meshes.find(mesh_name);
    return it != m_skinned_meshes.end() ? &it->second : nullptr;
}

const SkinnedMeshData* AnimationLibrary::GetSkinnedMesh(const std::string& mesh_name) const {
    auto it = m_skinned_meshes.find(mesh_name);
    return it != m_skinned_meshes.end() ? &it->second : nullptr;
}

} // namespace action
