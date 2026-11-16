"""Render a contact sheet of optimized character models for visual QA."""

import math
from pathlib import Path

import bpy
from mathutils import Vector


root = Path(__file__).resolve().parents[1]
source = root / "Content" / "ImportSource" / "Characters"
output = root / "Previews" / "characters.png"
output.parent.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

names = ["Blue", "Brown", "Pink", "Red", "Yellow", "Mint"]
for index, name in enumerate(names):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(source / f"{name}.glb"))
    imported = [obj for obj in bpy.context.scene.objects if obj not in before and obj.type == "MESH"]
    for obj in imported:
        obj.location.x += (index - 2.5) * 1.25

    bpy.ops.object.text_add(location=((index - 2.5) * 1.25 - 0.3, 0, -0.28),
                            rotation=(math.radians(90), 0, 0))
    label = bpy.context.object
    label.data.body = name
    label.data.size = 0.17
    label.data.extrude = 0.002

world = bpy.context.scene.world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.72, 0.85, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.7

bpy.ops.object.light_add(type="AREA", location=(-2, -4, 5))
bpy.context.object.data.energy = 900
bpy.context.object.data.shape = "DISK"
bpy.context.object.data.size = 5
bpy.ops.object.light_add(type="AREA", location=(4, 2, 3))
bpy.context.object.data.energy = 500
bpy.context.object.data.size = 4

bpy.ops.object.camera_add(location=(0, -10, 2.3))
camera = bpy.context.object
direction = Vector((0, 0, 0.5)) - camera.location
camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 9.5
bpy.context.scene.camera = camera

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1500
scene.render.resolution_y = 480
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(output)
scene.render.film_transparent = False
bpy.ops.render.render(write_still=True)
print(f"WROTE {output}")
