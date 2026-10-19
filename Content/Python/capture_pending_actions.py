"""Actual UE PIE samples of all seven new clips, then honest native movement checks.

Run Run-EditorScript.ps1 -Gui. No asset/map is saved. Output is entirely under
Saved/PendingActions/UE. Clip samples are explicitly labeled clip-sample; native
checks use the possessed production CourierCharacter, ABP and Enhanced Input.
CC_PENDING_RUNTIME=0 disables only the native movement checks.
"""
import ast
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import time
import traceback
import types
import uuid

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
CAPTURE_CODE_SHA256 = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
RUNTIME_SOURCE = ROOT/'Source/CityCourier/Player/CourierActionComponent.cpp'
RUNTIME_SOURCE_SHA256 = hashlib.sha256(RUNTIME_SOURCE.read_bytes()).hexdigest()
OUT = ROOT/'Saved/PendingActions/UE'
OUT.mkdir(parents=True, exist_ok=True)
RUN_ID = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())+'-'+uuid.uuid4().hex[:8]
RUN_OUT = OUT/'runs'/RUN_ID
RUN_OUT.mkdir(parents=True, exist_ok=True)
WIDTH, HEIGHT = 1800, 1200
FOLDER = '/Game/CityCourier/Art/Characters/'
TIMES = {'A_JumpStart': .12, 'A_JumpLoop': .20, 'A_Land': .07,
         'A_HitReact_Front': .09, 'A_HitReact_Back': .09,
         'A_RunStart': .15, 'A_RunStop': .12}
BASELINE_ONLY = os.environ.get('CC_PENDING_BASELINE_ONLY', '0') == '1'
if BASELINE_ONLY:
    TIMES = {'A_Idle': .7}
ANGLES = tuple(os.environ.get('CC_PENDING_ANGLES', 'front,side,rear').split(','))
assert set(ANGLES).issubset({'front', 'side', 'rear'})
JOBS = [dict(clip=name, sample=seconds, angle=angle, evidence='existing-idle-baseline' if BASELINE_ONLY else 'clip-sample')
        for name, seconds in TIMES.items() for angle in ANGLES]
RUNTIME = not BASELINE_ONLY and os.environ.get('CC_PENDING_RUNTIME', '1') != '0'
SKIP_SAMPLES = not BASELINE_ONLY and os.environ.get('CC_PENDING_SKIP_SAMPLES', '0') == '1'
if SKIP_SAMPLES:
    assert RUNTIME, 'CC_PENDING_SKIP_SAMPLES requires native runtime checks'
    JOBS = []
BONES = ('root', 'pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')

# Reuse the reviewed portrait helpers without importing its top-level launcher,
# default jobs, quick/environment filters or native ride fixture. AST selection
# executes its imports/functions only; no existing source or module is changed.
helper_path = ROOT/'Content/Python/capture_photoreal_rider.py'
tree = ast.parse(helper_path.read_text(encoding='utf-8'), filename=str(helper_path))
tree.body = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
helpers = types.ModuleType('pending_portrait_helpers')
exec(compile(tree, str(helper_path), 'exec'), helpers.__dict__)
helpers.HIDDEN_HEAD_SLOTS = {'M_Helmet', 'M_Rubber', 'M_ReflectiveStrip'}
require, xyz = helpers.require, helpers.xyz

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
stage, index, handle, busy, closing, owns_pie = 'startup', 0, None, False, False, False
started = time.monotonic()
shots, native_samples = [], []
runtime = {'status': 'pending' if RUNTIME else 'disabled',
           'evidence': 'Enhanced Input and forced AddMovementInput on real CourierCharacter/ABP; no single-node playback',
           'visual_acceptance': False,
           'run_stop_phase_limitation': 'A_RunStop starts at authored Sprint phase zero. Fixed-pose source matching alone does not validate arbitrary live release phases; native trajectories are reported separately.',
           'hit_reactions': 'clip-sample only; no public native strike callback invoked'}
protected = [p for folder in ('Content/CityCourier/Art/Characters', 'Content/CityCourier/Blueprints')
             for p in (ROOT/folder).rglob('*.uasset')]
protected += [ROOT/'Content/CityCourier/Maps/L_CharacterProbe.umap']
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}


