from pathlib import Path
import unreal


root = Path(__file__).resolve().parents[1]
for name in ("Place", "Error", "Win"):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(root / "Content" / "ImportSource" / "Audio" / f"{name}.wav"))
    task.set_editor_property("destination_path", "/Game/Audio")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
    assert paths, name
    asset = unreal.EditorAssetLibrary.load_asset(f"/Game/Audio/{name}")
    assert isinstance(asset, unreal.SoundWave), name
    unreal.log(f"XIAOMA_AUDIO: {name} -> {paths}")
