"""Dress the playable map as a warm tabletop set."""

import unreal


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary
assert level_editor.load_level("/Game/Maps/Main")
by_label = {actor.get_actor_label(): actor for actor in actors.get_all_level_actors()}

palette = {
    "MI_Grass": (0.36, 0.52, 0.43),
    "MI_Cream": (0.86, 0.67, 0.45),
    "MI_Wood": (0.31, 0.16, 0.08),
    "MI_Leaves": (0.29, 0.50, 0.33),
    "MI_Gold": (0.95, 0.55, 0.20),
    "MI_Pink": (0.82, 0.34, 0.39),
}
for name, rgb in palette.items():
    material = assets.load_asset(f"/Game/Environment/{name}")
    assert material, name
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material, "Color", unreal.LinearColor(*rgb, 1))
    assert assets.save_loaded_asset(material, only_if_is_dirty=False)

tile = assets.load_asset("/Game/Board/Tile")
assert tile
by_label["软草地"].set_actor_scale3d(unreal.Vector(34, 34, 0.5))
for label in ("棋盘木质底座", "棋盘浅色基座"):
    component = by_label[label].static_mesh_component
    material = component.get_material(0)
    component.set_static_mesh(tile)
    component.set_material(0, material)
    component.set_cast_shadow(False)

key = by_label["温暖主光"]
key.set_actor_rotation(unreal.Rotator(roll=0, pitch=-68, yaw=-24), False)
key.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
key.light_component.set_editor_property("intensity", 2.2)
key.light_component.set_editor_property("light_source_angle", 9.0)
key.light_component.set_light_color(unreal.LinearColor(1.0, 0.91, 0.79, 1), False)

sky = by_label["环境补光"]
sky.light_component.set_editor_property("intensity", 0.4)
for label, pitch, yaw, intensity, colour in (
    ("桌游正面补光", -33, 76, 1.15, (0.78, 0.90, 1.0)),
    ("桌游侧面反光", -35, 190, 0.7, (1.0, 0.80, 0.64)),
):
    rotation = unreal.Rotator(roll=0, pitch=pitch, yaw=yaw)
    light = by_label.get(label) or actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(280, 280, 600), rotation)
    light.set_actor_label(label)
    light.set_actor_rotation(rotation, False)
    light.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    light.light_component.set_editor_property("intensity", intensity)
    light.light_component.set_editor_property("cast_shadows", False)
    light.light_component.set_light_color(unreal.LinearColor(*colour, 1), False)

for index, name in enumerate(("Blue", "Brown", "Pink", "Red", "Yellow")):
    x = -90 + index * 185
    y = 726 + abs(index - 2) * 12
    for prefix, z in (("角色台座", -25), ("原始角色", -15)):
        actor = by_label[f"{prefix}_{name}"]
        actor.set_actor_location(unreal.Vector(x, y, z), False, False)
        if prefix == "原始角色":
            actor.set_actor_scale3d(unreal.Vector(0.90, 0.90, 0.90))

for index, (x, y) in enumerate(((-152, -70), (715, -42), (40, -125),
                                (570, -138), (-155, 585), (715, 592))):
    by_label[f"花丛_{index}"].set_actor_location(unreal.Vector(x, y, -36), False, False)
    by_label[f"花心_{index}"].set_actor_location(unreal.Vector(x, y, -24), False, False)

assert level_editor.save_current_level()
unreal.log("XIAOMA_STYLE: tabletop colours, rounded board and studio lighting saved")
