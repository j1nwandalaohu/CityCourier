"""Replace animation data at existing player/NPC paths without native/Blueprint builds.

No automation tests, game launch, map generation or mesh/skeleton saves.
Each completed import is recorded immediately for interruption recovery.
"""
import hashlib
import json
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT/'AssetsSource/CityCourier'
OUT = ROOT/'Saved/Validation/LibraryActions'
OUT.mkdir(parents=True, exist_ok=True)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
player_folder = '/Game/CityCourier/Art/Characters'
citizen_folder = '/Game/CityCourier/Art/Ambient/CharMorph'
player_skeleton = unreal.load_asset(player_folder+'/SK_Courier_Skeleton')
citizen_skeleton = unreal.load_asset(citizen_folder+'/SK_CitizenCharMorph_Skeleton')
legacy_citizen_skeleton = unreal.load_asset('/Game/CityCourier/Art/Ambient/Citizen/SK_Courier_Skeleton')
if not player_skeleton or not citizen_skeleton:
    raise RuntimeError('Both existing independent character skeletons are required')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_file(package):
    return ROOT/'Content'/(package.removeprefix('/Game/')+'.uasset')


protected = [package_file(folder+'/'+name) for folder, name in (
    (player_folder, 'SK_Courier'), (player_folder, 'SK_Courier_Skeleton'),
    (citizen_folder, 'SK_CitizenCharMorph'), (citizen_folder, 'SK_CitizenCharMorph_Skeleton'))]
before = {str(path): sha(path) for path in protected}
if legacy_citizen_skeleton:
    path = package_file('/Game/CityCourier/Art/Ambient/Citizen/SK_Courier_Skeleton')
    before[str(path)] = sha(path)
library = json.loads((SOURCE/'library_actions.json').read_text(encoding='utf-8'))
jobs = []
for name, profile in library['clips'].items():
    if name in ('A_Knockdown', 'A_GetUp'):
        continue  # New actions are imported by import_library_actions.py.
    jobs.append((SOURCE/'Exports/LibraryActions'/f'{name}.fbx', profile, player_folder, name, player_skeleton, 100))
jobs.extend([
    (SOURCE/'Exports/LibraryActions/A_IdleRelaxed.fbx', library['clips']['A_IdleRelaxed'], player_folder, 'A_Idle', player_skeleton, 100),
    (SOURCE/'Exports/MotionProbe/A_M2M_Walk.fbx', {'source_action': 'Walk'}, player_folder, 'A_Walk', player_skeleton, 30),
    (SOURCE/'Exports/ProductionActions/A_Sprint.fbx', {'source_action': 'Sprint'}, player_folder, 'A_Run', player_skeleton, 30),
])
if legacy_citizen_skeleton:
    jobs.append((SOURCE/'Exports/MotionProbe/A_M2M_Walk.fbx', {'source_action': 'Walk'},
        '/Game/CityCourier/Art/Ambient/Citizen', 'A_Walk', legacy_citizen_skeleton, 30))
for group in ('Reactions', 'Behavior'):
    folder = SOURCE/'Ambient/CharMorph'/group
    manifest = json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
    if not manifest.get('pose_authoring', '').startswith('Library motion only'):
        raise RuntimeError('Run library-only NPC retargeting first: '+group)
    for name, profile in manifest['clips'].items():
        jobs.append((folder/(name+'.fbx'), profile, citizen_folder+'/'+group, name, citizen_skeleton, 60))
for path, profile, folder, name, skeleton, fps in jobs:
    if not path.is_file() or (profile.get('fbx_sha256') and sha(path) != profile['fbx_sha256']):
        raise RuntimeError('Missing or changed source export: '+str(path))
    existing = unreal.load_asset(folder+'/'+name)
    if not isinstance(existing, unreal.AnimSequence) or existing.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('Expected owned existing animation: '+folder+'/'+name)
transitions = []
for name, source_name, duration in (('A_RunStart', 'CC_M_Neutral_Sprint_Start_F_Lfoot', .2),
                                    ('A_RunStop', 'CC_M_Neutral_Sprint_Stop_F_Lfoot', .25)):
    source = unreal.load_asset('/Game/CityCourier/Animation/Epic/Clips/'+source_name)
    target = unreal.load_asset(player_folder+'/'+name)
    if not source or not target or source.get_editor_property('skeleton') != player_skeleton:
        raise RuntimeError('Missing target-retargeted GASP transition: '+name)
    transitions.append((name, source, target, duration))
receipt = {'scope': 'player_and_npc', 'assets': {}, 'status': 'in_progress',
    'native_build': 'not_run', 'blueprint_generation': 'not_run', 'tests': 'not_run', 'game_window': 'not_launched'}
receipt_path = OUT/'all-characters-replaced.json'
if receipt_path.exists():
    receipt = json.loads(receipt_path.read_text(encoding='utf-8'))


