"""Replace only the heavy punch; preserve all meshes, light punches and locomotion."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root/'AssetsSource/CityCourier'
definition = json.loads((source/'compact_heavy.json').read_text(encoding='utf-8'))
fbx = source/'Exports/ProductionActions/A_HeavyPunch_R.fbx'
assert hashlib.sha256(fbx.read_bytes()).hexdigest() == definition['fbx_sha256']
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
folder = '/Game/CityCourier/Art/Characters'
protected = [p for p in (root/'Content/CityCourier/Art/Characters').glob('*.uasset') if p.stem != 'A_HeavyPunch_R']
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {p: digest(p) for p in protected}
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.import_mesh = False
options.import_as_skeletal = True
options.import_animations = True
options.import_materials = False
options.import_textures = False
options.skeleton = unreal.load_asset(folder+'/SK_Courier_Skeleton')
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', False)
options.anim_sequence_import_data.set_editor_property('custom_sample_rate', 60)
task = unreal.AssetImportTask()
task.filename = str(fbx)
task.destination_path = folder
task.destination_name = 'A_HeavyPunch_R'
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
task.factory = unreal.FbxFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
clip = unreal.load_asset(folder+'/A_HeavyPunch_R')
assert isinstance(clip, unreal.AnimSequence) and clip.get_editor_property('skeleton') == options.skeleton
assert abs(clip.get_play_length()-.95) < .02
clip.set_editor_property('enable_root_motion', False)
assert assets.save_loaded_asset(clip)
assert all(digest(p) == before[p] for p in protected)
definition.update(runtime_import='complete', runtime_package_sha256=digest(root/'Content/CityCourier/Art/Characters/A_HeavyPunch_R.uasset'))
out = root/'Saved/Validation/CompactHeavy'
out.mkdir(parents=True, exist_ok=True)
(out/'import.json').write_text(json.dumps(definition, indent=2)+'\n', encoding='utf-8')
unreal.log('COMPACT_HEAVY_IMPORTED')
