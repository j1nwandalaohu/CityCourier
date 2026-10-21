"""Import only new citizen behavior clips; never save the existing skeleton or reactions."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root/'AssetsSource/CityCourier/Ambient/CharMorph/Behavior'
folder = '/Game/CityCourier/Art/Ambient/CharMorph/Behavior'
report = json.loads((source/'manifest.json').read_text(encoding='utf-8'))
expected = {'A_CitizenGuard', 'A_CitizenPunchL', 'A_CitizenPunchR', 'A_CitizenRun', 'A_CitizenCower'}
if set(report['clips']) != expected:
    raise RuntimeError('Unexpected citizen behavior source set')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def reference(skeleton):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    result = {}
    for n in unreal.AnimPoseExtensions.get_bone_names(pose):
        t = pose.get_bone_pose(n, unreal.AnimPoseSpaces.LOCAL)
        result[str(n)] = [t.translation.x, t.translation.y, t.translation.z,
                         t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w,
                         t.scale3d.x, t.scale3d.y, t.scale3d.z]
    return result

skeleton = unreal.load_asset('/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph_Skeleton')
mesh = unreal.load_asset('/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph')
if not skeleton or not mesh:
    raise RuntimeError('Existing independent citizen skeleton and mesh are required')
before_ref = reference(skeleton)
protected = {p: sha(p) for parent in ('Ambient', 'Characters')
             for p in (root/'Content/CityCourier/Art'/parent).rglob('*.uasset')
             if not p.is_relative_to(root/'Content/CityCourier/Art/Ambient/CharMorph/Behavior')}
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
imported = {}
for name, profile in report['clips'].items():
    fbx = source/(name+'.fbx')
    if sha(fbx) != profile['fbx_sha256']:
        raise RuntimeError('Export hash mismatch: '+name)
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
    options.anim_sequence_import_data.set_editor_property('custom_sample_rate', 60)
    options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
    task = unreal.AssetImportTask()
    task.filename = str(fbx)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if list(task.imported_object_paths) != [folder+'/'+name+'.'+name]:
        raise RuntimeError('Unexpected import result: '+name)
    clip = unreal.load_asset(folder+'/'+name)
    if clip.get_editor_property('skeleton') != skeleton or abs(clip.get_play_length()-profile['duration_s']) > .001:
        raise RuntimeError('Skeleton/duration mismatch: '+name)
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', False)
    clip.set_editor_property('allow_frame_stripping', False)
    clip.set_editor_property('bone_compression_settings', unreal.load_asset('/Engine/Animation/DefaultRecorderBoneCompression'))
    clip.set_editor_property('loop', name not in ('A_CitizenPunchL', 'A_CitizenPunchR'))
    unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
    if reference(skeleton) != before_ref:
        raise RuntimeError('Citizen reference skeleton changed')
    raw = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW, optional_skeletal_mesh=mesh)
    compressed = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED, optional_skeletal_mesh=mesh)
    samples = []
    times = sorted(set([clip.get_play_length()*i/10 for i in range(11)] +
                       ([.22, .30, .38] if name.startswith('A_CitizenPunch') else [])))
    for time in times:
        a = clip.get_anim_pose_at_time(time, raw)
        b = clip.get_anim_pose_at_time(time, compressed)
        error = max((a.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation-
                     b.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation).length()
                    for n in ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r'))
        if error >= .2:
            raise RuntimeError('Compression changed pose: %s at %.3f by %.3f cm' % (name, time, error))
        hands = [a.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation for n in ('hand_l','hand_r')]
        fists = [a.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD) for n in ('hand_l','hand_r')]
        fists = [t.translation+t.rotation.get_axis_x()*5 for t in fists]
        samples.append({'time_s': time, 'compression_error_cm': error,
                        'left_hand_cm': list(hands[0].to_tuple()), 'right_hand_cm': list(hands[1].to_tuple()),
                        'left_fist_cm': list(fists[0].to_tuple()), 'right_fist_cm': list(fists[1].to_tuple())})
    if not assets.save_loaded_asset(clip, only_if_is_dirty=False):
        raise RuntimeError('Cannot save '+name)
    imported[name] = {'duration_s': clip.get_play_length(), 'asset': clip.get_path_name(), 'pose_samples': samples}
if any(not p.is_file() or sha(p) != h for p,h in protected.items()):
    raise RuntimeError('Existing protected asset package changed')
report['ue_import'] = {'clips': imported, 'existing_packages_unchanged': True, 'reference_unchanged': True}
(source/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
unreal.log('CITIZEN_BEHAVIOR_IMPORT_COMPLETE')
