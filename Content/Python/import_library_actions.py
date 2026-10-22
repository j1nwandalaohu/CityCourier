"""Import validated library replacements against the existing courier skeleton.

Does not rebuild native code, open a game window, or alter source sample projects.
Graph regeneration is deliberately a separate, post-build operation.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT/'AssetsSource/CityCourier'
FOLDER = '/Game/CityCourier/Animation/Library'
report = json.loads((SOURCE/'library_actions.json').read_text(encoding='utf-8'))
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
skeleton = unreal.load_asset('/Game/CityCourier/Art/Characters/SK_Courier_Skeleton')
if not skeleton or not report['skeleton_reference_unchanged']:
    raise RuntimeError('Courier reference skeleton is missing or changed')
protected = [ROOT/'Content/CityCourier/Art/Characters'/name for name in ('SK_Courier.uasset', 'SK_Courier_Skeleton.uasset')]
before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
for name, clip in report['clips'].items():
    path = SOURCE/'Exports/LibraryActions'/f'{name}.fbx'
    if hashlib.sha256(path.read_bytes()).hexdigest() != clip['fbx_sha256']:
        raise RuntimeError('Export hash mismatch: '+name)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
results = {}
for name, definition in report['clips'].items():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', False)
    options.anim_sequence_import_data.set_editor_property('custom_sample_rate', report['fps'])
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE/'Exports/LibraryActions'/f'{name}.fbx')
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    clip = unreal.load_asset(FOLDER+'/'+name)
    if not isinstance(clip, unreal.AnimSequence) or clip.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('Missing clip or skeleton mismatch: '+name)
    if abs(clip.get_play_length()-definition['duration_s']) > .011:
        raise RuntimeError('Clip duration mismatch: '+name)
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', True)
    if not assets.save_loaded_asset(clip):
        raise RuntimeError('Could not save: '+name)
    package = ROOT/'Content/CityCourier/Animation/Library'/f'{name}.uasset'
    results[name] = {'asset': clip.get_path_name(), 'duration_s': clip.get_play_length(), 'source_fbx_sha256': definition['fbx_sha256'],
                     'runtime_package_sha256': hashlib.sha256(package.read_bytes()).hexdigest()}
for p, digest in before.items():
    if hashlib.sha256(p.read_bytes()).hexdigest() != digest:
        raise RuntimeError('Import changed protected courier package: '+str(p))
out = ROOT/'Saved/Validation/LibraryActions'
out.mkdir(parents=True, exist_ok=True)
(out/'import.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
unreal.log('LIBRARY_IMPORT_COMPLETE '+json.dumps(results))
