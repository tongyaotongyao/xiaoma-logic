from pathlib import Path
import unreal


source = Path(__file__).resolve().parents[1] / "Content" / "ImportSource" / "Board" / "Tile.glb"
task = unreal.AssetImportTask()
task.set_editor_property("filename", str(source))
task.set_editor_property("destination_path", "/Game/Board")
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset = unreal.EditorAssetLibrary.load_asset("/Game/Board/Tile")
assert isinstance(asset, unreal.StaticMesh)
unreal.log(f"XIAOMA_TILE: {asset.get_bounds()}")
