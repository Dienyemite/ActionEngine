# Models Directory

This directory is watched by the Asset Hot Reloader for automatic import.

## Supported Formats

- **OBJ** (.obj) - Fully supported
- **glTF** (.gltf, .glb) - Planned
- **FBX** (.fbx) - Planned

## Blender Export Workflow

### Export as OBJ

1. In Blender, select your object(s)
2. Go to **File > Export > Wavefront (.obj)**
3. Settings:
   - **Up Axis**: Z Up (default for Blender, converted automatically)
   - **Forward Axis**: -Y Forward
   - **Apply Modifiers**: ✓ Enable this
   - **Export**: Selected Only (or all)
   - **Include**: UVs, Normals
4. Save to this `assets/models/` folder
5. The engine will automatically detect and import the file!

### Hot Reload

When you save changes to a model file:
- The engine automatically detects the change
- Reimports the asset
- Updates in-scene instances (if supported)

### Tips

- For best results, apply all transforms in Blender (Ctrl+A > All Transforms)
- Triangulate faces before export (Ctrl+T in Edit Mode)
- Keep file names simple (no spaces or special characters)
