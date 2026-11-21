"""Build the reusable 3D scene in an asset staging UE 5.5 project."""

import unreal


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
levels = unreal.EditorLevelLibrary
base_material = assets.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
cube = assets.load_asset("/Engine/BasicShapes/Cube")
sphere = assets.load_asset("/Engine/BasicShapes/Sphere")
cylinder = assets.load_asset("/Engine/BasicShapes/Cylinder")
assert base_material and cube and sphere and cylinder


def colour_material(name, colour):
    path = f"/Game/Environment/{name}"
    material = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if material is None:
        material = asset_tools.create_asset(
            name, "/Game/Environment", unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    material.set_editor_property("parent", base_material)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material, "Color", unreal.LinearColor(*colour))
    assets.save_loaded_asset(material)
    return material


grass = colour_material("MI_Grass", (0.26, 0.56, 0.46, 1))
cream = colour_material("MI_Cream", (0.92, 0.82, 0.65, 1))
wood = colour_material("MI_Wood", (0.44, 0.29, 0.23, 1))
leaf = colour_material("MI_Leaves", (0.33, 0.68, 0.54, 1))
gold = colour_material("MI_Gold", (0.98, 0.78, 0.43, 1))
pink = colour_material("MI_Pink", (0.95, 0.55, 0.68, 1))

assert levels.new_level("/Game/Maps/Main")


def mesh_actor(name, mesh, location, scale, material=None, rotation=None):
    actor = levels.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location),
        rotation or unreal.Rotator())
    actor.set_actor_label(name)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        component.set_material(0, material)
    return actor


mesh_actor("软草地", cube, (280, 300, -70), (65, 65, 0.5), grass)
mesh_actor("棋盘浅色基座", cube, (280, 280, -29), (7.1, 7.1, 0.22), cream)
mesh_actor("棋盘木质底座", cube, (280, 280, -49), (7.4, 7.4, 0.18), wood)

# 上侧待命区木栅栏与浅色站台（6 匹待命小马由运行时动态管理）
mesh_actor("待命区地台", cube, (280, 815, -36), (6.6, 1.45, 0.16), cream)
for index in range(7):
    post_x = -20 + index * 100
    mesh_actor(f"待命栅栏柱_{index}", cylinder, (post_x, 735, -2), (0.10, 0.10, 0.52), wood)
    mesh_actor(f"待命后柱_{index}", cylinder, (post_x, 895, -2), (0.10, 0.10, 0.52), wood)
mesh_actor("待命前横栏_上", cube, (280, 735, 10), (6.2, 0.08, 0.07), wood)
mesh_actor("待命前横栏_下", cube, (280, 735, -10), (6.2, 0.08, 0.07), wood)
mesh_actor("待命后横栏_上", cube, (280, 895, 10), (6.2, 0.08, 0.07), wood)
mesh_actor("待命后横栏_下", cube, (280, 895, -10), (6.2, 0.08, 0.07), wood)

for index, (x, y) in enumerate(((-570, -220), (-570, 720), (1120, -220), (1120, 720),
                                 (-300, 1080), (900, 1080))):
    mesh_actor(f"树干_{index}", cylinder, (x, y, 15), (0.22, 0.22, 1.45), wood)
    mesh_actor(f"树冠_{index}", sphere, (x, y, 115), (1.0, 1.0, 1.0), leaf)
    mesh_actor(f"树冠亮点_{index}", sphere, (x - 30, y - 18, 145), (0.34, 0.34, 0.34), grass)

for index, (x, y) in enumerate(((-250, 280), (850, 260), (0, -360), (570, -360),
                                 (-220, 650), (840, 730))):
    mesh_actor(f"花丛_{index}", sphere, (x, y, -30), (0.24, 0.24, 0.15), pink)
    mesh_actor(f"花心_{index}", sphere, (x, y, -16), (0.08, 0.08, 0.08), gold)

sun = levels.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(100, -200, 900),
    unreal.Rotator(-48, -30, 0))
sun.set_actor_label("温暖主光")
sun.light_component.set_editor_property("intensity", 4.0)

sky = levels.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(280, 280, 400))
sky.set_actor_label("环境补光")
sky.light_component.set_editor_property("intensity", 1.2)

assert levels.save_current_level()
unreal.log(f"XIAOMA_SCENE: saved /Game/Maps/Main with {len(levels.get_all_level_actors())} actors")
