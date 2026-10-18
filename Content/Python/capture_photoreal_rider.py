"""Honest, fixed-camera UE PIE rider A/B capture; never saves assets or the map.

Run through Scripts/Run-EditorScript.ps1 -Gui. CC_PORTRAIT_PHASE accepts before,
after, or both (default). Output: Saved/Validation/PhotorealRider/results.json.
The BEFORE asset must already be a deep material/texture snapshot, not merely a
duplicate mesh referring to subsequently edited production materials.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import time
import traceback
import uuid

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Validation/PhotorealRider'
OUT.mkdir(parents=True, exist_ok=True)
ART = '/Game/CityCourier/Art'
MESHES = {
    'before': ART + '/Portrait/Baseline/SK_CourierBefore',
    'after': ART + '/Characters/SK_Courier',
}
PHASE = os.environ.get('CC_PORTRAIT_PHASE', 'both').lower()
PHASES = ('before', 'after') if PHASE == 'both' else (PHASE,)
if any(phase not in MESHES for phase in PHASES):
    raise RuntimeError('CC_PORTRAIT_PHASE must be before, after, or both')
WIDTH, HEIGHT = 1800, 1200
HIDDEN_HEAD_SLOTS = {'M_Helmet', 'M_Rubber', 'M_ReflectiveStrip'}
BONES = ('head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')
PROFILE = {
    'neutral': {'key_kelvin': 6500, 'key': 12., 'fill': 1.2, 'rim': .5, 'sky': .10},
    'dusk': {'key_kelvin': 4400, 'key': 12., 'fill': 1.2, 'rim': .5, 'sky': .10},
}
# Camera coordinates, lights and sampled idle time are identical between phases.
VIEWS = [
    dict(name='head-helmet-neutral', view='head', helmet=True, lighting='neutral'),
    dict(name='head-bare-neutral', view='head', helmet=False, lighting='neutral'),
    dict(name='head-bare-dusk', view='head', helmet=False, lighting='dusk'),
    dict(name='profile-bare-neutral', view='profile', helmet=False, lighting='neutral'),
    dict(name='fullbody-run-sample', view='body', helmet=True, lighting='neutral', clip='A_Run', sample=.19),
    dict(name='fullbody-ride-sample', view='ride', helmet=True, lighting='neutral', clip='A_Ride', sample=.40),
    dict(name='native-gameplay-run', view='body', helmet=True, lighting='neutral', native='run'),
    dict(name='native-gameplay-ride', view='ride', helmet=True, lighting='neutral', native='ride'),
]
if os.environ.get('CC_PORTRAIT_INCLUDE_M2M') == '1':
    for clip, sample in [('A_M2M_Jog', .25), ('A_M2M_Run', .19)]:
        VIEWS.append(dict(name=clip.lower() + '-sample', view='body', helmet=True,
                          lighting='neutral', clip=clip, sample=sample, experiment=True))
if os.environ.get('CC_PORTRAIT_QUICK') == '1':
    VIEWS = [view for view in VIEWS if view['name'] in ('head-bare-neutral', 'head-bare-dusk')]
if os.environ.get('CC_PORTRAIT_VIEWS'):
    requested = {name.strip() for name in os.environ['CC_PORTRAIT_VIEWS'].split(',') if name.strip()}
    unknown = requested - {view['name'] for view in VIEWS}
    if unknown:
        raise RuntimeError('Unknown or quick-filtered CC_PORTRAIT_VIEWS: ' + str(sorted(unknown)))
    VIEWS = [view for view in VIEWS if view['name'] in requested]
JOBS = [dict(view, phase=phase) for view in VIEWS for phase in PHASES]
RUN_ID = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) + '-' + uuid.uuid4().hex[:8]
RUN_OUT = OUT / 'runs' / RUN_ID
RUN_OUT.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
started = time.monotonic()
stage, index, busy, finished = 'startup', 0, False, False
handle, task = None, None
closing, owns_pie, final_error = False, False, None
cleanup_started = None
editor_fixture, editor_fixture_path, editor_fixture_tag = None, None, None
shots, managers, vehicles = [], [], []
mount_baseline_samples = {}
checks = {'section_visibility_api': 'show_material_section(material_id, section_index, show, lod_index)',
          'section_visibility_documentation': 'https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SkinnedMeshComponent?application_version=5.6',
          'no_map_or_asset_save': True, 'native_pose_is_not_single_node': True}


def xyz(vector):
    return [float(getattr(vector, axis)) for axis in ('x', 'y', 'z')]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load(path):
    asset = unreal.load_asset(path)
    require(asset is not None, 'Required capture asset is missing: ' + path)
    return asset


def finalize_report():
    global finished
    if finished:
        return
    finished = True
    report = dict(run_id=RUN_ID, error=final_error, complete=final_error is None and len(shots) == len(JOBS),
                  phase=PHASE, shots=shots, planned_shots=len(JOBS), checks=checks,
                  failed_job=globals().get('active') if final_error else None,
                  dimensions=[WIDTH, HEIGHT], profiles=PROFILE,
                  interpretation='Actual Unreal PIE renders; visual realism requires human review.',
                  exposure='Fixed lab exposure; DOF, motion blur, bloom, vignette and grain disabled.',
                  elapsed_seconds=time.monotonic() - started)
    # Keep independent before/after runs inspectable as well as the latest result.
    for path in (RUN_OUT / 'results.json', OUT / ('results-' + PHASE + '.json'), OUT / 'results.json'):
        path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    if final_error:
        unreal.log_error('PHOTOREAL RIDER CAPTURE FAILED: ' + final_error)
    else:
        unreal.log('PHOTOREAL RIDER CAPTURE COMPLETE: ' + str(OUT / 'results.json'))


def cleanup_tick():
    """Wait for asynchronous EndPIE before touching the exact editor fixture."""
    global final_error, editor_fixture
    if owns_pie and editor.get_game_world() is not None:
        if time.monotonic() - cleanup_started > 30.:
            final_error = (final_error + '\n' if final_error else '') + (
                'EndPIE timed out; editor fixture was not destroyed while PIE remained active: '
                + str(editor_fixture_path))
            checks['editor_fixture_cleanup_complete'] = False
            finalize_report()
        return
    # EndPIE is queued; allow its teardown to complete before editor mutation.
    if owns_pie and time.monotonic() - cleanup_started < .25:
        return
    try:
        if editor_fixture is not None:
            require(editor_fixture.get_path_name() == editor_fixture_path,
                    'Editor fixture reference no longer matches the created actor')
            require(editor_actors.destroy_actor(editor_fixture),
                    'Could not destroy the exact temporary editor scooter fixture')
            editor_fixture = None
            checks['editor_fixture_destroyed'] = True
        checks['editor_fixture_cleanup_complete'] = True
    except Exception:
        final_error = (final_error + '\n' if final_error else '') + traceback.format_exc()
        checks['editor_fixture_cleanup_complete'] = False
    finalize_report()


def finish(error=None):
    global closing, cleanup_started, final_error
    if finished or closing:
        return
    closing, final_error, cleanup_started = True, error, time.monotonic()
    try:
        if owns_pie:
            game_world = editor.get_game_world()
            if game_world:
                unreal.SystemLibrary.execute_console_command(game_world, 'Input.-action IA_Sprint')
            levels.editor_request_end_play()
    except Exception:
        final_error = (final_error + '\n' if final_error else '') + traceback.format_exc()
    if handle is None:
        cleanup_tick()


def prepare_native_ride_fixture():
    """Editor-only, unsaved actor; PIE duplicates it as an ordinary gameplay bike."""
    global editor_fixture, editor_fixture_path, editor_fixture_tag
    if not any(job.get('native') == 'ride' for job in JOBS):
        checks['editor_fixture_created'] = False
        return
    existing = [actor for actor in editor_actors.get_all_level_actors()
                if isinstance(actor, unreal.ScooterPawn)]
    require(len(existing) <= 1, 'Lab has multiple existing editor scooter fixtures')
    if existing:
        checks['editor_fixture_created'] = False
        checks['existing_scooter_editor_path'] = existing[0].get_path_name()
        return
    cls = unreal.load_class(None, '/Game/CityCourier/Blueprints/BP_Scooter.BP_Scooter_C')
    require(cls is not None, 'Missing BP_Scooter class for native ride fixture')
    # bTransient=False is intentional: a normal editor actor participates in
    # PIE world duplication. The map is never saved, and this instance is removed.
    editor_fixture = editor_actors.spawn_actor_from_class(cls, unreal.Vector(2000, 2000, 52))
    require(editor_fixture is not None, 'Could not spawn temporary editor BP_Scooter fixture')
    editor_fixture_path = editor_fixture.get_path_name()
    editor_fixture_tag = 'CC_PortraitFixture_' + uuid.uuid4().hex
    editor_fixture.tags = list(editor_fixture.tags) + [unreal.Name(editor_fixture_tag)]
    editor_fixture.set_actor_hidden_in_game(True)
    checks.update(editor_fixture_created=True, editor_fixture_destroyed=False,
                  editor_fixture_cleanup_complete=False,
                  editor_fixture_path=editor_fixture_path, editor_fixture_tag=editor_fixture_tag)


def assert_no_ambient():
    require(not any(m.get_active_vehicle_count() or m.get_active_pedestrian_count()
                    for m in managers), 'Ambient population entered the portrait lab')


def configure_animation_tick(component):
    # A Slate callback can run before the PIE viewport has ever rendered this
    # actor. Pose verification must not depend on recently-rendered visibility.
    component.set_editor_property('visibility_based_anim_tick_option',
                                 unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    component.set_editor_property('enable_update_rate_optimizations', False)
    component.set_editor_property('pause_anims', False)
    component.set_editor_property('no_skeleton_update', False)
    component.set_component_tick_enabled(True)


def head_sections(component, helmet):
    mesh = component.get_skeletal_mesh_asset()
    slots = mesh.get_editor_property('materials')
    names = [str(slot.get_editor_property('material_slot_name')) for slot in slots]
    require(HIDDEN_HEAD_SLOTS.issubset(names), 'Helmet slot mapping changed: ' + str(names))
    require(hasattr(component, 'show_material_section') and
            hasattr(component, 'is_material_section_shown'),
            'This UE build does not expose the required section-visibility API')
    component.set_forced_lod(1)  # UE forced LOD uses 1 for LOD0.
    component.show_all_material_sections(0)
    hidden = []
    if not helmet:
        for material_id, name in enumerate(names):
            if name in HIDDEN_HEAD_SLOTS:
                # Verified against UE5.6 SkinnedMeshComponent.cpp: INDEX_NONE
                # bypasses LODMaterialMap remapping and addresses MaterialID.
                component.show_material_section(material_id, -1, False, 0)
                require(not component.is_material_section_shown(material_id, 0),
                        'UE did not hide helmet material ' + name)
                hidden.append(dict(material_id=material_id, slot=name))
    return dict(forced_lod=0, hidden_materials=hidden, slots=names)


def lighting(name):
    profile = PROFILE[name]
    for tag, key in [('Key', 'key'), ('Fill', 'fill'), ('Rim', 'rim')]:
        light = parts[tag].light_component
        light.set_editor_property('intensity', profile[key])
        light.set_editor_property('use_temperature', True)
        light.set_editor_property('temperature', profile['key_kelvin'] if tag == 'Key' else 6500.)
        light.set_editor_property('source_width', 90. if tag == 'Key' else 150.)
        light.set_editor_property('source_height', 90. if tag == 'Key' else 150.)
    parts['Sky'].light_component.set_editor_property('intensity', profile['sky'])


def position_camera(view, origin):
    # Fixed framing in centimetres. Head views show forehead, chin and shoulders.
    offsets = {
        'head': ((38, -142, 163), (0, 0, 160), 23.),
        'profile': ((148, -12, 163), (0, 0, 160), 23.),
        'body': ((110, -440, 145), (0, 0, 91), 38.),
        'ride': ((220, -360, 150), (0, 0, 99), 42.),
    }
    offset, target_offset, fov = offsets[view]
    pos = origin + unreal.Vector(*offset)
    target = origin + unreal.Vector(*target_offset)
    camera.set_actor_location(pos, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, target), False)
    camera.camera_component.set_field_of_view(fov)
    pc.set_view_target_with_blend(camera, 0)
    return dict(position_cm=xyz(pos), target_cm=xyz(target), fov_degrees=fov)


def init_world():
    global world, pc, rider, bike, camera, parts, managers, vehicles, meshes, anim_class, baseline_bones
    world = editor.get_game_world()
    if not world:
        return False
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if not pc or not pc.get_controlled_pawn():
        return False
    rider = pc.get_controlled_pawn()
    require(isinstance(rider, unreal.CourierCharacter), 'PIE did not possess the gameplay CourierCharacter')
    checks['initial_world_paused'] = unreal.GameplayStatics.is_game_paused(world)
    if checks['initial_world_paused']:
        require(unreal.GameplayStatics.set_game_paused(world, False), 'Could not unpause the capture PIE world')
    bikes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ScooterPawn)
    require(len(bikes) <= 1, 'Expected at most one gameplay scooter, found ' + str(len(bikes)))
    bike = bikes[0] if bikes else None
    if any(job.get('native') == 'ride' for job in JOBS):
        require(bike is not None, 'Native ride capture needs a BP_Scooter fixture in L_CharacterProbe; quick portraits do not')
        if editor_fixture_tag:
            require(editor_fixture_tag in [str(tag) for tag in bike.tags],
                    'PIE scooter is not the duplicate of this capture fixture')
        checks['native_scooter_pie_path'] = bike.get_path_name()
    all_actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))
    parts = {str(tag).removeprefix('CC_Probe_'): actor for actor in all_actors
             for tag in actor.tags if str(tag).startswith('CC_Probe_')}
    required = {'Before', 'After', 'Camera', 'Floor', 'Key', 'Fill', 'Rim', 'Sky', 'Exposure'}
    require(required.issubset(parts), 'L_CharacterProbe is missing tagged fixtures: ' + str(required - parts.keys()))
    camera = parts['Camera']
    camera.camera_component.set_editor_property('post_process_blend_weight', 0.)
    manager_class = getattr(unreal, 'AmbientTrafficManager', None)
    managers = list(unreal.GameplayStatics.get_all_actors_of_class(world, manager_class)) if manager_class else []
    for manager in managers:
        manager.set_actor_tick_enabled(False)
    for class_name in ('AmbientVehicle', 'AmbientPedestrian'):
        cls = getattr(unreal, class_name, None)
        if cls:
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
                actor.set_population_active(False)
                vehicles.append(actor)
    # Hide *all* mesh-bearing background actors, including comparison busts and
    # runtime map populations. Only selected character/floor/scooter is revealed.
    visible = {parts[tag] for tag in ('Floor', 'Key', 'Fill', 'Rim', 'Sky', 'Exposure', 'Camera')}
    for actor in all_actors:
        if actor not in visible and actor.get_components_by_class(unreal.MeshComponent):
            actor.set_actor_hidden_in_game(True)
    rider.disable_input(pc)
    pc.set_ignore_look_input(True)
    pc.set_ignore_move_input(True)
    rider.character_movement.stop_movement_immediately()
    rider.set_actor_enable_collision(False)
    rider.set_actor_location(unreal.Vector(2000, 2000, 90), False, True)
    if bike:
        bike.set_actor_tick_enabled(False)
        bike.set_actor_enable_collision(False)
        bike.get_movement_component().set_component_tick_enabled(False)
    meshes = {phase: load(MESHES[phase]) for phase in set(PHASES) | {'before'}}
    checks['skeletal_mesh_package_sha256'] = {
        phase: hashlib.sha256((ROOT / 'Content' / (path.removeprefix('/Game/') + '.uasset')).read_bytes()).hexdigest()
        for phase, path in MESHES.items() if phase in meshes}
    baseline_component = parts['Before'].get_component_by_class(unreal.SkeletalMeshComponent)
    baseline_component.set_skeletal_mesh_asset(meshes['before'])
    baseline_bones = [(str(baseline_component.get_bone_name(i)),
                       str(baseline_component.get_parent_bone(baseline_component.get_bone_name(i))))
                      for i in range(baseline_component.get_num_bones())]
    checks['baseline_bones_and_parents'] = baseline_bones
    checks['baseline_ue_bone_count'] = len(baseline_bones)
    if 'after' in meshes:
        require(meshes['before'] != meshes['after'], 'A/B meshes must be distinct assets')
        require(meshes['before'].get_editor_property('skeleton') == meshes['after'].get_editor_property('skeleton'),
                'A/B must retain the identical production skeleton')
    anim_class = unreal.load_class(None, '/Game/CityCourier/Blueprints/ABP_Courier.ABP_Courier_C')
    require(anim_class is not None, 'Missing production ABP_Courier animation class')
    load('/Game/CityCourier/Input/IA_Sprint')
    # The inherited map fixes exposure; explicitly remove optical hiding effects.
    pp = parts['Exposure'].get_editor_property('settings')
    for property_name, value in [('bloom_intensity', 0.), ('motion_blur_amount', 0.),
                                 ('vignette_intensity', 0.), ('film_grain_intensity', 0.)]:
        pp.set_editor_property('override_' + property_name, True)
        pp.set_editor_property(property_name, value)
    parts['Exposure'].set_editor_property('settings', pp)
    for command in ['t.MaxFPS 30', 'ShowFlag.Game 1', 'ShowFlag.HUD 0', 'r.ScreenPercentage 100',
                    'r.DepthOfFieldQuality 0', 'r.MotionBlurQuality 0', 'r.Streaming.PoolSize 2400'] + [
                        'sg.' + name + 'Quality 3' for name in ('ViewDistance', 'Shadow', 'GlobalIllumination',
                                                              'Reflection', 'Texture', 'Effects', 'Foliage', 'PostProcess')]:
        unreal.SystemLibrary.execute_console_command(world, command)
    # Quality presets can reset DOF/motion blur; apply these after the presets.
    for command in ('r.DepthOfFieldQuality 0', 'r.MotionBlurQuality 0'):
        unreal.SystemLibrary.execute_console_command(world, command)
    checks['ambient_managers_disabled'] = len(managers)
    checks['ambient_entities_deactivated'] = len(vehicles)
    assert_no_ambient()
    return True


def setup_job():
    global active, component, target_actor, ready, native_start, native_bones, native_origin, screenshot_file
    active = dict(JOBS[index], run_id=RUN_ID)
    for tag in ('Before', 'After'):
        parts[tag].set_actor_hidden_in_game(True)
    rider.set_actor_hidden_in_game(True)
    if bike:
        bike.set_actor_hidden_in_game(True)
    unreal.SystemLibrary.execute_console_command(world, 'Input.-action IA_Sprint')
    if bike and pc.get_controlled_pawn() == bike:
        require(bike.try_dismount(), 'Gameplay scooter could not dismount between capture jobs')
    if bike:
        bike.set_actor_enable_collision(False)
        bike.set_actor_location(unreal.Vector(2000, 2000, 52), False, True)
    rider.disable_input(pc)
    rider.mesh.set_component_tick_enabled(True)
    rider.character_movement.set_component_tick_enabled(True)
    rider.character_movement.stop_movement_immediately()
    rider.set_actor_enable_collision(False)
    lighting(active['lighting'])
    mesh = meshes[active['phase']]
    if 'native' in active:
        target_actor = rider
        component = rider.mesh
        component.set_skeletal_mesh_asset(mesh)
        component.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT)
        component.set_anim_instance_class(anim_class)
        rider.set_actor_location(unreal.Vector(0, 0, 90), False, True)
        rider.set_actor_rotation(unreal.Rotator(pitch=0., yaw=270., roll=0.), False)
        rider.set_actor_hidden_in_game(False)
        rider.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
        # The floor remains physical; only actor collision was disabled while
        # teleporting. Force movement input is the same route used by validation.
        rider.set_actor_enable_collision(True)
        if active['native'] == 'run':
            # UE5.6 EnhancedInputModule.cpp exposes this simulation command.
            # It fires the production SprintStart binding and preserves the
            # real action component's movement limits every gameplay tick.
            rider.enable_input(pc)
            unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Sprint true')
            active['native_input'] = 'Enhanced Input forced IA_Sprint + force AddMovementInput'
        else:
            bike.set_actor_location(unreal.Vector(0, 0, 52), False, True)
            bike.set_actor_rotation(unreal.Rotator(pitch=0., yaw=270., roll=0.), False)
            bike.set_actor_enable_collision(True)
            require(bike.try_mount(rider), 'Real gameplay scooter rejected the rider mount')
            bike.disable_input(pc)
            bike.set_drive_input(0, 0, True)
            bike.set_actor_hidden_in_game(False)
        native_start = time.monotonic()
        native_origin = rider.get_actor_location()
        native_bones = None
        ready = native_start + 1.3
    else:
        target_actor = parts['Before' if active['phase'] == 'before' else 'After']
        component = target_actor.get_component_by_class(unreal.SkeletalMeshComponent)
        component.set_skeletal_mesh_asset(mesh)
        configure_animation_tick(component)
        component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        target_actor.set_actor_location(unreal.Vector(0, 0, 0), False, True)
        target_actor.set_actor_rotation(unreal.Rotator(pitch=0., yaw=180., roll=0.), False)
        target_actor.set_actor_hidden_in_game(False)
        folder = '/Experiments/Mesh2Motion/' if active.get('experiment') else '/Characters/'
        sequence = load(ART + folder + active.get('clip', 'A_Idle'))
        require(sequence.get_editor_property('skeleton') == mesh.get_editor_property('skeleton'),
                'Animation skeleton does not match ' + MESHES[active['phase']])
        position = active.get('sample', .70) % sequence.get_play_length()
        component.play_animation(sequence, True)
        component.set_play_rate(0.)
        component.set_position(position, False)
        active.update(sequence=sequence.get_path_name(), sample_seconds=position,
                      animation_length_seconds=sequence.get_play_length())
        active['camera'] = position_camera(active['view'], unreal.Vector(0, 0, 0))
        ready = time.monotonic() + 3.
    active['sections'] = head_sections(component, active['helmet'])
    configure_animation_tick(component)
    active['mesh'] = mesh.get_path_name()
    active['material_assets'] = [component.get_material(i).get_path_name() if component.get_material(i) else None
                                 for i in range(component.get_num_materials())]
    active['skeleton'] = mesh.get_editor_property('skeleton').get_path_name()
    active['bone_count'] = component.get_num_bones()
    bones = [(str(component.get_bone_name(i)), str(component.get_parent_bone(component.get_bone_name(i))))
             for i in range(component.get_num_bones())]
    require(bones == baseline_bones, 'UE imported bone roster/hierarchy differs from the baseline')
    active['ue_bone_roster_matches_baseline'] = True
    screenshot_file = OUT / active['phase'] / (active['name'] + '.png')
    screenshot_file.parent.mkdir(parents=True, exist_ok=True)
    # Eliminate stale screenshot success if a previous capture used this filename.
    if screenshot_file.exists():
        screenshot_file.unlink()


def runtime_checks():
    assert_no_ambient()
    active['animation_instance'] = component.get_anim_instance().get_class().get_name()
    active['bones_world_cm'] = {bone: xyz(component.get_socket_location(bone)) for bone in BONES}
    rotation = target_actor.get_actor_rotation()
    active['actor_rotation_pyr_degrees'] = [rotation.pitch, rotation.yaw, rotation.roll]
    require(active['bones_world_cm']['head'][2] >
            min(active['bones_world_cm']['foot_l'][2], active['bones_world_cm']['foot_r'][2]) + 50.,
            'Rider is not upright in world space; inspect actor_rotation_pyr_degrees')
    require(pc.get_view_target() == camera, 'Gameplay view target changed away from the inspection camera')
    if 'native' in active:
        anim = component.get_anim_instance()
        require(component.get_editor_property('animation_mode') == unreal.AnimationMode.ANIMATION_BLUEPRINT,
                'Native gameplay capture unexpectedly switched to single-node playback')
        mode = active['native']
        speed = float(anim.get_editor_property('ground_speed'))
        animation_index = int(anim.get_editor_property('animation_index'))
        active.update(native_ground_speed_cm_s=speed, native_animation_index=animation_index,
                      native_b_riding=bool(anim.get_editor_property('riding')))
        require(animation_index == (2 if mode == 'run' else 3), 'Native gameplay selected incorrect clip index')
        clips = list(anim.get_editor_property('clips'))
        active['native_clip'] = clips[animation_index].get_path_name()
        if mode == 'run':
            require(speed > 500., 'Native gameplay run did not reach the run threshold')
            require(active['native_distance_cm'] > 100., 'Native rider did not actually travel')
            require(active['native_pose_motion_cm'] > .1, 'Native animation bones did not change while moving')
        else:
            require(active['native_b_riding'] and rider.get_attach_parent_actor() == bike,
                    'Ride screenshot is not an actual mounted gameplay rider')
            sequence = load(active['native_clip'])
            options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED,
                                                       optional_skeletal_mesh=meshes['before'])
            reference = sequence.get_anim_pose_at_time(.4 % sequence.get_play_length(), options)
            verify_bones = ('root', 'pelvis') + BONES
            # A_Ride is authored as a static seated hold; verify that assumption
            # before comparing the native instance (whose private clock is not exposed).
            static_error = 0.
            for sample_time in (0., sequence.get_play_length() * .5):
                sample = sequence.get_anim_pose_at_time(sample_time, options)
                static_error = max(static_error, max(
                    (sample.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation -
                     reference.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation).length()
                    for bone in verify_bones))
            active['native_ride_clip_static_error_cm'] = static_error
            require(static_error < .1, 'A_Ride is no longer a static hold; exact native clock sampling is required')
            pose_error = max(
                (component.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation -
                 reference.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation).length()
                for bone in verify_bones)
            active['native_ride_baseline_compressed_pose_error_cm'] = pose_error
            require(pose_error < .1, 'Native mounted pose differs from compressed baseline clip: ' + str(pose_error))
            anchors = {c.get_name(): c for c in bike.get_components_by_class(unreal.SceneComponent)}
            active['native_mount_component_relative_location_cm'] = xyz(component.get_editor_property('relative_location'))
            active['native_mount_actor_relative_location_cm'] = xyz(rider.root_component.get_editor_property('relative_location'))
            active['native_mount_chain'] = ('ScooterPawn: actor relative = RiderSeat+(0,0,9); '
                                            'CourierArtCharacter.Tick: mesh relative = (14,0,-32-actorRelativeZ). '
                                            'Existing art/gameplay offsets are measured, not corrected.')
            component_world = component.get_world_transform()
            bike_transform = bike.get_actor_transform()
            mounted = {bone: xyz(unreal.MathLibrary.inverse_transform_location(
                bike_transform, component.get_socket_location(bone))) for bone in verify_bones}
            active['native_bones_in_scooter_space_cm'] = mounted
            reference_world = {bone: unreal.MathLibrary.transform_location(
                component_world, reference.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation)
                for bone in verify_bones}
            baseline_world_error = max((component.get_socket_location(bone) - reference_world[bone]).length()
                                       for bone in verify_bones)
            active['native_ride_baseline_world_error_cm'] = baseline_world_error
            require(baseline_world_error < .1, 'Mounted world bones differ from baseline transformed through the real mesh attachment')
            errors, residuals = {}, {}
            for bone, anchor, offset_z, legacy_tolerance in [
                    ('pelvis', 'RiderSeat', 10., 10.), ('hand_l', 'LeftGrip', 0., 12.),
                    ('hand_r', 'RightGrip', 0., 12.), ('foot_l', 'LeftPedal', 0., 12.),
                    ('foot_r', 'RightPedal', 0., 12.)]:
                point = anchors[anchor].get_world_location()
                point += unreal.Vector(0, 0, offset_z)
                residual = component.get_socket_location(bone) - point
                expected_residual = reference_world[bone] - point
                errors[bone] = residual.length()
                residuals[bone] = dict(anchor=anchor, reference_offset_z_cm=offset_z,
                                       anchor_world_cm=xyz(point), bone_world_cm=xyz(component.get_socket_location(bone)),
                                       actual_offset_cm=xyz(residual), baseline_offset_cm=xyz(expected_residual),
                                       baseline_delta_cm=(residual - expected_residual).length(),
                                       legacy_absolute_tolerance_cm=legacy_tolerance,
                                       meets_legacy_absolute_tolerance=residual.length() < legacy_tolerance)
                require((residual - expected_residual).length() < .1,
                        'New mounted alignment changed from baseline at ' + anchor)
            active['native_anchor_error_cm'] = errors
            active['native_anchor_residuals'] = residuals
            active['absolute_anchor_alignment_passed'] = all(row['meets_legacy_absolute_tolerance'] for row in residuals.values())
            active['native_ride_validation_basis'] = 'Unchanged baseline pose and mount-space alignment; absolute residuals remain reported separately'
            if active['phase'] == 'before':
                mount_baseline_samples[active['lighting']] = mounted
            elif active['lighting'] in mount_baseline_samples:
                previous = mount_baseline_samples[active['lighting']]
                deltas = {bone: (unreal.Vector(*mounted[bone]) - unreal.Vector(*previous[bone])).length()
                          for bone in verify_bones}
                active['native_ride_before_after_mount_delta_cm'] = deltas
                require(max(deltas.values()) < .1, 'New mesh changed native mounted bone positions from the actual before capture')
    else:
        sequence = load(active['sequence'])
        options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED,
                                                   optional_skeletal_mesh=component.get_skeletal_mesh_asset())
        pose = sequence.get_anim_pose_at_time(component.get_position(), options)
        # AnimPose.h explicitly defines WORLD as component space, matching
        # RTS_COMPONENT here; actor camera/world rotation must not be applied.
        diagnostics = {}
        for bone in BONES:
            actual = component.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation
            expected = pose.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation
            reference = pose.get_ref_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation
            diagnostics[bone] = dict(actual_component_cm=xyz(actual), expected_component_cm=xyz(expected),
                                     ref_component_cm=xyz(reference), error_cm=(actual - expected).length(),
                                     distance_to_ref_cm=(actual - reference).length())
        active['pose_diagnostics'] = diagnostics
        active['tick_diagnostics'] = dict(world_paused=unreal.GameplayStatics.is_game_paused(world),
                                        component_tick_enabled=component.is_component_tick_enabled(),
                                        pause_anims=component.get_editor_property('pause_anims'),
                                        no_skeleton_update=component.get_editor_property('no_skeleton_update'),
                                        visibility_option=str(component.get_editor_property('visibility_based_anim_tick_option')))
        error = max(row['error_cm'] for row in diagnostics.values())
        active['rendered_pose_error_cm'] = error
        active['rendered_sample_seconds'] = component.get_position()
        if error >= .1:
            unreal.log_error('PORTRAIT POSE DIAGNOSTICS ' + json.dumps(active))
        require(error < .1, 'Rendered sample disagrees with authored clip by ' + str(error) + ' cm; see failed_job.pose_diagnostics')
        root_offset = component.get_socket_transform('root', unreal.RelativeTransformSpace.RTS_COMPONENT).translation
        authored_root = pose.get_bone_pose('root', unreal.AnimPoseSpaces.WORLD).translation
        root_error = (root_offset - authored_root).length()
        active['root_error_cm'] = root_error
        active['authored_root_offset_cm'] = xyz(authored_root)
        active['rendered_root_offset_cm'] = xyz(root_offset)
        require(root_error < .1, 'Rendered root differs from authored root: ' + str(root_error) + ' cm')


def tick_impl(delta):
    global stage, index, task, ready, native_bones
    require(time.monotonic() - started < 720., 'Portrait capture timed out at stage ' + stage)
    if stage == 'startup':
        if init_world():
            stage = 'setup'
    elif stage == 'setup':
        setup_job()
        stage = 'settle'
    elif stage == 'settle':
        if active.get('native') == 'run':
            rider.add_movement_input(unreal.Vector(0, -1, 0), 1., True)
            if native_bones is None and time.monotonic() > native_start + .5:
                native_bones = {bone: component.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation for bone in BONES}
        if time.monotonic() >= ready:
            if 'native' in active:
                active['native_distance_cm'] = (rider.get_actor_location() - native_origin).length()
                if active['native'] == 'run':
                    require(native_bones is not None, 'Native run did not produce an intermediate pose')
                    active['native_pose_motion_cm'] = max((component.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation - native_bones[bone]).length() for bone in BONES)
                # Freeze the already evaluated *native* pose for a sharp capture.
                component.set_component_tick_enabled(False)
                rider.character_movement.set_component_tick_enabled(False)
                active['native_travel_end_actor_cm'] = xyz(rider.get_actor_location())
                if active['native'] == 'run':
                    # Bring the evaluated pose into the fixed lighting fixture;
                    # recorded travel above remains actual gameplay movement.
                    rider.set_actor_location(unreal.Vector(0, 0, 88), False, True)
                    origin = unreal.Vector(0, 0, 0)
                else:
                    origin = unreal.Vector(0, 0, 0)
                active['camera'] = position_camera(active['view'], origin)
            runtime_checks()
            ready = time.monotonic() + 1.
            stage = 'capture'
    elif stage == 'capture' and time.monotonic() >= ready:
        task = unreal.AutomationLibrary.take_high_res_screenshot(WIDTH, HEIGHT, str(screenshot_file))
        require(task is not None, 'UE screenshot task creation failed')
        ready = time.monotonic() + 45.
        stage = 'screenshot'
    elif stage == 'screenshot':
        require(time.monotonic() < ready, 'UE screenshot task/file completion timed out: ' + str(screenshot_file))
        if not task.is_task_done() or not screenshot_file.exists():
            return
        data = screenshot_file.read_bytes()
        require(data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) > 10000, 'Screenshot is missing or invalid')
        require(struct.unpack('>II', data[16:24]) == (WIDTH, HEIGHT), 'Screenshot has unexpected dimensions')
        assert_no_ambient()
        active.update(file=str(screenshot_file.relative_to(ROOT)), png_sha256=hashlib.sha256(data).hexdigest(),
                      screenshot_task_complete=True, png_bytes=len(data))
        archived = RUN_OUT / active['phase'] / screenshot_file.name
        archived.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(screenshot_file, archived)
        active['run_archive_file'] = str(archived.relative_to(ROOT))
        shots.append(active)
        index += 1
        if index == len(JOBS):
            finish()
        else:
            stage = 'setup'


def tick(delta):
    global busy
    if busy or finished:
        return
    busy = True
    try:
        if closing:
            cleanup_tick()
        else:
            tick_impl(delta)
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy = False


try:
    require(editor.get_game_world() is None, 'End the existing PIE session before starting portrait capture')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    require(levels.load_level('/Game/CityCourier/Maps/L_CharacterProbe'), 'Could not load L_CharacterProbe')
    prepare_native_ride_fixture()
    handle = unreal.register_slate_post_tick_callback(tick)
    owns_pie = True
    levels.editor_request_begin_play()
except Exception:
    finish(traceback.format_exc())
