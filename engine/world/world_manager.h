#pragma once

#include "core/types.h"
#include "gameplay/ecs/ecs.h"
#include "render/renderer.h"
#include "render/culling/frustum_culling.h"
#include <unordered_map>
#include <vector>

namespace action {

/*
 * World Manager - Seamless Streaming System
 * 
 * Key features for action-adventure:
 * - Chunk-based world partitioning (256m chunks)
 * - Predictive loading based on player velocity
 * - Streaming rings (hot/warm/cold zones)
 * - No loading screens
 * 
 * Optimized for GTX 660:
 * - 2MB/frame streaming budget
 * - Aggressive LOD at distance
 * - Memory-conscious chunk loading
 */

struct WorldManagerConfig {
    float chunk_size = 256.0f;           // Meters per chunk
    float hot_zone_radius = 100.0f;      // Fully loaded, high LOD
    float warm_zone_radius = 500.0f;     // Loaded, medium LOD
    float cold_zone_radius = 2000.0f;    // Low LOD, streaming
    float lod_bias = 1.0f;
    float draw_distance = 400.0f;
};

// Chunk coordinate
struct ChunkCoord {
    i32 x, z;
    
    bool operator==(const ChunkCoord& o) const { return x == o.x && z == o.z; }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        return std::hash<i64>{}((static_cast<i64>(c.x) << 32) | static_cast<u32>(c.z));
    }
};

// Streaming priority for assets
enum class StreamPriority : u8 {
    Critical = 0,   // On-screen, close
    High = 1,       // On-screen, medium distance
    Normal = 2,     // Off-screen but nearby
    Low = 3,        // Predictive loading
    Background = 4  // Pre-fetching
};

// Chunk state
enum class ChunkState : u8 {
    Unloaded = 0,
    Loading = 1,
    Loaded = 2,
    Active = 3,     // In hot zone
    Unloading = 4
};

// World object (entity in the world)
struct WorldObject {
    Entity entity = INVALID_ENTITY;
    vec3 position;
    AABB bounds;
    MeshHandle mesh;
    MaterialHandle material;
    vec4 color{0.8f, 0.8f, 0.8f, 1.0f};  // Object color (default light gray)
    u8 lod_level;
    bool visible;
};

// Chunk data
struct Chunk {
    ChunkCoord coord;
    ChunkState state = ChunkState::Unloaded;
    AABB bounds;
    
    std::vector<WorldObject> objects;
    std::vector<Entity> entities;
    
    // Streaming info
    StreamPriority priority = StreamPriority::Background;
    float last_access_time = 0;
    size_t memory_usage = 0;
};

class WorldManager {
public:
    WorldManager() = default;
    ~WorldManager() = default;
    
    bool Initialize(const WorldManagerConfig& config);
    void Shutdown();
    
    // Update streaming based on player position/velocity
    void Update(const vec3& player_pos, const vec3& player_velocity, float dt);
    
    // Gather visible objects for rendering
    void GatherVisibleObjects(const Camera& camera, RenderList& out_list);
    
    // Chunk management
    Chunk* GetChunk(ChunkCoord coord);
    Chunk* LoadChunk(ChunkCoord coord);
    void UnloadChunk(ChunkCoord coord);
    
    // Add object to world (finds appropriate chunk)
    void AddObject(const WorldObject& object);
    void RemoveObject(Entity entity);
    void UpdateObject(Entity entity, const vec3& position, const vec4& color);
    
    // Clear all objects from all chunks
    void Clear();
    
    // Query
    std::vector<Entity> QuerySphere(const vec3& center, float radius);
    std::vector<Entity> QueryAABB(const AABB& bounds);
    
    // Stats
    u32 GetLoadedChunkCount() const { return static_cast<u32>(m_chunks.size()); }
    size_t GetMemoryUsage() const { return m_memory_usage; }
    
    // Set ECS reference for transform queries
    void SetECS(ECS* ecs) { m_ecs = ecs; }
    
private:
    // Chunk coordinate from world position
    ChunkCoord WorldToChunk(const vec3& pos) const;
    vec3 ChunkToWorld(ChunkCoord coord) const;
    
    // Streaming
    void UpdateStreamingPriorities(const vec3& player_pos, const vec3& player_velocity);
    void ProcessStreamingQueue();
    float CalculateStreamPriority(const ChunkCoord& coord, 
                                   const vec3& player_pos,
                                   const vec3& player_velocity);
    
    // Zone classification
    enum class Zone { Hot, Warm, Cold, Outside };
    Zone GetZone(const vec3& pos, const vec3& player_pos) const;
    
    WorldManagerConfig m_config;
    
    // Loaded chunks
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> m_chunks;
    
    // Entity-to-Chunk index for O(1) lookup (instead of O(chunks * objects))
    std::unordered_map<Entity, ChunkCoord> m_entity_to_chunk;
    
    // Streaming queue (sorted by priority)
    std::vector<ChunkCoord> m_load_queue;
    std::vector<ChunkCoord> m_unload_queue;
    
    // Current player state
    vec3 m_player_pos;
    vec3 m_player_velocity;
    
    // Culling
    FrustumCuller m_culler;
    
    // ECS reference for transform queries
    ECS* m_ecs = nullptr;
    
    // Memory tracking
    size_t m_memory_usage = 0;
    size_t m_memory_budget = 800_MB;  // For streaming pool
    
    float m_time = 0;
};

} // namespace action
