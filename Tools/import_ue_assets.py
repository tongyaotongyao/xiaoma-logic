"""Import staged GLBs into a temporary UE project before the C++ build is repaired."""

from pathlib import Path
import unreal


root = Path(__file__).resolve().parents[1]
tools = unreal.AssetToolsHelpers.get_asset_tools()
colours = ("Blue", "Brown", "Pink", "Red", "Yellow", "Mint")
jobs = [
    (root / "Content" / "ImportSource" / "Ponies" / f"Pony_{name}.glb", "/Game/Ponies")
    for name in colours
]
jobs += [
    (root / "Content" / "ImportSource" / "Characters" / f"{name}.glb", f"/Game/Characters/{name}")
    for name in colours
]

for filename, destination in jobs:
    if not filename.is_file():
        raise FileNotFoundError(filename)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([task])
    paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
    unreal.log(f"XIAOMA_ASSET: {filename.name} -> {paths}")
    if not paths:
        raise RuntimeError(f"No asset imported from {filename}")

unreal.EditorAssetLibrary.save_directory("/Game/Ponies", only_if_is_dirty=False, recursive=True)
unreal.EditorAssetLibrary.save_directory("/Game/Characters", only_if_is_dirty=False, recursive=True)
unreal.log("XIAOMA_ASSET: import complete")
