#pragma once

/*
 * AnimationLibrary — runtime store for skeletons and animation clips.
 *
 * Populated by AssetImporter after importing a skinned glTF/FBX that contains
 * skeletal data.  The AnimationSystem queries it by name each frame to build
 * skinning matrices.
 *
 * Thread-safety: not thread-safe — all access must happen on the main thread.
 * Async imports write via AssetManager callbacks on the main thread.
 */

#include "core/types.h"
#include "animation/skeleton.h"
#include "animation/animation_clip.h"
#include "animation/skinned_mesh.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace action {

class AnimationLibrary {
public:
    AnimationLibrary()  = default;
    ~AnimationLibrary() = default;

    // -----------------------------------------------------------------------
    // Registration (called by AssetImporter / scripting layer)
    // -----------------------------------------------------------------------

    // Register a skeleton.  Returns a handle.  If a skeleton with the same
    // name already exists it is replaced.
    SkeletonHandle AddSkeleton(Skeleton skeleton);

    // Register an animation clip.  Returns a handle.
    AnimationClipHandle AddClip(AnimationClip clip);

    // Register skinned mesh data (bind-pose vertices + bone influence data).
    void AddSkinnedMesh(SkinnedMeshData data);

    // -----------------------------------------------------------------------
    // Lookup
    // -----------------------------------------------------------------------
    const Skeleton*       GetSkeleton(const std::string& name) const;
    Skeleton*             GetSkeleton(const std::string& name);
    const Skeleton*       GetSkeleton(SkeletonHandle handle) const;

    const AnimationClip*  GetClip(const std::string& name) const;
    AnimationClip*        GetClip(const std::string& name);
    const AnimationClip*  GetClip(AnimationClipHandle handle) const;

    SkinnedMeshData*      GetSkinnedMesh(const std::string& mesh_name);
    const SkinnedMeshData* GetSkinnedMesh(const std::string& mesh_name) const;

    // -----------------------------------------------------------------------
    // Enumeration (for editor/debug UI)
    // -----------------------------------------------------------------------
    const std::unordered_map<std::string, Skeleton>&      GetSkeletons() const  { return m_skeletons; }
    const std::unordered_map<std::string, AnimationClip>& GetClips() const      { return m_clips; }

    // Stats
    u32 SkeletonCount()    const { return (u32)m_skeletons.size(); }
    u32 ClipCount()        const { return (u32)m_clips.size(); }
    u32 SkinnedMeshCount() const { return (u32)m_skinned_meshes.size(); }

private:
    std::unordered_map<std::string, Skeleton>      m_skeletons;
    std::unordered_map<std::string, AnimationClip> m_clips;
    std::unordered_map<std::string, SkinnedMeshData> m_skinned_meshes;

    // Handle-to-name maps for O(1) lookup by handle
    std::unordered_map<u32, std::string> m_skeleton_handles;   // handle.index -> name
    std::unordered_map<u32, std::string> m_clip_handles;

    u32 m_next_skeleton_handle = 1;
    u32 m_next_clip_handle     = 1;
};

} // namespace action
