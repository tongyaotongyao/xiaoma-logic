"""Create a beveled low-poly board tile for the 3D game board."""

from pathlib import Path
import bpy


output = Path(__file__).resolve().parents[1] / "Content" / "ImportSource" / "Board"
output.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_cube_add(size=1)
tile = bpy.context.object
tile.name = "Tile"
bevel = tile.modifiers.new("Soft corners", "BEVEL")
bevel.width = 0.08
bevel.segments = 3
bpy.ops.object.modifier_apply(modifier=bevel.name)
material = bpy.data.materials.new("Tile_White")
material.diffuse_color = (0.9, 0.9, 0.9, 1)
tile.data.materials.append(material)
bpy.ops.export_scene.gltf(filepath=str(output / "Tile.glb"), export_format="GLB",
                          use_selection=True, export_apply=True)
print(f"TILE: {len(tile.data.polygons)} polygons -> {output / 'Tile.glb'}")