def finish(error=None):
    global closing, final_error, cleanup_started, stage
    if closing:
        return
    if error and runtime['status'] == 'running':
        runtime['status'] = 'failed'
        runtime['error'] = error
    final_error, cleanup_started, closing, stage = error, time.monotonic(), True, 'cleanup'
    if owns_pie and editor.get_game_world():
        for name in ('IA_Jump', 'IA_Sprint'):
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), 'Input.-action '+name)
        levels.editor_request_end_play()
    if handle is None:
        cleanup()


def cleanup():
    global final_error
    if owns_pie and editor.get_game_world() and time.monotonic()-cleanup_started < 25.:
        return
    if owns_pie and editor.get_game_world():
        final_error = (final_error+'\n' if final_error else '')+'EndPIE teardown exceeded 25 seconds; game world remains present'
    changed = [name for name, value in before.items() if hashlib.sha256((ROOT/name).read_bytes()).hexdigest() != value]
    error = final_error
    if changed:
        error = (error+'\n' if error else '')+'Protected packages changed: '+str(changed)
    report = {'run_id': RUN_ID, 'complete': error is None and len(shots) == len(JOBS) and (not RUNTIME or runtime['status'] == 'passed'),
              'capture_complete': len(shots) == len(JOBS), 'error': error,
              'evidence_mode': 'native-only' if SKIP_SAMPLES else 'existing-idle-baseline' if BASELINE_ONLY else 'clip-samples-and-native-checks',
              'planned_shots': len(JOBS), 'dimensions': [WIDTH, HEIGHT], 'shots': shots,
              'native_runtime': runtime, 'native_samples': native_samples,
              'capture_code_sha256': CAPTURE_CODE_SHA256,
              'runtime_source': str(RUNTIME_SOURCE.relative_to(ROOT)), 'runtime_source_sha256': RUNTIME_SOURCE_SHA256,
              'protected_package_count': len(before), 'existing_packages_unchanged': not changed,
              'protected_package_sha256': before, 'no_map_or_asset_save': True,
              'interpretation': ('Native runtime trajectory checks only; zero screenshots requested or claimed.' if SKIP_SAMPLES else
                                 'Actual PIE clip-sample stills; native movement checks are recorded separately when enabled. Hit reactions remain clip-sample evidence.'),
              'failed_job': globals().get('active') if error else None,
              'elapsed_seconds': time.monotonic()-started}
    latest_name = 'results-idle-baseline.json' if BASELINE_ONLY else 'results-native.json' if SKIP_SAMPLES else 'results.json'
    for path in (RUN_OUT/'results.json', OUT/latest_name):
        path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    if error:
        unreal.log_error('PENDING ACTION CAPTURE FAILED '+error)
    else:
        unreal.log('PENDING ACTION CAPTURE COMPLETE '+str(OUT/'results.json'))
    # SetKeepPythonScriptAlive(false) lets EditorPythonExecuter defer its own
    # QUIT_EDITOR after this callback. An immediate second quit can race teardown.


