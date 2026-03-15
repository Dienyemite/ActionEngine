"""
ae_exporter.py
==============
Core export logic for the ActionEngine Blender addon.

Responsibilities (Blender side of the division):
  1. Collect per-object ActionEngine properties (AEObjectProperties).
  2. Export the selection (or all mesh objects) as a .glb file using Blender's
     built-in glTF exporter, injecting per-mesh extras so the engine can read them.
  3. Write a .aeimport sidecar JSON file next to the .glb that encodes all
     import overrides the engine needs (scale, up-axis, LOD settings, animation
     flag, per-mesh collision/physics overrides).

What the engine handles:
  - Reading the .aeimport sidecar and passing its values to ImportSettings.
  - LOD generation, collision shape baking, physics body creation.
  - Animation playback via the AnimationSystem.
"""

from __future__ import annotations

import bpy
import json
import os
import datetime
from bpy_extras.io_utils import ExportHelper
from bpy.props import (
    StringProperty,
    FloatProperty,
    IntProperty,
    BoolProperty,
    EnumProperty,
)
from bpy.types import PropertyGroup, Operator, Panel


# ---------------------------------------------------------------------------
# Per-object properties
# ---------------------------------------------------------------------------

COLLISION_ITEMS = [
    ("none",         "None",         "No collision shape"),
    ("convex_hull",  "Convex Hull",  "Convex hull (fast, good for non-concave objects)"),
    ("trimesh",      "Trimesh",      "Exact triangle mesh (static only)"),
    ("capsule",      "Capsule",      "Capsule (ideal for characters)"),
    ("box",          "Box",          "Axis-aligned bounding box"),
    ("sphere",       "Sphere",       "Bounding sphere"),
]


class AEObjectProperties(PropertyGroup):
    collision_type: EnumProperty(
        name="Collision",
        description="Collision shape type for this mesh",
        items=COLLISION_ITEMS,
        default="convex_hull",
    )
    physics_layer: IntProperty(
        name="Physics Layer",
        description="Bitmask layer index (0-31)",
        default=0,
        min=0,
        max=31,
    )
    cast_shadows: BoolProperty(
        name="Cast Shadows",
        description="Whether this mesh casts shadows",
        default=True,
    )
    lod_bias: FloatProperty(
        name="LOD Bias",
        description="Multiplier for LOD transition distances (> 1 = use higher LOD further away)",
        default=1.0,
        min=0.1,
        max=4.0,
    )
    is_static: BoolProperty(
        name="Static",
        description="Mark as static geometry (enables static batching / static collision)",
        default=False,
    )


# ---------------------------------------------------------------------------
# Scene-wide export settings
# ---------------------------------------------------------------------------

class AESceneProperties(PropertyGroup):
    export_scale: FloatProperty(
        name="Scale",
        description="Global scale applied at import time (1.0 = Blender units = metres)",
        default=1.0,
        min=0.001,
        max=100.0,
    )
    flip_uvs: BoolProperty(
        name="Flip UVs",
        description="Flip V coordinate (usually needed when coming from DCC tools)",
        default=True,
    )
    generate_normals: BoolProperty(
        name="Generate Normals",
        description="Regenerate smooth normals on import if missing",
        default=True,
    )
    import_animations: BoolProperty(
        name="Import Animations",
        description="Extract and import skeletal animations",
        default=True,
    )
    generate_lod: BoolProperty(
        name="Generate LOD",
        description="Auto-generate LOD levels from the base mesh",
        default=True,
    )
    lod_level_count: IntProperty(
        name="LOD Levels",
        description="Number of LOD levels to generate (including LOD0)",
        default=4,
        min=1,
        max=5,
    )
    lod_distance_0: FloatProperty(name="LOD 0 Distance", default=10.0,  min=1.0)
    lod_distance_1: FloatProperty(name="LOD 1 Distance", default=30.0,  min=1.0)
    lod_distance_2: FloatProperty(name="LOD 2 Distance", default=60.0,  min=1.0)
    lod_distance_3: FloatProperty(name="LOD 3 Distance", default=100.0, min=1.0)
    optimize_meshes: BoolProperty(
        name="Optimize Meshes",
        description="Run mesh optimisation (join identical vertices, remove redundant materials)",
        default=False,
    )


