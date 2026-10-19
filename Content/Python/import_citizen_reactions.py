"""Animation-only import onto the original citizen skeleton, with pose/hash checks."""
import hashlib
import json
from pathlib import Path
import unreal


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


root = Path(unreal.Paths.project_dir()).resolve()
source = root/'AssetsSource/CityCourier/Ambient/CharMorph/Reactions'
report = json.loads((source/'manifest.json').read_text())
folder = '/Game/CityCourier/Art/Ambient/CharMorph/Reactions'
skeleton = unreal.load_asset('/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph_Skeleton')
mesh = unreal.load_asset('/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph')
assert isinstance(skeleton, unreal.Skeleton) and isinstance(mesh, unreal.SkeletalMesh)
before_ref = reference(skeleton)
protected = {p: sha(p) for p in (root/'Content/CityCourier/Art/Ambient').rglob('*.uasset')
             if 'Reactions' not in p.parts}
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
imported = {}
for name, profile in report['clips'].items():
    fbx = source/(name+'.fbx')
    assert sha(fbx) == profile['fbx_sha256']
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
    assert list(task.imported_object_paths) == [folder+'/'+name+'.'+name]
    clip = unreal.load_asset(folder+'/'+name)
    assert isinstance(clip, unreal.AnimSequence)
    assert clip.get_editor_property('skeleton') == skeleton
    expected = (profile['samples']-1)/60
    assert abs(clip.get_play_length()-expected) < .001, (name, clip.get_play_length(), expected)
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', False)
    clip.set_editor_property('allow_frame_stripping', False)
    clip.set_editor_property('bone_compression_settings', unreal.load_asset('/Engine/Animation/DefaultRecorderBoneCompression'))
    clip.set_editor_property('loop', name.endswith(('Idle', 'Walk')))
    unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
    raw = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW, optional_skeletal_mesh=mesh)
    compressed = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED, optional_skeletal_mesh=mesh)
    rows = []
    for i in range(11):
        a = clip.get_anim_pose_at_time(clip.get_play_length()*i/10, raw)
        b = clip.get_anim_pose_at_time(clip.get_play_length()*i/10, compressed)
        error = max((a.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation-
                     b.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation).length()
                    for n in ('pelvis', 'head', 'hand_l', 'foot_l', 'foot_r'))
        assert error < .2, (name, i, error)
        head = a.get_bone_pose('head', unreal.AnimPoseSpaces.WORLD).translation
        pelvis = a.get_bone_pose('pelvis', unreal.AnimPoseSpaces.WORLD).translation
        rows.append({'u': i/10, 'head_cm': [head.x, head.y, head.z],
                     'pelvis_cm': [pelvis.x, pelvis.y, pelvis.z], 'compression_error_cm': error})
    assert reference(skeleton) == before_ref, 'Citizen reference skeleton changed'
    assert assets.save_loaded_asset(clip, only_if_is_dirty=False)
    imported[name] = {'duration_s': clip.get_play_length(), 'pose_samples': rows}
assert all(p.is_file() and sha(p) == value for p, value in protected.items()), 'Existing ambient packages changed'
report['ue_import'] = {'clips': imported, 'reference_unchanged': True,
                       'existing_package_hashes_unchanged': True, 'status': 'passed'}
(source/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
unreal.log('CITIZEN_REACTIONS_IMPORT_PASSED')
