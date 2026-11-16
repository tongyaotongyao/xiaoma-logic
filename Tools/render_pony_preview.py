"""Render all six pony pieces for visual QA."""

from pathlib import Path

import bpy
from mathutils import Vector


root = Path(__file__).resolve().parents[1]
source = root / "Content" / "ImportSource" / "Ponies"
output = root / "Previews" / "ponies.png"
output.parent.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for index, name in enumerate(("Blue", "Brown", "Pink", "Red", "Yellow", "Mint")):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(source / f"Pony_{name}.glb"))
    for obj in set(bpy.context.scene.objects) - before:
        if obj.type == "MESH":
            obj.location.x += (index - 2.5) * 1.5

world = bpy.context.scene.world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.58, 0.72, 0.82, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.7
bpy.ops.object.light_add(type="AREA", location=(-2, -4, 5))
bpy.context.object.data.energy = 900
bpy.context.object.data.size = 5
bpy.ops.object.camera_add(location=(0, -12, 3))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 0.75)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 11
bpy.context.scene.camera = camera
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1500
scene.render.resolution_y = 420
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(output)
bpy.ops.render.render(write_still=True)
print(f"WROTE {output}")