def init_world():
    global world, pc, rider, parts, actor, component, camera, mesh, skeleton, anim_class, managers
    world = editor.get_game_world()
    if not world:
        return False
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if not pc or not pc.get_controlled_pawn():
        return False
    rider = pc.get_controlled_pawn()
    require(isinstance(rider, unreal.CourierCharacter), 'Not the production CourierCharacter')
    unreal.GameplayStatics.set_game_paused(world, False)
    all_actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    parts = {str(tag).removeprefix('CC_Probe_'): a for a in all_actors
             for tag in a.tags if str(tag).startswith('CC_Probe_')}
    require({'After', 'Camera', 'Floor', 'Key', 'Fill', 'Rim', 'Sky', 'Exposure'}.issubset(parts), 'Missing character lab fixtures')
    managers = []
    for a in all_actors:
        if a.get_components_by_class(unreal.MeshComponent) and a != parts['Floor']:
            a.set_actor_hidden_in_game(True)
        if isinstance(a, unreal.AmbientTrafficManager):
            managers.append(a)
            a.set_actor_tick_enabled(False)
        if hasattr(a, 'set_population_active'):
            a.set_population_active(False)
    actor, camera = parts['After'], parts['Camera']
    component = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    mesh = unreal.load_asset(FOLDER+'SK_Courier')
    skeleton = unreal.load_asset(FOLDER+'SK_Courier_Skeleton')
    anim_class = unreal.load_class(None, '/Game/CityCourier/Blueprints/ABP_Courier.ABP_Courier_C')
    require(mesh and skeleton and anim_class, 'Missing formal mesh, shared skeleton or ABP')
    component.set_skeletal_mesh_asset(mesh)
    helpers.configure_animation_tick(component)
    require(component.get_num_bones() == 51, 'Shared UE skeleton must remain 51 bones')
    rider.disable_input(pc)
    rider.character_movement.stop_movement_immediately()
    rider.character_movement.set_component_tick_enabled(False)
    rider.set_actor_enable_collision(False)
    rider.set_actor_location(unreal.Vector(2000, 2000, 90), False, True)
    for tag, intensity in (('Key', 12.), ('Fill', 1.2), ('Rim', .5), ('Sky', .10)):
        light = parts[tag].light_component
        light.set_editor_property('intensity', intensity)
        if tag != 'Sky':
            light.set_editor_property('use_temperature', True)
            light.set_editor_property('temperature', 6500.)
    camera.camera_component.set_editor_property('post_process_blend_weight', 0.)
    camera.camera_component.set_editor_property('constrain_aspect_ratio', False)
    pp = parts['Exposure'].get_editor_property('settings')
    for key in ('bloom_intensity', 'motion_blur_amount', 'vignette_intensity', 'film_grain_intensity'):
        pp.set_editor_property('override_'+key, True)
        pp.set_editor_property(key, 0.)
    parts['Exposure'].set_editor_property('settings', pp)
    for command in ['t.MaxFPS 30', 'ShowFlag.Game 1', 'ShowFlag.HUD 0', 'r.ScreenPercentage 100', 'r.Streaming.PoolSize 2400']+[
            'sg.'+name+'Quality 3' for name in ('ViewDistance', 'Shadow', 'GlobalIllumination', 'Reflection', 'Texture', 'Effects', 'Foliage', 'PostProcess')]+[
            'r.DepthOfFieldQuality 0', 'r.MotionBlurQuality 0']:
        unreal.SystemLibrary.execute_console_command(world, command)
    return True


def setup_sample():
    global active, sequence, screenshot_file, ready
    active = dict(JOBS[index], run_id=RUN_ID)
    sequence = unreal.load_asset(FOLDER+active['clip'])
    require(sequence and sequence.get_editor_property('skeleton') == skeleton, 'Clip is missing or has a different skeleton')
    component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    helpers.configure_animation_tick(component)
    actor.set_actor_location(unreal.Vector(0, 0, 0), False, True)
    actor.set_actor_rotation(unreal.Rotator(pitch=0., yaw=180., roll=0.), False)
    actor.set_actor_hidden_in_game(False)
    component.play_animation(sequence, False)
    component.set_play_rate(0.)
    component.set_position(active['sample'], False)
    active['sections'] = helpers.head_sections(component, True)
    offsets = {'front': (90., -550., 150.), 'side': (550., -35., 150.), 'rear': (90., 550., 150.)}
    pos, target = unreal.Vector(*offsets[active['angle']]), unreal.Vector(0., 0., 91.)
    camera.set_actor_location(pos, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, target), False)
    camera.camera_component.set_field_of_view(40.)
    pc.set_view_target_with_blend(camera, 0.)
    active.update(camera={'position_cm': xyz(pos), 'target_cm': xyz(target), 'fov_degrees': 40., 'constrain_aspect_ratio': False},
                  sequence=sequence.get_path_name(), mesh=mesh.get_path_name(), skeleton=skeleton.get_path_name(),
                  bone_count=component.get_num_bones(), root_motion=sequence.get_editor_property('enable_root_motion'),
                  animation_length_seconds=sequence.get_play_length(),
                  actor_rotation_pyr_degrees=[0., 180., 0.])
    screenshot_file = RUN_OUT/(active['clip']+'-'+active['angle']+'.png')
    ready = time.monotonic()+2.5


