"""Import only seven new animation assets; protect all existing rider and ABP data.

Run through scripts/Run-EditorScript.ps1 after source validation. This script
does not build an animation blueprint, create montages, or save a level.
"""
import hashlib
import json
from pathlib import Path

import unreal

raise RuntimeError('Retired hand-authored pose importer. Use replace_all_character_actions.py; helper definitions remain for posture imports.')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(transform):
    t, q, s = transform.translation, transform.rotation, transform.scale3d
    return [t.x, t.y, t.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z]


def reference(skeleton):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    return {str(name): record(pose.get_bone_pose(name, unreal.AnimPoseSpaces.LOCAL))
            for name in unreal.AnimPoseExtensions.get_bone_names(pose)}


def validate_imported_pose(clip, mesh):
    options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW,
                                              optional_skeletal_mesh=mesh)
    initial = clip.get_anim_pose_at_time(0., options)
    bone_names = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(initial)}
    roots = [n for n in ('CourierRig', 'root') if n in bone_names]
    assert 'root' in roots, 'Imported animation has no authored root bone'
    initial_roots = {n: record(initial.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD)) for n in roots}
    max_translation, max_rotation, compression_error = 0., 0., 0.
    last = None
    for index in range(21):
        pose = clip.get_anim_pose_at_time(clip.get_play_length()*index/20, options)
        compressed = clip.get_anim_pose_at_time(clip.get_play_length()*index/20,
            unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED, optional_skeletal_mesh=mesh))
        compression_error = max(compression_error, max((pose.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation-
            compressed.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation).length() for n in ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')))
        for name in roots:
            current, first = record(pose.get_bone_pose(name, unreal.AnimPoseSpaces.WORLD)), initial_roots[name]
            max_translation = max(max_translation, sum((a-b)**2 for a, b in zip(current[:3], first[:3]))**.5)
            max_rotation = max(max_rotation, min(max(abs(a-b) for a, b in zip(current[3:7], first[3:7])),
                                                 max(abs(a+b) for a, b in zip(current[3:7], first[3:7]))))
        last = pose
    assert max_translation < .01 and max_rotation < .0001, ('UE raw root drift', clip.get_name(), max_translation, max_rotation)
    assert compression_error < .1, ('UE raw/compressed pose error exceeds 1mm', clip.get_name(), compression_error)
    endpoint_error = max((last.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation-
                          initial.get_bone_pose(n, unreal.AnimPoseSpaces.WORLD).translation).length()
                         for n in ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r'))
    return {'raw_pose_samples': 21, 'raw_compressed_pose_error_cm': compression_error, 'root_translation_cm': max_translation,
            'root_quaternion_component_error': max_rotation, 'endpoint_joint_error_cm': endpoint_error}


def main():
    root = Path(unreal.Paths.project_dir()).resolve()
    source = root/'AssetsSource/CityCourier'
    report_path = source/'pending_actions.json'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    assert report['validation']['status'] == 'passed', 'Run Blender source validation before import'
    folder = '/Game/CityCourier/Art/Characters'
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    names = list(report['clips'])
    assert set(names) == {'A_JumpStart', 'A_JumpLoop', 'A_Land', 'A_HitReact_Front', 'A_HitReact_Back', 'A_RunStart', 'A_RunStop'}
    existing = [p for directory in ('Content/CityCourier/Art/Characters', 'Content/CityCourier/Blueprints')
                for p in (root/directory).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.uexp', '.ubulk')]
    # Permit an idempotent rerun only for these seven newly authored assets.
    protected = [p for p in existing if not (p.parent == root/'Content/CityCourier/Art/Characters' and p.stem in names)]
    before = {str(p.relative_to(root)): digest(p) for p in protected}
    skeleton = unreal.load_asset(folder+'/SK_Courier_Skeleton')
    assert isinstance(skeleton, unreal.Skeleton)
    mesh = unreal.load_asset(folder+'/SK_Courier')
    expected_reference = reference(skeleton)
    compression = unreal.load_asset('/Engine/Animation/DefaultRecorderBoneCompression')
    assert isinstance(compression, unreal.AnimBoneCompressionSettings), 'Missing engine high-fidelity recorder compression settings'
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    imported = {}
    try:
        for name, profile in report['clips'].items():
            path = source/'Exports/PendingActions'/f'{name}.fbx'
            assert path.is_file() and digest(path) == profile['fbx_sha256'], ('Changed/missing source FBX', name)
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
            options.anim_sequence_import_data.set_editor_property('custom_sample_rate', profile['fps'])
            # Never mutate the shared reference pose on animation import.
            options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
            task = unreal.AssetImportTask()
            task.filename = str(path)
            task.destination_path = folder
            task.destination_name = name
            task.automated = True
            task.replace_existing = True
            task.save = False
            task.options = options
            task.factory = unreal.FbxFactory()
            asset_tools.import_asset_tasks([task])
            imported_paths = [str(p) for p in task.imported_object_paths]
            expected_path = folder+'/'+name+'.'+name
            # An empty result must fail before load_asset can reuse a stale
            # sequence from an earlier successful run. No other object/package
            # is accepted as part of this strictly animation-only import.
            assert imported_paths == [expected_path], ('Expected exactly the newly imported AnimSequence', name, imported_paths)
            clip = unreal.load_asset(folder+'/'+name)
            assert isinstance(clip, unreal.AnimSequence), ('Missing AnimSequence', name, task.imported_object_paths)
            assert clip.get_editor_property('skeleton') == skeleton
            assert abs(clip.get_play_length()-profile['duration_s']) < .001, (name, clip.get_play_length())
            clip.set_editor_property('enable_root_motion', False)
            clip.set_editor_property('force_root_lock', False)
            # The unchanged shared skeleton retains cumulative 100x scale.
            # Tighten compression on these new clips so small source-space
            # errors cannot grow above 1 mm in evaluated component space.
            clip.set_editor_property('compression_error_threshold_scale', 100.)
            clip.set_editor_property('allow_frame_stripping', False)
            clip.set_editor_property('bone_compression_settings', compression)
            # Sequence looping metadata is used by single-node preview; runtime
            # JumpLoop's evaluator also supplies bLooping from its state.
            clip.set_editor_property('loop', profile['loop'])
            # Property notifications clear cached data and queue recompression.
            # Evaluate only after the engine's public finish-all console command
            # has joined those jobs; otherwise a pose can be a ref-pose fallback.
            unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
            pose_checks = validate_imported_pose(clip, mesh)
            if profile['loop']:
                assert pose_checks['endpoint_joint_error_cm'] < .01, ('UE JumpLoop seam', pose_checks)
            assert reference(skeleton) == expected_reference, 'Animation import changed shared in-memory skeleton reference'
            assert assets.save_loaded_asset(clip, only_if_is_dirty=False)
            imported[name] = {'package': folder+'/'+name, 'duration_s': clip.get_play_length(),
                              'root_motion': clip.get_editor_property('enable_root_motion'),
                              'loop': clip.get_editor_property('loop'),
                              'skeleton': skeleton.get_path_name(), 'pose_validation': pose_checks}
    finally:
        changed = [path for path, value in before.items() if not (root/path).is_file() or digest(root/path) != value]
        assert not changed, ('Protected existing character/blueprint packages changed', changed)
    report['ue_import'] = {'status': 'passed', 'clips': imported,
                           'protected_package_count': len(before),
                           'protected_package_sha256': before,
                           'existing_package_hashes_unchanged': True,
                           'note': 'Import/load verification only. Runtime camera validation is separate.'}
    report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    unreal.log('PENDING ACTION IMPORT COMPLETE '+json.dumps(report['ue_import']))


main()
