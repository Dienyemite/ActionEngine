#pragma once

/*
 * TerrainSystem — heightmap-based terrain for large outdoor worlds.
 *
 * The terrain is divided into a grid of square chunks.  Each chunk owns one
 * full-resolution mesh (LOD 0) and one half-resolution mesh (LOD 1).  The
 * system selects between them based on camera distance.
 *
 * Key public API:
 *   GenerateProcedural(seed) — create layered-sinusoid landscape.
 *   LoadHeightmap(data, w, h) — load heights from a pre-built float array.
 *   GetHeightAt(wx, wz)       — world-space height query (bilinear interp).
 *   GetNormalAt(wx, wz)       — world-space surface normal (finite diff).
 *   GatherVisibleChunks(cam, list) — frustum-cull and add terrain to a RenderList.
 *
 * Integration:
 *   1. Engine::Render() calls GatherVisibleChunks() after GatherVisibleObjects().
 *   2. Character controller calls GetHeightAt() / GetNormalAt() for grounding.
 *   3. IK foot-plant solver calls GetHeightAt() to drive foot IK targets.
 */

#include "core/types.h"
#include "core/math/math.h"
#include "assets/asset_manager.h"
#include "render/renderer.h"
#include <vector>

namespace action {

// -------------------------------------------------------------------------
// TerrainConfig
// -------------------------------------------------------------------------
struct TerrainConfig {
    float world_size    = 1024.f;  // Total side length of the terrain (metres)
    float height_scale  = 120.f;   // Maximum height above the terrain base plane
    float uv_scale      = 0.05f;   // Texture UV tiles per metre
    u32   chunk_grid    = 8;       // NxN chunks (total: chunk_grid * chunk_grid)
    u32   chunk_quads   = 32;      // Quads per chunk edge (verts = quads + 1)

    // LOD thresholds (squared distances from camera to chunk centre).
    float lod1_dist_sq  = 200.f * 200.f;   // Beyond this → LOD 1 (half-res)
    float lod_cull_dist_sq = 600.f * 600.f; // Beyond this → culled entirely
};

// -------------------------------------------------------------------------
// HeightmapData — internal 2D grid of normalised heights [0, 1]
// -------------------------------------------------------------------------
struct HeightmapData {
    std::vector<float> heights;  // heights[y * width + x]
    u32 width  = 0;
    u32 height = 0;

    bool is_valid() const { return width > 0 && height > 0 && !heights.empty(); }

    // Nearest-sample height at grid coordinates (clamped).
    float Get(u32 x, u32 y) const;

    // Bilinear interpolation at normalised coordinates [0, 1].
    float SampleBilinear(float nx, float nz) const;
};

// -------------------------------------------------------------------------
// TerrainChunk — one NxN patch of the terrain mesh
// -------------------------------------------------------------------------
struct TerrainChunk {
    MeshHandle mesh_lod0;       // Full resolution (LOD 0)
    MeshHandle mesh_lod1;       // Half resolution (LOD 1)
    AABB       bounds;          // World-space AABB for frustum culling
    vec3       world_origin;    // Bottom-left corner of this chunk in world space
    float      chunk_size_ws;   // Side length of this chunk in world units
    bool       ready = false;   // True once both meshes are uploaded
};

// -------------------------------------------------------------------------
// TerrainSystem — manages terrain data, mesh generation, and rendering
// -------------------------------------------------------------------------
class TerrainSystem {
public:
    TerrainSystem() = default;
    ~TerrainSystem();

    // Must be called once with a valid AssetManager before build methods.
    void Initialize(AssetManager* assets);

    // Build terrain from a flat float array of normalised heights [0, 1].
    // width * height must equal the number of elements.
    void LoadHeightmap(const std::vector<float>& heights, u32 width, u32 height,
                       const TerrainConfig& config = {});

    // Build terrain procedurally using layered sinusoids.
    void GenerateProcedural(u32 seed = 42u, const TerrainConfig& config = {});

    // Per-frame update: upload pending chunks + tick LOD.
    void Update(float dt);

    // Add visible terrain chunks to 'out_list' for the current frame.
    void GatherVisibleChunks(const Camera& camera, RenderList& out_list) const;

    // Height and normal queries (bilinear interpolation).
    float GetHeightAt(float world_x, float world_z) const;
    vec3  GetNormalAt(float world_x, float world_z) const;

    // True if a world-space point is at or below the terrain surface.
    bool IsOnGround(const vec3& pos, float margin = 0.05f) const;

    const TerrainConfig& GetConfig() const { return m_config; }
    bool IsReady() const { return m_ready; }

private:
    // Build m_heightmap using the provided procedural height function.
    void BuildHeightmap(std::function<float(float, float)> height_fn,
                        u32 resolution);

    // Generate mesh vertices + indices for one chunk at grid position (cx, cz).
    // quads_per_edge controls the mesh resolution (LOD).
    void BuildChunkMesh(u32 cx, u32 cz, u32 quads_per_edge,
                        std::vector<Vertex>& out_verts,
                        std::vector<u32>& out_indices) const;

    // Convert world-space (wx, wz) → normalised heightmap coordinates [0,1].
    void WorldToNorm(float wx, float wz, float& out_nx, float& out_nz) const;

    AssetManager* m_assets  = nullptr;
    TerrainConfig m_config;
    HeightmapData m_heightmap;
    std::vector<TerrainChunk> m_chunks;  // size = chunk_grid * chunk_grid
    bool m_ready = false;
};

} // namespace action