def already_imported(package, source_hash):
    entry = receipt['assets'].get(package, {})
    path = package_file(package)
    return entry.get('source_fbx_sha256') == source_hash and path.exists() and entry.get('runtime_package_sha256') == sha(path)


def checkpoint(package, definition):
    receipt['assets'][package] = {**definition, 'runtime_package_sha256': sha(package_file(package))}
    (OUT/'all-characters-replaced.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')


# Refresh the final exports at the new native loader paths as well.
if not all(already_imported('/Game/CityCourier/Animation/Library/'+name, profile['fbx_sha256']) for name, profile in library['clips'].items()):
    runpy.run_path(str(ROOT/'Content/Python/import_library_actions.py'), run_name='__main__')
for name, profile in library['clips'].items():
    checkpoint('/Game/CityCourier/Animation/Library/'+name,
        {'source_action': profile['source_action'], 'source_fbx_sha256': profile['fbx_sha256'], 'operation': 'library_import'})
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
for path, profile, folder, name, skeleton, fps in jobs:
    if already_imported(folder+'/'+name, sha(path)):
        continue
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
    options.anim_sequence_import_data.set_editor_property('custom_sample_rate', fps)
    options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(path), folder, name
    task.automated, task.replace_existing, task.save = True, True, False
    task.options, task.factory = options, unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    clip = unreal.load_asset(folder+'/'+name)
    if clip.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('Import changed skeleton: '+name)
    if 'duration_s' in profile and abs(clip.get_play_length()-profile['duration_s']) > 1/fps+.0001:
        raise RuntimeError('Duration mismatch: '+name)
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', True)
    if not assets.save_loaded_asset(clip, only_if_is_dirty=False):
        raise RuntimeError('Could not save: '+name)
    checkpoint(folder+'/'+name, {'source_action': profile.get('source_action', name),
        'source_fbx': path.relative_to(ROOT).as_posix(), 'source_fbx_sha256': sha(path),
        'duration_s': clip.get_play_length(), 'operation': 'existing_animation_reimport'})

# Reuse GASP's already retargeted poses at the legacy transition addresses.
# Sample before mutating the target and keep both root conversion tracks fixed.
reference = unreal.AnimPoseExtensions.get_reference_pose(player_skeleton)
bones = unreal.AnimPoseExtensions.get_bone_names(reference)
for name, source, target, duration in transitions:
    count = round(duration*100)
    keys = {}
    for bone in bones:
        transforms = [unreal.AnimationLibrary.get_bone_pose_for_time(source, bone, source.get_play_length()*i/count, False) for i in range(count+1)]
        if str(bone) in ('CourierRig', 'root'):
            transforms = [reference.get_bone_pose(bone, unreal.AnimPoseSpaces.LOCAL)]*(count+1)
        keys[bone] = transforms
    model = target.get_editor_property('data_model_interface')
    # UE 5.6 assets use the Sequencer-backed model; the legacy controller
    # performs a checked cast to AnimDataModel and must never receive it.
    # Script-interface properties are returned as AnimationDataModel wrappers,
    # so Python isinstance does not expose the underlying native model class.
    model_class = model.get_class().get_path_name()
    if model_class == '/Script/AnimationData.AnimationSequencerDataModel':
        controller = unreal.AnimSequencerController()
    elif model_class == '/Script/Engine.AnimDataModel':
        controller = unreal.AnimDataController()
    else:
        raise RuntimeError('Unsupported native animation model: '+model_class)
    controller.set_model(model)
    controller.open_bracket('Replace authored transition with GASP library motion', False)
    try:
        controller.remove_all_bone_tracks(False)
        controller.set_frame_rate(unreal.FrameRate(100, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(count), False)
        for bone, transforms in keys.items():
            if not controller.add_bone_curve(bone, False):
                raise RuntimeError('Could not add track: '+str(bone))
            if not controller.set_bone_track_keys(bone, [t.translation for t in transforms], [t.rotation for t in transforms], [t.scale3d for t in transforms], False):
                raise RuntimeError('Could not write track: '+str(bone))
    finally:
        controller.close_bracket(False)
    target.set_editor_property('enable_root_motion', False)
    target.set_editor_property('force_root_lock', True)
    if not assets.save_loaded_asset(target, only_if_is_dirty=False):
        raise RuntimeError('Could not save GASP transition: '+name)
    checkpoint(player_folder+'/'+name, {'source_action': source.get_name(), 'source_package': source.get_path_name(),
        'duration_s': target.get_play_length(), 'operation': 'library_pose_track_resampling'})
if any(sha(Path(path)) != value for path, value in before.items()):
    raise RuntimeError('A protected character mesh/skeleton package changed')
receipt.update(status='animation_resources_replaced', replaced_count=len(receipt['assets']), protected_character_packages_unchanged=True,
    pending=['native build for new player state logic', 'version 9 Epic idle graph regeneration', 'gameplay/visual validation'])
(OUT/'all-characters-replaced.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
unreal.log('ALL_CHARACTER_ANIMATION_RESOURCES_REPLACED '+str(len(receipt['assets'])))
