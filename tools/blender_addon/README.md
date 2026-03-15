# ActionEngine Blender Addon — README

## Installation

1. Select `tools/blender_addon/` and zip it to `ActionEngine_BlenderAddon.zip`.
2. In Blender: **Edit → Preferences → Add-ons → Install from File** → choose the zip.
3. Enable **"ActionEngine Exporter"** in the add-on list.

## Usage

### Per-object settings
Select a mesh object → **Properties → Object → ActionEngine** panel:

| Property | What it does in the engine |
|----------|---------------------------|
| Collision | Shape used for Jolt Physics body (convex_hull, trimesh, capsule…) |
| Physics Layer | Bitmask layer index (0-31) for layer-vs-layer filtering |
| Cast Shadows | Whether to include in shadow passes |
| LOD Bias | Multiplier on LOD transition distances |
| Static | Tags mesh as static; enables trimesh collider + static batching |

### Scene-wide settings
**Properties → Scene → ActionEngine Export** panel:

| Property | Effect |
|----------|--------|
| Scale | Global import scale (1.0 = Blender metres = engine metres) |
| Flip UVs | Flip V component (usually needed for DCC tools) |
| Generate Normals | Regenerate normals on import if missing |
| Import Animations | Export skeleton + animation clips; engine loads via AnimationSystem |
| Generate LOD | Ask engine to auto-generate LOD levels |
| LOD Levels | How many LOD levels (1-5) |
| LOD Distances | Screen-space transition distances per level |
| Optimize Meshes | Join identical vertices, remove redundant materials |

### Exporting
**File → Export → ActionEngine (.glb + .aeimport)**

This produces **two files**:
- `hero.glb` — geometry, materials, skeleton, animation clips (standard glTF 2.0)
- `hero.aeimport` — sidecar JSON with engine-specific overrides

The engine's `AssetImporter` reads both: the glTF via Assimp, then the `.aeimport`
sidecar to override `ImportSettings` and per-mesh collision/shadow/LOD settings.

## .aeimport Format

```json
{
  "ae_version": 1,
  "source_file": "hero.glb",
  "blender_version": "3.6.5",
  "export_timestamp": "2026-03-14T12:00:00Z",
  "import_settings": {
    "scale": 1.0,
    "flip_uvs": true,
    "generate_normals": true,
    "optimize_meshes": false,
    "up_axis": "Y",
    "import_animations": true
  },
  "lod": {
    "generate": true,
    "level_count": 4,
    "distances": [10.0, 30.0, 60.0]
  },
  "mesh_overrides": [
    {
      "name": "Body",
      "collision": "capsule",
      "physics_layer": 0,
      "cast_shadows": true,
      "lod_bias": 1.0,
      "static": false
    }
  ],
  "animation_names": ["Idle", "Run", "Jump", "Attack"]
}
```
