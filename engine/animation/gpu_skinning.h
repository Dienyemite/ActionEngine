#pragma once
#include "core/types.h"
#include "gameplay/ecs/ecs.h"
#include <string>

namespace action {

class AnimationLibrary;
class Renderer;

// GPU-side buffer handle for the per-entity bone-matrix SSBO.
// Stored as u64 so Vulkan headers are not pulled into user code.
// 0 == not allocated / VK_NULL_HANDLE.
struct GpuSkinnedMeshComponent {
    std::string mesh_name;
    bool        use_gpu             = false;   // Intent: game code sets true to request GPU path
    bool        enabled             = true;
    u64         gpu_skinning_buffer = 0;       // VkBuffer (opaque)
};

// GpuSkinningSystem replaces the CPU bone-matrix upload once a compute
// shader (`skin_compute.spv`) is available.  Until then every update is a
// no-op and the existing CPUSkinMesh() path in AnimationSystem remains active.
class GpuSkinningSystem : public System {
public:
    GpuSkinningSystem(ECS* ecs, AnimationLibrary* library, Renderer* renderer);

    // Returns true when the compute pipeline is ready for GPU skinning.
    bool IsGpuReady() const { return m_gpu_ready; }

    void Update(float dt) override;

private:
    bool InitializeComputePipeline();

    ECS*              m_ecs      = nullptr;
    AnimationLibrary* m_library  = nullptr;
    Renderer*         m_renderer = nullptr;
    bool              m_gpu_ready = false;
};

} // namespace action
