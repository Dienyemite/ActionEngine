# ActionEngine

A custom game engine optimized for action-adventure games, targeting seamless open-world gameplay on low-end hardware (GTX 660 + i5-6600K at 1080p 60FPS).

## Features

- **Vulkan 1.0 Renderer** - Forward+ clustered rendering for older GPUs
- **Seamless World Streaming** - No loading screens, predictive asset loading
- **Aggressive LOD System** - 5 LOD levels, distance + screen-size based
- **Data-Oriented ECS** - Cache-friendly entity component system
- **4-Thread Job System** - Optimized for quad-core CPUs
- **Strict Memory Budgets** - Fits in 2GB VRAM

## Target Hardware

| Component | Minimum Spec | Budget |
|-----------|--------------|--------|
| GPU | GTX 660 (2GB VRAM) | 1.6GB usable |
| CPU | i5-6600K (4 cores) | 3 workers + main |
| RAM | 8GB | 2GB engine budget |

## Performance Targets

- **Resolution**: 1920×1080
- **Frame Rate**: 60 FPS (16.67ms)
- **Draw Calls**: < 2500 per frame
- **Triangles**: < 800,000 per frame
- **Streaming**: 2MB upload per frame

## Building

### Prerequisites

- CMake 3.20+
- Vulkan SDK 1.0+
- C++20 compiler (MSVC 2022, GCC 11+, Clang 14+)

### Windows

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Project Structure

```
ActionEngine/
├── engine/
│   ├── core/           # Types, math, memory, jobs
│   ├── platform/       # Window, input, Vulkan
│   ├── render/         # Forward+ renderer, LOD, culling
│   ├── world/          # Streaming, chunks, spatial
│   ├── assets/         # Asset loading, caching
│   ├── physics/        # Collision, raycasting
│   ├── audio/          # Sound (stub)
│   └── gameplay/       # ECS, systems
├── game/               # Game executable
├── shaders/            # GLSL shaders
├── assets/             # Game assets
└── third_party/        # Dependencies
```

## Architecture Overview

### Rendering Pipeline

1. **CPU Culling** - Frustum + distance culling
2. **LOD Selection** - Based on distance and velocity
3. **Light Clustering** - Compute shader (Forward+)
4. **Depth Pre-Pass** - Early-Z optimization
5. **Forward Pass** - Opaque geometry with clustered lights
6. **Transparent Pass** - Alpha-blended objects
7. **Post-Process** - Bloom (optional), tone mapping

### Streaming System

```
Player Position + Velocity
        ↓
Predictive Loading (2s ahead)
        ↓
Priority Queue (distance + direction)
        ↓
Background I/O → Decompress → GPU Upload
        ↓
LRU Cache Eviction (800MB pool)
```

### Streaming Zones

- **Hot Zone** (0-100m): Fully loaded, LOD0-1
- **Warm Zone** (100-500m): Loaded, LOD1-3
- **Cold Zone** (500-2000m): Streaming, LOD3-4
- **Outside**: Unloaded, billboards only

## Quality Scaling

The engine supports runtime quality adjustment:

| Setting | Low | Medium | High |
|---------|-----|--------|------|
| Shadow Cascades | 1 | 2 | 2 |
| Shadow Resolution | 512 | 1024 | 1024 |
| LOD Bias | 2.0 | 1.0 | 0.5 |
| Draw Distance | 200m | 400m | 600m |
| Bloom | Off | On | On |
| Texture Size | 512 | 1024 | 1024 |

## License

MIT License - See LICENSE file