def sample_checks():
    raw = sequence.get_anim_pose_at_time(component.get_position(), unreal.AnimPoseEvaluationOptions(
        evaluation_type=unreal.AnimDataEvalType.RAW, optional_skeletal_mesh=mesh))
    compressed = sequence.get_anim_pose_at_time(component.get_position(), unreal.AnimPoseEvaluationOptions(
        evaluation_type=unreal.AnimDataEvalType.COMPRESSED, optional_skeletal_mesh=mesh))
    rendered_error, compression_error = 0., 0.
    diagnostics = {}
    for bone in BONES:
        actual = component.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation
        authored = raw.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation
        cooked = compressed.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation
        rendered_error = max(rendered_error, (actual-cooked).length())
        compression_error = max(compression_error, (authored-cooked).length())
        diagnostics[bone] = {'rendered_component_cm': xyz(actual), 'raw_component_cm': xyz(authored),
                             'compressed_component_cm': xyz(cooked), 'world_cm': xyz(component.get_socket_location(bone))}
    active.update(rendered_compressed_pose_error_cm=rendered_error, raw_compressed_pose_error_cm=compression_error,
                  bones=diagnostics, rendered_sample_seconds=component.get_position())
    require(rendered_error < .1 and (BASELINE_ONLY or compression_error < .1), 'Clip sample pose mismatch '+json.dumps(active))
    root = diagnostics['root']['raw_component_cm']
    active['root_offset_cm'] = root
    require(BASELINE_ONLY or sum(v*v for v in root)**.5 < .01, 'Authored root is not zero')
    initial = sequence.get_anim_pose_at_time(0., unreal.AnimPoseEvaluationOptions(
        evaluation_type=unreal.AnimDataEvalType.RAW, optional_skeletal_mesh=mesh))
    q = raw.get_bone_pose('root', unreal.AnimPoseSpaces.WORLD).rotation
    first_q = initial.get_bone_pose('root', unreal.AnimPoseSpaces.WORLD).rotation
    rotation_drift = min(max(abs(getattr(q, a)-getattr(first_q, a)) for a in ('x', 'y', 'z', 'w')),
                         max(abs(getattr(q, a)+getattr(first_q, a)) for a in ('x', 'y', 'z', 'w')))
    active['root_quaternion_drift_from_first_frame'] = rotation_drift
    require(BASELINE_ONLY or rotation_drift < .0001, 'Authored root orientation changed')
    require(component.get_num_bones() == 51, 'Bone count changed')
    require(not active['root_motion'], 'Root Motion must be disabled')
    require(diagnostics['head']['world_cm'][2] > min(diagnostics['foot_l']['world_cm'][2], diagnostics['foot_r']['world_cm'][2])+50.,
            'Rider is upside down or collapsed')
    require(pc.get_view_target() == camera, 'Camera view target changed')
    require(not any(m.get_active_vehicle_count() or m.get_active_pedestrian_count() for m in managers), 'Ambient population entered capture')


def prepare_native():
    global native_started, native_origin, native_seen, native_phase, native_ready, release_index, release_targets, sprint_poses
    actor.set_actor_hidden_in_game(True)
    rider.set_actor_hidden_in_game(False)
    rider.mesh.set_skeletal_mesh_asset(mesh)
    rider.mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT)
    rider.mesh.set_anim_instance_class(anim_class)
    helpers.configure_animation_tick(rider.mesh)
    rider.set_actor_rotation(unreal.Rotator(pitch=0., yaw=270., roll=0.), False)
    rider.set_actor_location(unreal.Vector(0, 0, 90), False, True)
    rider.set_actor_enable_collision(True)
    rider.character_movement.set_component_tick_enabled(True)
    rider.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    rider.character_movement.stop_movement_immediately()
    rider.enable_input(pc)
    pc.set_ignore_move_input(True)
    native_started, native_ready, native_phase = time.monotonic(), time.monotonic()+.5, 'settle'
    native_origin = rider.get_actor_location()
    native_seen = set()
    release_index, release_targets = 0, (.05, .30, .55, .80)
    if SKIP_SAMPLES and (OUT/'results.json').is_file():
        previous = json.loads((OUT/'results.json').read_text(encoding='utf-8'))
        trials = previous.get('native_runtime', {}).get('run_stop_releases', [])
        if len(trials) == 4:
            release_targets = tuple(row['measured_sprint_phase'] for row in trials)
            runtime['release_phase_baseline_run'] = previous['run_id']
            runtime['release_phase_baseline_file'] = str((OUT/'results.json').relative_to(ROOT))
    sprint_clip = unreal.load_asset(FOLDER+'A_Sprint')
    options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED, optional_skeletal_mesh=mesh)
    sprint_poses = []
    for index in range(240):
        pose = sprint_clip.get_anim_pose_at_time(sprint_clip.get_play_length()*index/240, options)
        sprint_poses.append({bone: pose.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation for bone in ('foot_l', 'foot_r')})
    runtime.update(status='running', character_class=rider.get_class().get_path_name(),
                   abp_class=rider.mesh.get_anim_instance().get_class().get_path_name(), bone_count=rider.mesh.get_num_bones(),
                   run_stop_releases=[])