# ---------------------------------------------------------------------------
# Object panel (Properties > Object > ActionEngine)
# ---------------------------------------------------------------------------

class AE_PT_ObjectPanel(Panel):
    bl_label = "ActionEngine"
    bl_idname = "OBJECT_PT_ae_properties"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"
    bl_options = {"DEFAULT_CLOSED"}

    @classmethod
    def poll(cls, context):
        return context.object is not None and context.object.type == "MESH"

    def draw(self, context):
        layout = self.layout
        props = context.object.ae_props

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(align=True)
        col.prop(props, "collision_type")
        col.prop(props, "physics_layer")
        col.separator()
        col.prop(props, "cast_shadows")
        col.prop(props, "lod_bias")
        col.prop(props, "is_static")

        layout.separator()
        layout.operator(AE_OT_ExportToEngine.bl_idname, icon="EXPORT")


# ---------------------------------------------------------------------------
# Scene panel (Properties > Scene > ActionEngine Export)
# ---------------------------------------------------------------------------

class AE_PT_ScenePanel(Panel):
    bl_label = "ActionEngine Export"
    bl_idname = "SCENE_PT_ae_export"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "scene"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        props = context.scene.ae_props

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(align=True)
        col.prop(props, "export_scale")
        col.prop(props, "flip_uvs")
        col.prop(props, "generate_normals")
        col.prop(props, "import_animations")
        col.separator()
        col.prop(props, "generate_lod")
        if props.generate_lod:
            col.prop(props, "lod_level_count")
            box = col.box()
            box.label(text="LOD Transition Distances (m):")
            n = props.lod_level_count
            if n >= 2: box.prop(props, "lod_distance_0")
            if n >= 3: box.prop(props, "lod_distance_1")
            if n >= 4: box.prop(props, "lod_distance_2")
            if n >= 5: box.prop(props, "lod_distance_3")
        col.separator()
        col.prop(props, "optimize_meshes")

        layout.separator()
        layout.operator(AE_OT_ExportToEngine.bl_idname, icon="EXPORT")


# ---------------------------------------------------------------------------
# Export operator
# ---------------------------------------------------------------------------

