"""Keep lighting fully dynamic so the shipped level needs no baked light build."""

import unreal


levels = unreal.EditorLevelLibrary
assert levels.load_level("/Game/Maps/Main")
lights = {}
for actor in levels.get_all_level_actors():
    if actor.get_actor_label() in ("温暖主光", "环境补光"):
        actor.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        lights[actor.get_actor_label()] = actor

assert set(lights) == {"温暖主光", "环境补光"}, lights
lights["温暖主光"].light_component.set_editor_property("intensity", 2.0)
lights["环境补光"].light_component.set_editor_property("intensity", 2.0)
assert levels.save_current_level()
unreal.log("XIAOMA_LIGHTING: two movable lights saved")
