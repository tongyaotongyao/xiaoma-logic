"""Create six small, original low-poly pony pieces for the logic board."""

from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "Content" / "ImportSource" / "Ponies"
OUTPUT.mkdir(parents=True, exist_ok=True)

PALETTE = {
    "Blue": (0.28, 0.60, 0.95, 1),
    "Brown": (0.57, 0.35, 0.22, 1),
    "Pink": (0.98, 0.52, 0.73, 1),
    "Red": (0.88, 0.30, 0.36, 1),
    "Yellow": (0.98, 0.77, 0.27, 1),
    "Mint": (0.35, 0.85, 0.69, 1),
}


def material(name, rgba, roughness=0.7):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = rgba
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = rgba
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


def sphere(name, location, scale, mat, segments=12, rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(mat)
    return obj


def cone(name, location, radius1, radius2, depth, mat, vertices=8):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius1,
                                    radius2=radius2, depth=depth, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    return obj


def build(name, colour):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    coat = material(f"Pony_{name}_Coat", colour)
    pale = material(f"Pony_{name}_Muzzle", tuple(min(1, c * 0.55 + 0.45) for c in colour[:3]) + (1,))
    mane = material(f"Pony_{name}_Mane", tuple(max(0, c * 0.46) for c in colour[:3]) + (1,))
    hoof = material(f"Pony_{name}_Hoof", (0.18, 0.20, 0.24, 1))
    white = material("Pony_EyeWhite", (1, 1, 1, 1))
    dark = material("Pony_Eye", (0.025, 0.03, 0.05, 1), 0.25)

    parts = []
    parts.append(sphere("Body", (0, 0.04, 0.70), (0.36, 0.53, 0.34), coat))
    parts.append(sphere("Neck", (0, -0.36, 0.84), (0.29, 0.28, 0.40), coat))
    parts.append(sphere("Head", (0, -0.54, 1.18), (0.35, 0.32, 0.33), coat))
    parts.append(sphere("Muzzle", (0, -0.82, 1.06), (0.24, 0.19, 0.16), pale))
    for x in (-0.23, 0.23):
        for y in (-0.30, 0.39):
            parts.append(cone("Leg", (x, y, 0.34), 0.13, 0.095, 0.58, coat))
            parts.append(sphere("Hoof", (x, y - 0.025, 0.08), (0.145, 0.16, 0.10), hoof))
    for x in (-0.23, 0.23):
        parts.append(cone("Ear", (x, -0.51, 1.49), 0.12, 0.025, 0.30, coat))
        parts.append(sphere("EyeWhite", (x * 1.14, -0.785, 1.22), (0.08, 0.04, 0.10), white))
        parts.append(sphere("Pupil", (x * 1.15, -0.822, 1.22), (0.045, 0.025, 0.066), dark))
        parts.append(sphere("Sparkle", (x * 1.15 - 0.018, -0.843, 1.25), (0.015, 0.012, 0.018), white))
    for index, y in enumerate((-0.52, -0.33, -0.14)):
        parts.append(sphere("Mane", (0, y, 1.48 - index * 0.025), (0.20, 0.16, 0.13), mane))
    parts.append(sphere("TailBase", (0, 0.56, 0.79), (0.13, 0.22, 0.14), mane))
    parts.append(sphere("TailTip", (0, 0.76, 0.98), (0.16, 0.13, 0.19), mane))

    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.convert(target="MESH")
    bpy.ops.object.join()
    piece = bpy.context.object
    piece.name = f"SM_Pony_{name}"
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    target = OUTPUT / f"Pony_{name}.glb"
    bpy.ops.export_scene.gltf(filepath=str(target), export_format="GLB",
                              use_selection=True, export_apply=True)
    print(f"PONY {name}: {len(piece.data.polygons)} triangles -> {target}")


for name, colour in PALETTE.items():
    build(name, colour)