class AE_OT_ExportToEngine(Operator, ExportHelper):
    """Export selected objects (or all mesh objects) to ActionEngine.
    Produces a .glb + .aeimport sidecar pair."""

    bl_idname = "ae.export_to_engine"
    bl_label = "Export to ActionEngine"
    bl_options = {"REGISTER", "UNDO"}

    # ExportHelper mixin sets up filepath, check_existing, etc.
    filename_ext = ".glb"
    filter_glob: StringProperty(default="*.glb", options={"HIDDEN"})

    def execute(self, context):
        filepath = self.filepath
        if not filepath.lower().endswith(".glb"):
            filepath += ".glb"

        scene_props = context.scene.ae_props

        # ------------------------------------------------------------------
        # Step 1: Collect per-object ActionEngine extras to inject into glTF.
        # We temporarily write them to object custom properties so that the
        # glTF exporter picks them up under the "extras" key.
        # ------------------------------------------------------------------
        original_extras: dict[str, dict] = {}
        mesh_objects = [
            o for o in bpy.data.objects
            if o.type == "MESH" and (o.select_get() or not self._only_selected())
        ]

        for obj in mesh_objects:
            # Back up any existing custom props
            original_extras[obj.name] = {}
            props = obj.ae_props
            # These become the mesh's glTF extras.mesh field
            obj["ae_collision"]   = props.collision_type
            obj["ae_phys_layer"]  = props.physics_layer
            obj["ae_shadows"]     = int(props.cast_shadows)
            obj["ae_lod_bias"]    = props.lod_bias
            obj["ae_static"]      = int(props.is_static)

        # ------------------------------------------------------------------
        # Step 2: Export glTF (.glb)
        # ------------------------------------------------------------------
        try:
            bpy.ops.export_scene.gltf(
                filepath=filepath,
                export_format="GLB",
                export_extras=True,           # Include custom properties as extras
                export_animations=scene_props.import_animations,
                export_skins=scene_props.import_animations,
                export_morph=False,
                use_selection=False,
                export_apply=True,            # Apply modifiers
                export_yup=True,              # glTF convention: Y-up
                export_materials="EXPORT",
                export_texcoords=True,
                export_normals=True,
                export_tangents=True,
            )
        except Exception as e:
            self.report({"ERROR"}, f"glTF export failed: {e}")
            return {"CANCELLED"}
        finally:
            # Clean up temp custom props
            for obj in mesh_objects:
                for key in ("ae_collision","ae_phys_layer","ae_shadows","ae_lod_bias","ae_static"):
                    if key in obj:
                        del obj[key]

        # ------------------------------------------------------------------
        # Step 3: Write .aeimport sidecar JSON
        # ------------------------------------------------------------------
        lod_distances = [
            scene_props.lod_distance_0,
            scene_props.lod_distance_1,
            scene_props.lod_distance_2,
            scene_props.lod_distance_3,
        ][:scene_props.lod_level_count - 1]  # One distance per LOD transition

        mesh_overrides = []
        for obj in mesh_objects:
            p = obj.ae_props
            mesh_overrides.append({
                "name":           obj.name,
                "collision":      p.collision_type,
                "physics_layer":  p.physics_layer,
                "cast_shadows":   p.cast_shadows,
                "lod_bias":       round(p.lod_bias, 4),
                "static":         p.is_static,
            })

        # Determine animation names if any
        animation_names = []
        if scene_props.import_animations:
            for action in bpy.data.actions:
                animation_names.append(action.name)

        sidecar = {
            "ae_version":      1,
            "source_file":     os.path.basename(filepath),
            "blender_version": ".".join(str(v) for v in bpy.app.version),
            "export_timestamp": datetime.datetime.utcnow().isoformat() + "Z",
            "import_settings": {
                "scale":              round(scene_props.export_scale, 6),
                "flip_uvs":           scene_props.flip_uvs,
                "generate_normals":   scene_props.generate_normals,
                "optimize_meshes":    scene_props.optimize_meshes,
                "up_axis":            "Y",
                "import_animations":  scene_props.import_animations,
            },
            "lod": {
                "generate":     scene_props.generate_lod,
                "level_count":  scene_props.lod_level_count,
                "distances":    lod_distances,
            },
            "mesh_overrides":   mesh_overrides,
            "animation_names":  animation_names,
        }

        sidecar_path = os.path.splitext(filepath)[0] + ".aeimport"
        with open(sidecar_path, "w", encoding="utf-8") as f:
            json.dump(sidecar, f, indent=2)

        self.report(
            {"INFO"},
            f"Exported: {os.path.basename(filepath)}  +  {os.path.basename(sidecar_path)}"
        )
        return {"FINISHED"}

    def _only_selected(self) -> bool:
        return False  # Export all mesh objects; can be wired to a UI toggle later

    def invoke(self, context, event):
        # Pre-fill filepath from scene name
        if not self.filepath:
            name = bpy.path.clean_name(context.scene.name)
            self.filepath = os.path.join(bpy.path.abspath("//"), name + ".glb")
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


# ---------------------------------------------------------------------------
# File handler (drag-and-drop .glb import into ActionEngine project browser)
# Blender 4.1+ supports TOPBAR_MT_file_import file handlers; we provide a
# stub for older Blender compatibility.
# ---------------------------------------------------------------------------

class AE_FH_ExportGLB(bpy.types.FileHandler):
    bl_label               = "GLB (ActionEngine)"
    bl_import_operator     = "ae.export_to_engine"
    bl_file_extensions     = ".glb"

    @classmethod
    def poll_drop(cls, context):
        return context.area and context.area.type == "VIEW_3D"
