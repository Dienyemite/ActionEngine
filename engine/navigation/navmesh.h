#pragma once
#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include <vector>
#include <functional>

namespace action {

struct NavNode {
    vec3  position = {0,0,0};
    float cost     = 1.0f;
    bool  walkable = true;
};

struct NavPath {
    std::vector<vec3> waypoints;
    float total_length = 0.0f;
    bool  valid        = false;
};

// Grid-based navmesh with A* pathfinding.
class NavMesh {
public:
    // height_fn(x, z) → world-space Y at that point
    bool Build(float min_x, float min_z, float max_x, float max_z,
               float cell_size, float max_slope_deg,
               std::function<float(float, float)> height_fn);

    NavPath FindPath(const vec3& from, const vec3& to) const;
    bool    IsWalkable(const vec3& pos) const;
    vec3    SnapToNavMesh(const vec3& pos) const;

    bool IsBuilt() const { return m_grid_w > 0; }

private:
    NavPath AStar(u32 start_idx, u32 end_idx) const;

    u32 WorldToIndex(const vec3& pos) const;
    vec3 IndexToWorld(u32 idx)        const;

    std::vector<NavNode>         m_nodes;       // m_grid_w * m_grid_h
    std::vector<std::vector<u32>> m_adj;         // per-node adjacency list
    u32   m_grid_w    = 0;
    u32   m_grid_h    = 0;
    float m_cell_size = 1.0f;
    float m_min_x     = 0.0f;
    float m_min_z     = 0.0f;
};

// -----------------------------------------------------------------------

struct NavAgentComponent {
    float speed             = 5.0f;
    float turn_speed        = 3.0f;      // rad/s
    float arrival_radius    = 0.5f;
    float path_recalc_period = 2.0f;     // seconds between forced recalcs

    vec3  destination       = {0,0,0};
    bool  has_destination   = false;
    bool  arrived           = false;

    NavPath current_path;
    u32     current_waypoint = 0;
    float   recalc_timer     = 0.0f;
};

class NavMeshSystem : public System {
public:
    NavMeshSystem(ECS* ecs, NavMesh* navmesh);

    void SetDestination(Entity entity, const vec3& target);
    void Stop(Entity entity);

    void Update(float dt) override;

private:
    void SteerAgent(Entity entity, NavAgentComponent& agent, float dt);

    ECS*    m_ecs     = nullptr;
    NavMesh* m_navmesh = nullptr;
};

} // namespace action
