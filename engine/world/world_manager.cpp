#include "world_manager.h"
#include "core/logging.h"
#include "core/profiler.h"
#include <algorithm>

namespace action {

bool WorldManager::Initialize(const WorldManagerConfig& config) {
    m_config = config;
    
    LOG_INFO("WorldManager initialized");
    LOG_INFO("  Chunk size: {}m", config.chunk_size);
    LOG_INFO("  Hot zone: {}m, Warm zone: {}m, Cold zone: {}m",
             config.hot_zone_radius, config.warm_zone_radius, config.cold_zone_radius);
    LOG_INFO("  Draw distance: {}m", config.draw_distance);
    
    return true;
}

void WorldManager::Shutdown() {
    m_chunks.clear();
    m_load_queue.clear();
    m_unload_queue.clear();
    
    LOG_INFO("WorldManager shutdown");
}

void WorldManager::Update(const vec3& player_pos, const vec3& player_velocity, float dt) {
    PROFILE_SCOPE("WorldManager::Update");
    
    m_player_pos = player_pos;
    m_player_velocity = player_velocity;
    m_time += dt;
    
    // Update streaming priorities
    UpdateStreamingPriorities(player_pos, player_velocity);
    
    // Process streaming queue
    ProcessStreamingQueue();
}

void WorldManager::GatherVisibleObjects(const Camera& camera, RenderList& out_list) {
    PROFILE_SCOPE("WorldManager::GatherVisibleObjects");
    
    out_list.Clear();
    
    // Set up frustum culler
    m_culler.SetFrustum(camera.GetFrustum(), camera.position);
    
    // Iterate loaded chunks
    for (auto& [coord, chunk] : m_chunks) {
        if (chunk.state != ChunkState::Loaded && chunk.state != ChunkState::Active) {
            continue;
        }
        
        // Quick chunk bounds check
        if (!m_culler.IsVisible(chunk.bounds)) {
            continue;
        }
        
        // Check individual objects
        for (auto& obj : chunk.objects) {
            // Distance check
            float dist_sq = distance_sq(obj.position, camera.position);
            if (dist_sq > m_config.draw_distance * m_config.draw_distance) {
                continue;
            }
            
            // Frustum check
            if (!m_culler.IsVisible(obj.bounds)) {
                continue;
            }
            
            // Add to render list
            RenderObject render_obj;
            render_obj.mesh = obj.mesh;
            render_obj.material = obj.material;
            render_obj.color = obj.color;
            
            // Use full transform from ECS if available
            if (obj.entity != INVALID_ENTITY && m_ecs && m_ecs->HasComponent<TransformComponent>(obj.entity)) {
                auto* transform = m_ecs->GetComponent<TransformComponent>(obj.entity);
                render_obj.transform = transform->GetMatrix();
            } else {
                render_obj.transform = mat4::translate(obj.position);
            }
            
            render_obj.bounds = obj.bounds;
            render_obj.distance_sq = dist_sq;
            render_obj.lod_level = obj.lod_level;
            
            out_list.opaque.push_back(render_obj);
            out_list.total_draw_calls++;
            // Note: triangle count would be added based on mesh data
        }
    }
    
    // Sort opaque by distance (front-to-back for early-z)
    std::sort(out_list.opaque.begin(), out_list.opaque.end(),
              [](const RenderObject& a, const RenderObject& b) {
                  return a.distance_sq < b.distance_sq;
              });
}

Chunk* WorldManager::GetChunk(ChunkCoord coord) {
    auto it = m_chunks.find(coord);
    if (it != m_chunks.end()) {
        return &it->second;
    }
    return nullptr;
}

Chunk* WorldManager::LoadChunk(ChunkCoord coord) {
    // Check if already loaded
    if (auto* existing = GetChunk(coord)) {
        return existing;
    }
    
    // Create new chunk
    Chunk chunk;
    chunk.coord = coord;
    chunk.state = ChunkState::Loading;
    
    // Calculate bounds
    vec3 min_pos = ChunkToWorld(coord);
    vec3 max_pos = min_pos + vec3(m_config.chunk_size, 1000.0f, m_config.chunk_size);
    chunk.bounds = AABB(min_pos, max_pos);
    
    // TODO: Load chunk data from disk asynchronously
    // For now, mark as loaded immediately
    chunk.state = ChunkState::Loaded;
    chunk.last_access_time = m_time;
    
    auto [it, inserted] = m_chunks.emplace(coord, std::move(chunk));
    
    LOG_DEBUG("Loaded chunk ({}, {})", coord.x, coord.z);
    
    return &it->second;
}

void WorldManager::UnloadChunk(ChunkCoord coord) {
    auto it = m_chunks.find(coord);
    if (it == m_chunks.end()) return;
    
    Chunk& chunk = it->second;
    
    // TODO: Save modified chunk data
    
    m_memory_usage -= chunk.memory_usage;
    m_chunks.erase(it);
    
    LOG_DEBUG("Unloaded chunk ({}, {})", coord.x, coord.z);
}

void WorldManager::AddObject(const WorldObject& object) {
    ChunkCoord coord = WorldToChunk(object.position);
    
    Chunk* chunk = GetChunk(coord);
    if (!chunk) {
        chunk = LoadChunk(coord);
    }
    
    chunk->objects.push_back(object);
    chunk->entities.push_back(object.entity);
}

void WorldManager::RemoveObject(Entity entity) {
    for (auto& [coord, chunk] : m_chunks) {
        auto obj_it = std::find_if(chunk.objects.begin(), chunk.objects.end(),
                                    [entity](const WorldObject& o) { 
                                        return o.entity == entity; 
                                    });
        
        if (obj_it != chunk.objects.end()) {
            chunk.objects.erase(obj_it);
            
            auto ent_it = std::find(chunk.entities.begin(), chunk.entities.end(), entity);
            if (ent_it != chunk.entities.end()) {
                chunk.entities.erase(ent_it);
            }
            return;
        }
    }
}

void WorldManager::UpdateObject(Entity entity, const vec3& position, const vec4& color) {
    for (auto& [coord, chunk] : m_chunks) {
        for (auto& obj : chunk.objects) {
            if (obj.entity == entity) {
                obj.position = position;
                obj.color = color;
                // Update bounds based on new position
                vec3 half_size = obj.bounds.extents();
                obj.bounds = AABB(position - half_size, position + half_size);
                return;
            }
        }
    }
}

std::vector<Entity> WorldManager::QuerySphere(const vec3& center, float radius) {
    std::vector<Entity> result;
    
    float radius_sq = radius * radius;
    
    // Determine which chunks to check
    ChunkCoord min_chunk = WorldToChunk(center - vec3(radius, 0, radius));
    ChunkCoord max_chunk = WorldToChunk(center + vec3(radius, 0, radius));
    
    for (i32 x = min_chunk.x; x <= max_chunk.x; ++x) {
        for (i32 z = min_chunk.z; z <= max_chunk.z; ++z) {
            Chunk* chunk = GetChunk({x, z});
            if (!chunk) continue;
            
            for (const auto& obj : chunk->objects) {
                if (distance_sq(obj.position, center) <= radius_sq) {
                    result.push_back(obj.entity);
                }
            }
        }
    }
    
    return result;
}

std::vector<Entity> WorldManager::QueryAABB(const AABB& bounds) {
    std::vector<Entity> result;
    
    ChunkCoord min_chunk = WorldToChunk(bounds.min);
    ChunkCoord max_chunk = WorldToChunk(bounds.max);
    
    for (i32 x = min_chunk.x; x <= max_chunk.x; ++x) {
        for (i32 z = min_chunk.z; z <= max_chunk.z; ++z) {
            Chunk* chunk = GetChunk({x, z});
            if (!chunk) continue;
            
            for (const auto& obj : chunk->objects) {
                if (bounds.intersects(obj.bounds)) {
                    result.push_back(obj.entity);
                }
            }
        }
    }
    
    return result;
}

ChunkCoord WorldManager::WorldToChunk(const vec3& pos) const {
    return {
        static_cast<i32>(std::floor(pos.x / m_config.chunk_size)),
        static_cast<i32>(std::floor(pos.z / m_config.chunk_size))
    };
}

vec3 WorldManager::ChunkToWorld(ChunkCoord coord) const {
    return {
        coord.x * m_config.chunk_size,
        0.0f,
        coord.z * m_config.chunk_size
    };
}

void WorldManager::UpdateStreamingPriorities(const vec3& player_pos, const vec3& player_velocity) {
    PROFILE_SCOPE("UpdateStreamingPriorities");
    
    // Calculate which chunks should be loaded
    ChunkCoord player_chunk = WorldToChunk(player_pos);
    
    // Prediction based on velocity
    float prediction_time = 2.0f;  // Look ahead 2 seconds
    vec3 predicted_pos = player_pos + player_velocity * prediction_time;
    ChunkCoord predicted_chunk = WorldToChunk(predicted_pos);
    
    // Calculate loading radius in chunks
    i32 cold_radius = static_cast<i32>(std::ceil(m_config.cold_zone_radius / m_config.chunk_size));
    
    // Clear queues
    m_load_queue.clear();
    m_unload_queue.clear();
    
    // Find chunks to load (around player and prediction)
    for (i32 dx = -cold_radius; dx <= cold_radius; ++dx) {
        for (i32 dz = -cold_radius; dz <= cold_radius; ++dz) {
            ChunkCoord coord = {player_chunk.x + dx, player_chunk.z + dz};
            
            // Check if within streaming distance
            vec3 chunk_center = ChunkToWorld(coord) + vec3(m_config.chunk_size * 0.5f, 0, m_config.chunk_size * 0.5f);
            float dist = length(chunk_center - player_pos);
            
            if (dist <= m_config.cold_zone_radius) {
                if (!GetChunk(coord)) {
                    m_load_queue.push_back(coord);
                }
            }
        }
    }
    
    // Find chunks to unload (too far from player)
    for (auto& [coord, chunk] : m_chunks) {
        vec3 chunk_center = ChunkToWorld(coord) + vec3(m_config.chunk_size * 0.5f, 0, m_config.chunk_size * 0.5f);
        float dist = length(chunk_center - player_pos);
        
        // Add hysteresis to prevent thrashing
        if (dist > m_config.cold_zone_radius * 1.2f) {
            m_unload_queue.push_back(coord);
        }
        
        // Update chunk state based on zone
        Zone zone = GetZone(chunk_center, player_pos);
        switch (zone) {
            case Zone::Hot:
                chunk.state = ChunkState::Active;
                chunk.priority = StreamPriority::Critical;
                break;
            case Zone::Warm:
                chunk.state = ChunkState::Loaded;
                chunk.priority = StreamPriority::High;
                break;
            case Zone::Cold:
                chunk.state = ChunkState::Loaded;
                chunk.priority = StreamPriority::Normal;
                break;
            default:
                chunk.priority = StreamPriority::Background;
                break;
        }
        
        chunk.last_access_time = m_time;
    }
    
    // Sort load queue by priority (closest first)
    std::sort(m_load_queue.begin(), m_load_queue.end(),
              [this, &player_pos, &player_velocity](const ChunkCoord& a, const ChunkCoord& b) {
                  return CalculateStreamPriority(a, player_pos, player_velocity) >
                         CalculateStreamPriority(b, player_pos, player_velocity);
              });
}

void WorldManager::ProcessStreamingQueue() {
    PROFILE_SCOPE("ProcessStreamingQueue");
    
    // Load high-priority chunks (limit per frame)
    constexpr u32 MAX_LOADS_PER_FRAME = 2;
    u32 loads = 0;
    
    while (!m_load_queue.empty() && loads < MAX_LOADS_PER_FRAME) {
        ChunkCoord coord = m_load_queue.back();
        m_load_queue.pop_back();
        
        LoadChunk(coord);
        loads++;
    }
    
    // Unload low-priority chunks (limit per frame)
    constexpr u32 MAX_UNLOADS_PER_FRAME = 1;
    u32 unloads = 0;
    
    while (!m_unload_queue.empty() && unloads < MAX_UNLOADS_PER_FRAME) {
        ChunkCoord coord = m_unload_queue.back();
        m_unload_queue.pop_back();
        
        UnloadChunk(coord);
        unloads++;
    }
}

float WorldManager::CalculateStreamPriority(const ChunkCoord& coord,
                                             const vec3& player_pos,
                                             const vec3& player_velocity) {
    vec3 chunk_center = ChunkToWorld(coord) + vec3(m_config.chunk_size * 0.5f, 0, m_config.chunk_size * 0.5f);
    
    // Base priority: inverse distance
    float dist = length(chunk_center - player_pos);
    float priority = 1.0f / (dist + 1.0f);
    
    // Boost for chunks in movement direction
    if (length(player_velocity) > 0.1f) {
        vec3 to_chunk = normalize(chunk_center - player_pos);
        vec3 vel_dir = normalize(player_velocity);
        float alignment = std::max(0.0f, dot(to_chunk, vel_dir));
        priority *= (1.0f + alignment * 2.0f);
    }
    
    return priority;
}

WorldManager::Zone WorldManager::GetZone(const vec3& pos, const vec3& player_pos) const {
    float dist = length(pos - player_pos);
    
    if (dist <= m_config.hot_zone_radius) return Zone::Hot;
    if (dist <= m_config.warm_zone_radius) return Zone::Warm;
    if (dist <= m_config.cold_zone_radius) return Zone::Cold;
    return Zone::Outside;
}

} // namespace action
