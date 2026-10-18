"""Keep the retained citizen mesh and walk on their original, independent rig."""
import unreal,runpy
from pathlib import Path

assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
folder='/Game/CityCourier/Art/Ambient/Citizen/'
skeleton=unreal.load_asset(folder+'SK_Courier_Skeleton')
mesh=unreal.load_asset('/Game/CityCourier/Art/Ambient/SK_Citizen')
walk=unreal.load_asset(folder+'A_Walk')
assert skeleton and mesh and walk
root=Path(unreal.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Content/Python/import_ambient.py'),init_globals={'CITIZEN_ONLY':True})
mesh=unreal.load_asset('/Game/CityCourier/Art/Ambient/SK_Citizen')
if walk.get_editor_property('skeleton')!=skeleton:
    assert assets.delete_asset(folder+'A_Walk')
options=unreal.FbxImportUI()
options.automated_import_should_detect_type=False
options.import_as_skeletal=True
options.import_mesh=False
options.import_animations=True
options.import_materials=False
options.import_textures=False
options.skeleton=skeleton
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION
task=unreal.AssetImportTask()
task.filename=str(root/'AssetsSource/CityCourier/Exports/A_Walk.fbx')
task.destination_path=folder.rstrip('/')
task.destination_name='A_Walk'
task.automated=True
task.replace_existing=True
task.save=True
task.options=options
task.factory=unreal.FbxFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
walk=unreal.load_asset(folder+'A_Walk')
assert mesh.get_editor_property('skeleton')==skeleton
assert walk.get_editor_property('skeleton')==skeleton
for asset in [skeleton,mesh,walk]:
    assert assets.save_loaded_asset(asset,only_if_is_dirty=False)
unreal.log('Citizen mesh and walk saved with independent original skeleton')
