"""
ActionEngine Blender Addon
==========================
Plug-and-play Blender → ActionEngine asset pipeline.

Install: Edit > Preferences > Add-ons > Install from File → select this folder as a zip.

Division of responsibility:
  - Blender (this addon): export glTF + embed per-asset metadata as glTF extras,
    generate .aeimport sidecar JSON alongside the exported file.
  - Engine (C++):  read .aeimport, override ImportSettings, drive AssetInspector panel,
    feed skeleton/animation data into the Animation system.

Usage:
  1. Select one or more mesh objects.
  2. Open Properties → Object (bone icon) → ActionEngine panel.
  3. Set per-object overrides (collision, physics layer, LOD bias, shadows).
  4. Open Properties → Scene → ActionEngine Export to set scene-wide settings.
  5. Click "Export to ActionEngine" or use File > Export > ActionEngine (.glb + .aeimport).
"""

bl_info = {
    "name": "ActionEngine Exporter",
    "author": "ActionEngine",
    "version": (1, 0, 0),
    "blender": (3, 6, 0),
    "location": "File > Export > ActionEngine (.glb) / Properties > Object > ActionEngine",
    "description": "Export assets to ActionEngine with .aeimport sidecar generation",
    "category": "Import-Export",
}

import bpy

# Registry of all classes to register / unregister
from .ae_exporter import (
    AE_OT_ExportToEngine,
    AE_PT_ObjectPanel,
    AE_PT_ScenePanel,
    AEObjectProperties,
    AESceneProperties,
    AE_FH_ExportGLB,
)

classes = (
    AEObjectProperties,
    AESceneProperties,
    AE_OT_ExportToEngine,
    AE_PT_ObjectPanel,
    AE_PT_ScenePanel,
    AE_FH_ExportGLB,
)


def menu_func_export(self, context):
    self.layout.operator(AE_OT_ExportToEngine.bl_idname,
                         text="ActionEngine (.glb + .aeimport)")


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Object.ae_props = bpy.props.PointerProperty(type=AEObjectProperties)
    bpy.types.Scene.ae_props  = bpy.props.PointerProperty(type=AESceneProperties)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    del bpy.types.Object.ae_props
    del bpy.types.Scene.ae_props

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
