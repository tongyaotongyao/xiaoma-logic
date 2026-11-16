"""Blender background job: make a board-sized static character copy.

Usage: blender -b -t 2 --python optimize_character.py -- INPUT.glb OUTPUT.glb
Original assets are read only. This script writes only OUTPUT.glb.
"""

import sys

import bpy
from mathutils import Vector


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    if len(args) < 2:
        raise SystemExit("Input and output paths required")
    source, target = args[:2]

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=source)
    objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError("No mesh in source GLB")
    for obj in objects:
        faces_before = len(obj.data.polygons)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        if faces_before > 30000:
            modifier = obj.modifiers.new("BoardSizeDecimation", "DECIMATE")
            modifier.ratio = 30000 / faces_before
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        print(f"MESH {obj.name}: {faces_before} -> {len(obj.data.polygons)} triangles")

        # The original model's geometry spans negative Z. Move the copied mesh
        # so its feet rest at Z=0 when Unreal places it on a board cell.
        minimum_z = min((obj.matrix_world @ Vector(corner)).z for corner in obj.bound_box)
        obj.location.z -= minimum_z
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    for image in bpy.data.images:
        if image.source == "FILE" and max(image.size) > 1024:
            image.scale(1024, 1024)
            image.pack()

    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.gltf(filepath=target, export_format="GLB",
                              use_selection=True, export_apply=True)
    print(f"WROTE {target}")


if __name__ == "__main__":
    main()
