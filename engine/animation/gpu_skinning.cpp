#include "gpu_skinning.h"
#include "animation_player.h"
#include "core/logging.h"

namespace action {

GpuSkinningSystem::GpuSkinningSystem(ECS* ecs, AnimationLibrary* library, Renderer* renderer)
    : m_ecs(ecs), m_library(library), m_renderer(renderer)
{
    m_gpu_ready = InitializeComputePipeline();
    if (!m_gpu_ready)
        LOG_INFO("GpuSkinningSystem: compute pipeline not ready, CPU skinning remains active");
    else
        LOG_INFO("GpuSkinningSystem initialized (GPU path active)");
}

bool GpuSkinningSystem::InitializeComputePipeline() {
    // Requires 'skin_compute.spv' which is not yet compiled.
    // Return false to keep the existing CPU skinning path active.
    LOG_INFO("GpuSkinningSystem: shader 'skin_compute.spv' not available — GPU skinning disabled");
    return false;
}

void GpuSkinningSystem::Update(float /*dt*/) {
    if (!m_gpu_ready) return;

    // --- GPU path (active once InitializeComputePipeline returns true) ---
    //
    // For each (GpuSkinnedMeshComponent, AnimationPlayerComponent) pair:
    //   1. Upload skinning_matrices to gpu_skinning_buffer via vkCmdCopyBuffer.
    //   2. Bind SSBO + vertex buffer as descriptor sets.
    //   3. vkCmdDispatch(ceil(vertex_count / 64), 1, 1).
    //
    // This replaces CPUSkinMesh() and removes the per-frame CPU-side vertex copy.
    m_ecs->ForEach<GpuSkinnedMeshComponent, AnimationPlayerComponent>(
        [&](Entity /*e*/, GpuSkinnedMeshComponent& gsc, AnimationPlayerComponent& /*ap*/) {
            if (!gsc.enabled || !gsc.use_gpu) return;
            // Dispatch stub — full impl awaits skin_compute.spv and SSBO allocation.
            (void)gsc;
        });
}

} // namespace action
