"""Import posture animation only; validate skeleton, loop seams and protected assets."""
import ast
import json
import types
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root/'AssetsSource/CityCourier'
report_path = source/'posture_actions.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
assert report['validation']['status'] == 'passed'
names = set(report['clips'])
assert names == {'A_CrouchIdle', 'A_CrouchWalk', 'A_ProneIdle', 'A_ProneCrawl'}
helper = root/'Content/Python/import_pending_actions.py'
tree = ast.parse(helper.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if isinstance(n, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
checks = types.ModuleType('posture_import_checks')
exec(compile(tree, str(helper), 'exec'), checks.__dict__)
protected = [p for directory in ('Content/CityCourier/Art/Characters', 'Content/CityCourier/Blueprints')
             for p in (root/directory).rglob('*.uasset') if p.stem not in names]
before = {str(p.relative_to(root)): checks.digest(p) for p in protected}
folder = '/Game/CityCourier/Art/Characters'
skeleton, mesh = unreal.load_asset(folder+'/SK_Courier_Skeleton'), unreal.load_asset(folder+'/SK_Courier')
reference = checks.reference(skeleton)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
results = {}
for name, profile in report['clips'].items():
    path = source/'Exports/PostureActions'/f'{name}.fbx'
    assert checks.digest(path) == profile['fbx_sha256']
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_mesh, options.import_as_skeletal, options.import_animations = False, True, True
    options.import_materials, options.import_textures, options.skeleton = False, False, skeleton
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', False)
    options.anim_sequence_import_data.set_editor_property('custom_sample_rate', profile['fps'])
    options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(path), folder, name
    task.automated, task.replace_existing, task.save = True, True, False
    task.options, task.factory = options, unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert list(task.imported_object_paths) == [folder+'/'+name+'.'+name]
    clip = unreal.load_asset(folder+'/'+name)
    assert clip.get_editor_property('skeleton') == skeleton
    assert abs(clip.get_play_length()-profile['duration_s']) < .001
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', False)
    clip.set_editor_property('loop', True)
    clip.set_editor_property('compression_error_threshold_scale', 100.)
    clip.set_editor_property('allow_frame_stripping', False)
    clip.set_editor_property('bone_compression_settings', unreal.load_asset('/Engine/Animation/DefaultRecorderBoneCompression'))
    unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
    results[name] = checks.validate_imported_pose(clip, mesh)
    assert results[name]['endpoint_joint_error_cm'] < .01
    assert checks.reference(skeleton) == reference
    assert assets.save_loaded_asset(clip, only_if_is_dirty=False)
assert all(checks.digest(root/p) == h for p, h in before.items()), 'Protected packages changed'
report['ue_import'] = {'status': 'passed', 'clips': results, 'existing_packages_unchanged': True}
report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
unreal.log('POSTURE IMPORT COMPLETE '+json.dumps(results))
