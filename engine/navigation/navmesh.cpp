#include "navmesh.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>

namespace action {

// ---------------------------------------------------------------------------
// NavMesh
// ---------------------------------------------------------------------------

bool NavMesh::Build(float min_x, float min_z, float max_x, float max_z,
                    float cell_size, float max_slope_deg,
                    std::function<float(float, float)> height_fn) {
    if (cell_size <= 0.0f || max_x <= min_x || max_z <= min_z) return false;

    m_cell_size = cell_size;
    m_min_x     = min_x;
    m_min_z     = min_z;

    m_grid_w = static_cast<u32>(std::ceil((max_x - min_x) / cell_size));
    m_grid_h = static_cast<u32>(std::ceil((max_z - min_z) / cell_size));
    if (m_grid_w == 0 || m_grid_h == 0) return false;

    u32 total = m_grid_w * m_grid_h;
    m_nodes.resize(total);
    m_adj.assign(total, {});

    float max_slope_tan = std::tan(Radians(max_slope_deg));

    // Sample height and check slope-based walkability
    for (u32 gz = 0; gz < m_grid_h; ++gz) {
        for (u32 gx = 0; gx < m_grid_w; ++gx) {
            u32   idx     = gz * m_grid_w + gx;
            float world_x = min_x + (gx + 0.5f) * cell_size;
            float world_z = min_z + (gz + 0.5f) * cell_size;
            float height  = height_fn(world_x, world_z);

            m_nodes[idx].position = {world_x, height, world_z};
            m_nodes[idx].cost     = 1.0f;
            m_nodes[idx].walkable = true;

            // Slope check to +X
            if (gx + 1 < m_grid_w) {
                float hr = height_fn(world_x + cell_size, world_z);
                if (std::fabs(hr - height) / cell_size > max_slope_tan)
                    m_nodes[idx].walkable = false;
            }
            // Slope check to +Z
            if (gz + 1 < m_grid_h && m_nodes[idx].walkable) {
                float hf = height_fn(world_x, world_z + cell_size);
                if (std::fabs(hf - height) / cell_size > max_slope_tan)
                    m_nodes[idx].walkable = false;
            }
        }
    }

    // Build 4-connected adjacency
    const int DX[] = { 1, -1,  0,  0 };
    const int DZ[] = { 0,  0,  1, -1 };
    for (u32 gz = 0; gz < m_grid_h; ++gz) {
        for (u32 gx = 0; gx < m_grid_w; ++gx) {
            u32 idx = gz * m_grid_w + gx;
            if (!m_nodes[idx].walkable) continue;
            for (int d = 0; d < 4; ++d) {
                int nx = static_cast<int>(gx) + DX[d];
                int nz = static_cast<int>(gz) + DZ[d];
                if (nx < 0 || nz < 0) continue;
                if (nx >= static_cast<int>(m_grid_w) || nz >= static_cast<int>(m_grid_h)) continue;
                u32 nidx = static_cast<u32>(nz) * m_grid_w + static_cast<u32>(nx);
                if (m_nodes[nidx].walkable)
                    m_adj[idx].push_back(nidx);
            }
        }
    }

    LOG_INFO("NavMesh built: {}x{} cells ({} total nodes)", m_grid_w, m_grid_h, total);
    return true;
}

u32 NavMesh::WorldToIndex(const vec3& pos) const {
    int gx = static_cast<int>((pos.x - m_min_x) / m_cell_size);
    int gz = static_cast<int>((pos.z - m_min_z) / m_cell_size);
    gx = std::max(0, std::min(gx, static_cast<int>(m_grid_w) - 1));
    gz = std::max(0, std::min(gz, static_cast<int>(m_grid_h) - 1));
    return static_cast<u32>(gz) * m_grid_w + static_cast<u32>(gx);
}

vec3 NavMesh::IndexToWorld(u32 idx) const {
    return m_nodes[idx].position;
}

bool NavMesh::IsWalkable(const vec3& pos) const {
    if (!IsBuilt()) return false;
    return m_nodes[WorldToIndex(pos)].walkable;
}

vec3 NavMesh::SnapToNavMesh(const vec3& pos) const {
    if (!IsBuilt()) return pos;
    return IndexToWorld(WorldToIndex(pos));
}

NavPath NavMesh::AStar(u32 start_idx, u32 end_idx) const {
    NavPath result;
    if (start_idx == end_idx) {
        result.waypoints.push_back(m_nodes[start_idx].position);
        result.valid = true;
        return result;
    }

    using PQItem = std::pair<float, u32>; // (f_cost, node_idx)
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> open;

    std::vector<float> g_cost(m_nodes.size(), std::numeric_limits<float>::infinity());
    std::vector<u32>   parent(m_nodes.size(), UINT32_MAX);
    std::vector<bool>  closed(m_nodes.size(), false);

    auto heuristic = [&](u32 a, u32 b) -> float {
        const vec3& pa = m_nodes[a].position;
        const vec3& pb = m_nodes[b].position;
        float dx = pa.x - pb.x, dy = pa.y - pb.y, dz = pa.z - pb.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    g_cost[start_idx] = 0.0f;
    open.push({heuristic(start_idx, end_idx), start_idx});

    while (!open.empty()) {
        auto [f, cur] = open.top(); open.pop();
        if (closed[cur]) continue;
        closed[cur] = true;

        if (cur == end_idx) {
            // Reconstruct path
            u32 node = end_idx;
            while (node != UINT32_MAX) {
                result.waypoints.push_back(m_nodes[node].position);
                node = parent[node];
            }
            std::reverse(result.waypoints.begin(), result.waypoints.end());
            for (u32 i = 1; i < static_cast<u32>(result.waypoints.size()); ++i) {
                const vec3& a = result.waypoints[i - 1];
                const vec3& b = result.waypoints[i];
                float dx = b.x-a.x, dy = b.y-a.y, dz = b.z-a.z;
                result.total_length += std::sqrt(dx*dx + dy*dy + dz*dz);
            }
            result.valid = true;
            return result;
        }

        for (u32 nb : m_adj[cur]) {
            if (closed[nb]) continue;
            const vec3& cp = m_nodes[cur].position;
            const vec3& np = m_nodes[nb].position;
            float dx = np.x-cp.x, dy = np.y-cp.y, dz = np.z-cp.z;
            float step_cost = std::sqrt(dx*dx + dy*dy + dz*dz) * m_nodes[nb].cost;
            float new_g = g_cost[cur] + step_cost;
            if (new_g < g_cost[nb]) {
                g_cost[nb] = new_g;
                parent[nb] = cur;
                open.push({new_g + heuristic(nb, end_idx), nb});
            }
        }
    }
    return result; // valid = false → no path
}

NavPath NavMesh::FindPath(const vec3& from, const vec3& to) const {
    if (!IsBuilt()) return {};
    u32 start = WorldToIndex(from);
    u32 end   = WorldToIndex(to);
    if (!m_nodes[start].walkable || !m_nodes[end].walkable)
        return {};
    return AStar(start, end);
}

// ---------------------------------------------------------------------------
// NavMeshSystem
// ---------------------------------------------------------------------------

NavMeshSystem::NavMeshSystem(ECS* ecs, NavMesh* navmesh)
    : m_ecs(ecs), m_navmesh(navmesh)
{
    LOG_INFO("NavMeshSystem initialized");
}

void NavMeshSystem::SetDestination(Entity entity, const vec3& target) {
    NavAgentComponent* ag = m_ecs->GetComponent<NavAgentComponent>(entity);
    if (!ag) return;
    ag->destination     = target;
    ag->has_destination = true;
    ag->arrived         = false;
    ag->current_path    = {};
    ag->current_waypoint = 0;
    ag->recalc_timer    = 0.0f; // Force immediate recalc
}

void NavMeshSystem::Stop(Entity entity) {
    NavAgentComponent* ag = m_ecs->GetComponent<NavAgentComponent>(entity);
    if (!ag) return;
    ag->has_destination = false;
    ag->arrived         = false;
    ag->current_path    = {};
}

void NavMeshSystem::SteerAgent(Entity entity, NavAgentComponent& ag, float dt) {
    if (!ag.has_destination || ag.arrived) return;

    auto* tc = m_ecs->GetComponent<TransformComponent>(entity);
    if (!tc) return;

    ag.recalc_timer -= dt;
    bool needs_recalc = ag.recalc_timer <= 0.0f
                     || !ag.current_path.valid
                     || ag.current_waypoint >= static_cast<u32>(ag.current_path.waypoints.size());

    if (needs_recalc) {
        ag.current_path      = m_navmesh->FindPath(tc->position, ag.destination);
        ag.current_waypoint  = 0;
        ag.recalc_timer      = ag.path_recalc_period;
        ag.arrived           = false;
    }

    if (!ag.current_path.valid) return;
    if (ag.current_waypoint >= static_cast<u32>(ag.current_path.waypoints.size())) {
        ag.arrived = true;
        return;
    }

    const vec3& wp = ag.current_path.waypoints[ag.current_waypoint];
    float dx   = wp.x - tc->position.x;
    float dz   = wp.z - tc->position.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    if (dist < ag.arrival_radius) {
        ++ag.current_waypoint;
        if (ag.current_waypoint >= static_cast<u32>(ag.current_path.waypoints.size()))
            ag.arrived = true;
        return;
    }

    float inv_dist = 1.0f / dist;
    float move     = std::min(ag.speed * dt, dist);
    tc->position.x += dx * inv_dist * move;
    tc->position.z += dz * inv_dist * move;
    tc->position.y  = wp.y;  // snap height to navmesh surface
}

void NavMeshSystem::Update(float dt) {
    m_ecs->ForEach<NavAgentComponent>([&](Entity e, NavAgentComponent& ag) {
        SteerAgent(e, ag, dt);
    });
}

} // namespace action
