"""Import one CC0-derived idle and bind it to the real courier animation graph."""
import hashlib
import json
from pathlib import Path
import unreal

raise RuntimeError('Retired hand-edited idle importer. Use replace_all_character_actions.py instead.')

root = Path(unreal.Paths.project_dir()).resolve()
source = root/'AssetsSource/CityCourier'
report_path = source/'relaxed_idle.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
folder = '/Game/CityCourier/Art/Characters'
name = 'A_IdleRelaxed'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
path = source/'Exports/RelaxedIdle'/f'{name}.fbx'
assert digest(path) == report['fbx_sha256']
assert report['skeleton_reference_unchanged'] and report['rider_mesh_unchanged']
assert report['validation']['status'] == 'passed'
protected = [p for directory in ('Content/CityCourier/Art/Characters', 'Content/CityCourier/Blueprints')
             for p in (root/directory).rglob('*.uasset') if p.stem not in (name, 'ABP_Courier')]
before = {str(p.relative_to(root)): digest(p) for p in protected}
skeleton = unreal.load_asset(folder+'/SK_Courier_Skeleton')
mesh = unreal.load_asset(folder+'/SK_Courier')
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
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
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
task = unreal.AssetImportTask()
task.filename, task.destination_path, task.destination_name = str(path), folder, name
task.automated, task.replace_existing, task.save = True, True, False
task.options, task.factory = options, unreal.FbxFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert [str(p) for p in task.imported_object_paths] == [folder+'/'+name+'.'+name]
clip = unreal.load_asset(folder+'/'+name)
assert clip.get_editor_property('skeleton') == skeleton
assert abs(clip.get_play_length()-report['duration_s']) < .001
clip.set_editor_property('enable_root_motion', False)
clip.set_editor_property('loop', True)
clip.set_editor_property('compression_error_threshold_scale', 100.)
clip.set_editor_property('allow_frame_stripping', False)
clip.set_editor_property('bone_compression_settings', unreal.load_asset('/Engine/Animation/DefaultRecorderBoneCompression'))
unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
assert assets.save_loaded_asset(clip, only_if_is_dirty=False)
opts = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED, optional_skeletal_mesh=mesh)
first = clip.get_anim_pose_at_time(0., opts)
last = clip.get_anim_pose_at_time(clip.get_play_length(), opts)
bones = ('root', 'pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')
positions = lambda pose: {n: pose.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation for n in bones}
start, end = positions(first), positions(last)
loop_error = max((start[n]-end[n]).length() for n in bones)
assert loop_error < .1, ('Loop seam', loop_error)
foot_drift = 0.
compression_error = 0.
for i in range(31):
    t = clip.get_play_length()*i/30
    p = positions(clip.get_anim_pose_at_time(t, opts))
    raw_opts = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW, optional_skeletal_mesh=mesh)
    raw = positions(clip.get_anim_pose_at_time(t, raw_opts))
    compression_error = max(compression_error, max((raw[n]-p[n]).length() for n in bones))
    foot_drift = max(foot_drift, *((p[n]-start[n]).length() for n in ('foot_l', 'foot_r')))
    assert (p['root']-start['root']).length() < .01
assert foot_drift < .1 and compression_error < .1, (foot_drift, compression_error)
bp = unreal.load_asset('/Game/CityCourier/Blueprints/ABP_Courier')
assert unreal.CourierArtBuilder.build_animation_blueprint(bp)
clips = [unreal.load_asset(folder+'/'+n) for n in [name, 'A_Jog', 'A_Sprint', 'A_Ride']]
unreal.get_default_object(bp.generated_class()).set_editor_property('clips', clips)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp, only_if_is_dirty=False)
assert all(digest(root/p) == h for p, h in before.items()), 'Unrelated rider packages changed'
report['ue_import'] = {'status': 'passed', 'loop_error_cm': loop_error, 'foot_drift_cm': foot_drift,
                       'raw_compressed_error_cm': compression_error, 'protected_package_count': len(before),
                       'protected_package_sha256': before, 'existing_packages_unchanged': True,
                       'bound_clips': [c.get_name() for c in clips]}
report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
unreal.log('RELAXED IDLE IMPORT COMPLETE '+json.dumps(report['ue_import']))