def sprint_phase():
    actual = {bone: rider.mesh.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation for bone in ('foot_l', 'foot_r')}
    index = min(range(len(sprint_poses)), key=lambda i: sum((actual[n]-sprint_poses[i][n]).length()**2 for n in actual))
    error = max((actual[n]-sprint_poses[index][n]).length() for n in actual)
    return index/len(sprint_poses), error


def finish_release():
    rows = [row for row in native_samples if row.get('release_trial') == release_index and row['phase'] == 'stop']
    points = [runtime['run_stop_releases'][-1]['pre_release']] + rows
    maxima = {}
    for space in ('feet_component_cm', 'feet_world_cm'):
        for side in ('foot_l', 'foot_r'):
            steps = [math.dist(a[space][side], b[space][side]) for a, b in zip(points, points[1:])]
            maxima[space+'_'+side+'_max_frame_step_cm'] = max(steps) if steps else None
    runtime['run_stop_releases'][-1].update(samples=len(rows), observed_actual_run_stop=any(row['actual_montage_sequence'] and row['actual_montage_sequence'].endswith('.A_RunStop') for row in rows),
                                           trajectory_measurements=maxima,
                                           interpretation='Observed native transition trajectory, not an automated visual-acceptance verdict. Component-space motion includes natural leg swing; world-space data includes physical deceleration.')


def observe_native():
    global native_phase, native_ready, native_origin, release_index
    anim = rider.mesh.get_anim_instance()
    require(rider.mesh.get_editor_property('animation_mode') == unreal.AnimationMode.ANIMATION_BLUEPRINT, 'Native mode became single-node')
    montage = anim.get_current_active_montage()
    reference = montage.get_first_anim_reference() if montage else None
    blend_in = None
    blend_api_error = None
    if montage:
        try:
            blend_in = float(montage.get_editor_property('blend_in').get_editor_property('blend_time'))
        except Exception as error:
            blend_api_error = str(error)
    if reference:
        native_seen.add(reference.get_name())
    native_samples.append({'elapsed_s': time.monotonic()-native_started, 'phase': native_phase, 'release_trial': release_index,
                           'actor_cm': xyz(rider.get_actor_location()), 'velocity_cm_s': xyz(rider.get_velocity()),
                           'feet_component_cm': {n: xyz(rider.mesh.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_COMPONENT).translation) for n in ('foot_l', 'foot_r')},
                           'feet_world_cm': {n: xyz(rider.mesh.get_socket_location(n)) for n in ('foot_l', 'foot_r')},
                           'falling': rider.character_movement.is_falling(), 'anim_airborne': bool(anim.get_editor_property('airborne')),
                           'animation_index': int(anim.get_editor_property('animation_index')),
                           'montage': montage.get_path_name() if montage else None,
                           'montage_blend_in_seconds': blend_in, 'montage_blend_api_error': blend_api_error,
                           'actual_montage_sequence': reference.get_path_name() if reference else None})
    now = time.monotonic()
    if native_phase == 'settle' and now >= native_ready:
        native_origin = rider.get_actor_location()
        unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Jump true')
        native_phase, native_ready = 'jump', now+1.7
    elif native_phase == 'jump' and now >= native_ready:
        unreal.SystemLibrary.execute_console_command(world, 'Input.-action IA_Jump')
        require({'A_JumpStart', 'A_JumpLoop', 'A_Land'}.issubset(native_seen), 'Native jump did not show all three imported sequences: '+str(native_seen))
        jump = [row for row in native_samples if row['phase'] == 'jump']
        runtime['physical_jump_height_cm'] = max(row['actor_cm'][2] for row in jump)-native_origin.z
        require(runtime['physical_jump_height_cm'] > 25., 'Native jump did not move the physical character')
        require(any(row['falling'] and row['anim_airborne'] for row in jump), 'ABP did not report actual airborne movement')
        unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Sprint true')
        native_phase, native_ready = 'run', now+1.6
    elif native_phase == 'run':
        rider.add_movement_input(unreal.Vector(0, -1, 0), 1., True)
        if now >= native_ready:
            phase, error = sprint_phase()
            target = release_targets[release_index]
            phase_delta = min(abs(phase-target), 1-abs(phase-target))
            tolerance = .0125 if SKIP_SAMPLES else .065
            if error < 5. and phase_delta < tolerance and not reference:
                runtime['run_stop_releases'].append({'trial': release_index, 'target_sprint_phase': target,
                    'measured_sprint_phase': phase, 'target_phase_error': phase_delta, 'phase_fit_error_cm': error,
                    'phase_method': 'Nearest two-foot compressed Sprint pose, 240 phase candidates; private native clock is not exposed',
                    'pre_release': native_samples[-1]})
                unreal.SystemLibrary.execute_console_command(world, 'Input.-action IA_Sprint')
                native_phase, native_ready = 'stop', now+.7
            elif now > native_ready+4.:
                raise RuntimeError('Could not match native sprint release phase '+str((target, phase, error)))
    elif native_phase == 'stop' and now >= native_ready:
        finish_release()
        if release_index+1 < len(release_targets):
            release_index += 1
            rider.character_movement.stop_movement_immediately()
            rider.set_actor_location(unreal.Vector(0, 0, native_origin.z), False, True)
            unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Sprint true')
            native_phase, native_ready = 'run', now+1.6
            return
        require({'A_RunStart', 'A_RunStop'}.issubset(native_seen), 'Native run transitions not observed: '+str(native_seen))
        require(all(row['observed_actual_run_stop'] for row in runtime['run_stop_releases']), 'A release trial did not activate actual RunStop')
        require(any(row['animation_index'] == 2 for row in native_samples if row['phase'] == 'run'), 'Native run never reached sprint clip')
        runtime.update(status='passed', observed_native_sequences=sorted(native_seen), samples=len(native_samples),
                       integration_checks_passed=True, visual_acceptance=False,
                       assertions=['Actual physical jump', 'Native ABP airborne', 'Native JumpStart/JumpLoop/Land montages', 'Native RunStart/RunStop montages', 'Actual sprint locomotion'])
        finish()


