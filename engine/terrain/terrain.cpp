#include "terrain.h"
#include "core/math/math.h"
#include "core/logging.h"
#include <cmath>
#include <functional>

namespace action {

// ---------------------------------------------------------------------------
// HeightmapData
// ---------------------------------------------------------------------------
float HeightmapData::Get(u32 x, u32 y) const {
    x = std::min(x, width  - 1u);
    y = std::min(y, height - 1u);
    return heights[y * width + x];
}

float HeightmapData::SampleBilinear(float nx, float nz) const {
    if (!is_valid()) return 0.f;

    // Map normalised [0,1] coordinates to heightmap pixel space.
    const float fx = Clamp(nx, 0.f, 1.f) * (float)(width  - 1u);
    const float fz = Clamp(nz, 0.f, 1.f) * (float)(height - 1u);

    const u32 x0 = (u32)fx;
    const u32 z0 = (u32)fz;
    const u32 x1 = std::min(x0 + 1u, width  - 1u);
    const u32 z1 = std::min(z0 + 1u, height - 1u);

    const float tx = fx - (float)x0;
    const float tz = fz - (float)z0;

    const float h00 = Get(x0, z0);
    const float h10 = Get(x1, z0);
    const float h01 = Get(x0, z1);
    const float h11 = Get(x1, z1);

    // Bilinear blend.
    const float h0 = Lerp(h00, h10, tx);
    const float h1 = Lerp(h01, h11, tx);
    return Lerp(h0, h1, tz);
}

// ---------------------------------------------------------------------------
// TerrainSystem
// ---------------------------------------------------------------------------
TerrainSystem::~TerrainSystem() {
    // MeshHandles are ref-counted by AssetManager; no explicit release needed here.
}

void TerrainSystem::Initialize(AssetManager* assets) {
    m_assets = assets;
}

// ---------------------------------------------------------------------------
// WorldToNorm
// ---------------------------------------------------------------------------
void TerrainSystem::WorldToNorm(float wx, float wz,
                                 float& out_nx, float& out_nz) const {
    out_nx = (wx + m_config.world_size * 0.5f) / m_config.world_size;
    out_nz = (wz + m_config.world_size * 0.5f) / m_config.world_size;
}

// ---------------------------------------------------------------------------
// GetHeightAt / GetNormalAt / IsOnGround
// ---------------------------------------------------------------------------
float TerrainSystem::GetHeightAt(float wx, float wz) const {
    if (!m_heightmap.is_valid()) return 0.f;
    float nx, nz;
    WorldToNorm(wx, wz, nx, nz);
    return m_heightmap.SampleBilinear(nx, nz) * m_config.height_scale;
}

vec3 TerrainSystem::GetNormalAt(float wx, float wz) const {
    if (!m_heightmap.is_valid()) return { 0.f, 1.f, 0.f };

    // Finite differences over a small step to approximate ∂h/∂x and ∂h/∂z.
    const float step = m_config.world_size / (float)(m_heightmap.width - 1u);
    const float hL = GetHeightAt(wx - step, wz);
    const float hR = GetHeightAt(wx + step, wz);
    const float hD = GetHeightAt(wx, wz - step);
    const float hU = GetHeightAt(wx, wz + step);

    // Tangent vectors along X and Z, then cross product.
    const vec3 tx = { 2.f * step, hR - hL, 0.f };
    const vec3 tz = { 0.f,        hU - hD, 2.f * step };
    return cross(tz, tx).normalized();
}

bool TerrainSystem::IsOnGround(const vec3& pos, float margin) const {
    return pos.y <= GetHeightAt(pos.x, pos.z) + margin;
}

// ---------------------------------------------------------------------------
// BuildHeightmap
// ---------------------------------------------------------------------------
void TerrainSystem::BuildHeightmap(std::function<float(float, float)> height_fn,
                                    u32 resolution)
{
    m_heightmap.width  = resolution;
    m_heightmap.height = resolution;
    m_heightmap.heights.resize(resolution * resolution);

    const float inv_res = 1.f / (float)(resolution - 1u);
    for (u32 z = 0; z < resolution; ++z) {
        for (u32 x = 0; x < resolution; ++x) {
            // Map grid position to world space for the function.
            const float wx = ((float)x * inv_res - 0.5f) * m_config.world_size;
            const float wz = ((float)z * inv_res - 0.5f) * m_config.world_size;
            m_heightmap.heights[z * resolution + x] =
                Clamp(height_fn(wx, wz), 0.f, 1.f);
        }
    }
}

// ---------------------------------------------------------------------------
// LoadHeightmap
// ---------------------------------------------------------------------------
void TerrainSystem::LoadHeightmap(const std::vector<float>& heights,
                                    u32 width, u32 height,
                                    const TerrainConfig& config)
{
    m_config  = config;
    m_heightmap.heights = heights;
    m_heightmap.width   = width;
    m_heightmap.height  = height;

    // Build chunk meshes.
    const u32 n = m_config.chunk_grid;
    m_chunks.resize(n * n);

    const float chunk_ws = m_config.world_size / (float)n;

    for (u32 cz = 0; cz < n; ++cz) {
        for (u32 cx = 0; cx < n; ++cx) {
            TerrainChunk& chunk = m_chunks[cz * n + cx];
            chunk.chunk_size_ws = chunk_ws;
            chunk.world_origin  = {
                -m_config.world_size * 0.5f + (float)cx * chunk_ws,
                 0.f,
                -m_config.world_size * 0.5f + (float)cz * chunk_ws
            };

            // Build LOD 0 mesh.
            std::vector<Vertex> verts;
            std::vector<u32>    indices;
            BuildChunkMesh(cx, cz, m_config.chunk_quads, verts, indices);

            MeshData md;
            md.name     = "terrain_" + std::to_string(cx) + "_" + std::to_string(cz);
            md.vertices = verts;
            md.indices  = indices;
            md.PackVertexData();

            // Compute AABB from first LOD vertices.
            for (const auto& v : verts) md.bounds.expand(v.position);
            chunk.bounds = md.bounds;

            chunk.mesh_lod0 = m_assets->CreateMesh(md);

            // Build LOD 1 mesh (half resolution, min 4 quads).
            const u32 lod1_quads = std::max(4u, m_config.chunk_quads / 2u);
            BuildChunkMesh(cx, cz, lod1_quads, verts, indices);

            MeshData md1;
            md1.name    = md.name + "_lod1";
            md1.vertices = verts;
            md1.indices  = indices;
            md1.PackVertexData();
            for (const auto& v : verts) md1.bounds.expand(v.position);

            chunk.mesh_lod1 = m_assets->CreateMesh(md1);
            chunk.ready = true;
        }
    }

    m_ready = true;
    LOG_INFO("Terrain loaded: {} chunks ({} world units, {} height scale)",
             n * n, (int)m_config.world_size, (int)m_config.height_scale);
}

// ---------------------------------------------------------------------------
// GenerateProcedural
// ---------------------------------------------------------------------------
void TerrainSystem::GenerateProcedural(u32 seed, const TerrainConfig& config) {
    m_config = config;

    // Layered sinusoid height function.
    // The seed offsets the phases so different seeds produce different terrain.
    const float seed_f = (float)seed;

    // Resolution: use 128 samples per chunk for a good heightmap resolution.
    const u32 resolution = m_config.chunk_grid * 16u;

    BuildHeightmap([&](float wx, float wz) -> float {
        float h = 0.f;
        float amp  = 1.0f;
        float freq = 1.f / (m_config.world_size * 0.5f);

        // 5 octaves of sinusoidal noise.
        for (int oct = 0; oct < 5; ++oct) {
            const float phase_x = seed_f * 0.137f * (float)(oct + 1);
            const float phase_z = seed_f * 0.251f * (float)(oct + 1);
            h += amp * std::sin(wx * freq * TWO_PI + phase_x)
                     * std::cos(wz * freq * TWO_PI + phase_z);
            amp  *= 0.5f;
            freq *= 2.1f;
        }
        // Normalize: range of sum ≈ [-2, 2] → [0, 1]
        return (h + 2.f) * 0.25f;
    }, resolution);

    // Delegate to LoadHeightmap (no copy, just reuse the buffer).
    LoadHeightmap(m_heightmap.heights, m_heightmap.width, m_heightmap.height,
                  m_config);
}

// ---------------------------------------------------------------------------
// BuildChunkMesh
// ---------------------------------------------------------------------------
void TerrainSystem::BuildChunkMesh(u32 cx, u32 cz, u32 quads_per_edge,
                                    std::vector<Vertex>& out_verts,
                                    std::vector<u32>&   out_indices) const
{
    const u32   vpe      = quads_per_edge + 1u;  // vertices per edge
    const float chunk_ws = m_config.world_size / (float)m_config.chunk_grid;
    const float step     = chunk_ws / (float)quads_per_edge;

    const float origin_x = -m_config.world_size * 0.5f + (float)cx * chunk_ws;
    const float origin_z = -m_config.world_size * 0.5f + (float)cz * chunk_ws;

    out_verts.resize(vpe * vpe);
    out_indices.resize(quads_per_edge * quads_per_edge * 6u);

    // Build vertices.
    for (u32 iz = 0; iz < vpe; ++iz) {
        for (u32 ix = 0; ix < vpe; ++ix) {
            const float wx = origin_x + (float)ix * step;
            const float wz = origin_z + (float)iz * step;
            const float wy = GetHeightAt(wx, wz);

            Vertex& v = out_verts[iz * vpe + ix];
            v.position = { wx, wy, wz };
            v.normal   = GetNormalAt(wx, wz);
            v.uv       = { wx * m_config.uv_scale, wz * m_config.uv_scale };
            v.color    = { 0.45f, 0.52f, 0.30f };  // Muted green (grass)
        }
    }

    // Build indices (counter-clockwise winding, y-up convention).
    u32 idx = 0;
    for (u32 iz = 0; iz < quads_per_edge; ++iz) {
        for (u32 ix = 0; ix < quads_per_edge; ++ix) {
            const u32 tl = iz * vpe + ix;
            const u32 tr = tl + 1u;
            const u32 bl = tl + vpe;
            const u32 br = bl + 1u;

            // Triangle 1
            out_indices[idx++] = tl;
            out_indices[idx++] = bl;
            out_indices[idx++] = tr;

            // Triangle 2
            out_indices[idx++] = tr;
            out_indices[idx++] = bl;
            out_indices[idx++] = br;
        }
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void TerrainSystem::Update(float /*dt*/) {
    // No streaming work needed at this level: all chunks are built once at
    // init.  This hook is available for future dynamic terrain modifications.
}

// ---------------------------------------------------------------------------
// GatherVisibleChunks
// ---------------------------------------------------------------------------
void TerrainSystem::GatherVisibleChunks(const Camera& camera,
                                         RenderList& out_list) const
{
    if (!m_ready) return;

    const Frustum frustum    = camera.GetFrustum();
    const vec3&   cam_pos    = camera.position;
    const MaterialHandle no_mat{};

    for (const TerrainChunk& chunk : m_chunks) {
        if (!chunk.ready) continue;

        // Frustum cull.
        bool any_visible = false;
        for (int p = 0; p < 6; ++p) {
            const vec4& plane = frustum.planes[p];
            // Check AABB against plane (positive half-space test).
            vec3 pv = {
                (plane.x > 0.f) ? chunk.bounds.max.x : chunk.bounds.min.x,
                (plane.y > 0.f) ? chunk.bounds.max.y : chunk.bounds.min.y,
                (plane.z > 0.f) ? chunk.bounds.max.z : chunk.bounds.min.z
            };
            if (plane.x * pv.x + plane.y * pv.y + plane.z * pv.z + plane.w < 0.f) {
                any_visible = false;
                break;
            }
            any_visible = true;
        }
        // Re-check: all planes passed → visible (the loop above has a logic flaw).
        // Use proper inside-out test.
        {
            bool outside = false;
            for (int p = 0; p < 6 && !outside; ++p) {
                const vec4& pl = frustum.planes[p];
                // Most-positive corner relative to plane normal.
                vec3 pv = {
                    (pl.x > 0.f) ? chunk.bounds.max.x : chunk.bounds.min.x,
                    (pl.y > 0.f) ? chunk.bounds.max.y : chunk.bounds.min.y,
                    (pl.z > 0.f) ? chunk.bounds.max.z : chunk.bounds.min.z
                };
                if (pl.x * pv.x + pl.y * pv.y + pl.z * pv.z + pl.w < 0.f)
                    outside = true;
            }
            if (outside) continue;
        }

        // Distance LOD selection.
        const vec3  centre  = chunk.bounds.center();
        const float dist_sq = distance_sq(cam_pos, centre);

        if (dist_sq > m_config.lod_cull_dist_sq) continue;

        const MeshHandle mesh = (dist_sq <= m_config.lod1_dist_sq)
                              ? chunk.mesh_lod0
                              : chunk.mesh_lod1;

        RenderObject ro;
        ro.mesh        = mesh;
        ro.material    = no_mat;
        ro.transform   = mat4::identity();   // Terrain verts are in world space
        ro.bounds      = chunk.bounds;
        ro.distance_sq = dist_sq;
        ro.color       = { 1.f, 1.f, 1.f, 1.f };
        ro.lod_level   = (dist_sq <= m_config.lod1_dist_sq) ? 0u : 1u;

        out_list.opaque.push_back(ro);
    }
}

} // namespace action