def tick_impl():
    global stage, index, ready, task
    require(time.monotonic()-started < 720., 'Pending capture timeout at '+stage)
    if stage == 'startup':
        if init_world():
            if SKIP_SAMPLES:
                prepare_native()
                stage = 'native'
            else:
                stage = 'setup'
    elif stage == 'setup':
        setup_sample()
        stage = 'settle'
    elif stage == 'settle' and time.monotonic() >= ready:
        sample_checks()
        task = unreal.AutomationLibrary.take_high_res_screenshot(WIDTH, HEIGHT, str(screenshot_file))
        require(task is not None, 'Could not create screenshot task')
        ready, stage = time.monotonic()+45., 'screenshot'
    elif stage == 'screenshot':
        require(time.monotonic() < ready, 'Screenshot timed out')
        if not task.is_task_done() or not screenshot_file.exists():
            return
        data = screenshot_file.read_bytes()
        require(data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) > 10000, 'Invalid screenshot PNG')
        require(struct.unpack('>II', data[16:24]) == (WIDTH, HEIGHT), 'Incorrect screenshot dimensions')
        active.update(file=str(screenshot_file.relative_to(ROOT)), png_sha256=hashlib.sha256(data).hexdigest(),
                      png_bytes=len(data), dimensions=[WIDTH, HEIGHT], screenshot_task_complete=True)
        shots.append(active)
        index += 1
        if index < len(JOBS):
            stage = 'setup'
        elif RUNTIME:
            prepare_native()
            stage = 'native'
        else:
            finish()
    elif stage == 'native':
        observe_native()


def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        cleanup() if closing else tick_impl()
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy = False


try:
    require(editor.get_game_world() is None, 'End existing PIE before pending capture')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    require(levels.load_level('/Game/CityCourier/Maps/L_CharacterProbe'), 'Cannot load lab map')
    handle = unreal.register_slate_post_tick_callback(tick)
    owns_pie = True
    levels.editor_request_begin_play()
except Exception:
    finish(traceback.format_exc())
