"""Native PIE idle portraits and moving-jump telemetry; never saves a map."""
import hashlib
import json
import math
import os
import time
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
pose_capture = os.environ.get('CC_JUMP_POSES', '0') == '1'
pose_phase = os.environ.get('CC_JUMP_PHASE', 'Before')
assert pose_phase in ('Before', 'After', 'Stationary')
stationary = pose_capture and pose_phase == 'Stationary'
out = root/('Saved/JumpPoses/'+pose_phase if pose_capture else 'Saved/RelaxedIdle/UE')
out.mkdir(parents=True, exist_ok=True)
run_out = out/'runs'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
run_out.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
protected = list((root/'Content/CityCourier/Art/Characters').glob('*.uasset'))
protected += list((root/'Content/CityCourier/Blueprints').glob('*.uasset'))
protected += [root/'Content/CityCourier/Maps/L_CharacterProbe.umap']
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
capture_hash = digest(Path(__file__))
before = {str(p.relative_to(root)): digest(p) for p in protected}
started, stage, busy, closing = time.monotonic(), 'startup', False, False
rows, shots = [], []
seen_air, seen_land, final_error = False, False, None
pose_index, paused_until = 0, None
pose_times = (.04, .12, .20, .30, .43, .56, .70, .84, .98)


def finish(error=None):
    global closing, cleanup_started, final_error
    final_error, closing, cleanup_started = error, True, time.monotonic()
    world = editor.get_game_world()
    if world:
        unreal.SystemLibrary.execute_console_command(world, 'Input.-action IA_Jump')
        levels.editor_request_end_play()


def cleanup():
    if editor.get_game_world() and time.monotonic()-cleanup_started < 20:
        return
    error = final_error
    changed = [p for p, h in before.items() if digest(root/p) != h]
    if changed or editor.get_game_world():
        error = str(error)+' teardown/protected packages: '+str(changed)
    report = {'complete': error is None, 'error': error, 'input': 'Production Enhanced Input',
              'paused_for_pose_screenshots': pose_capture, 'stationary_jump': stationary,
              'single_node_playback': False, 'map_or_asset_saved': False,
              'protected_packages_unchanged': not changed, 'shots': shots,
              'native_frames': rows, 'visual_acceptance': 'Manual image review required',
              'code_sha256': capture_hash}
    if rows:
        jump = [r for r in rows if r['stage'] == 'jump']
        landed = [r for r in jump if r['landed']]
        report['jump' if stationary else 'moving_jump'] = {'saw_airborne': seen_air, 'saw_landing': seen_land,
            'minimum_landing_speed_cm_s': min((r['speed'] for r in landed), default=None),
            'observed_movement_clips': sorted({r['movement_clip'] for r in jump if r['movement_clip']}),
            'height_above_start_cm': max((r['actor_cm'][2]-jump_origin for r in jump), default=0)}
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    (run_out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    (unreal.log_error if error else unreal.log)('LOCOMOTION CAPTURE '+str(error or 'COMPLETE'))


def shot(name):
    anim = rider.mesh.get_anim_instance()
    montage = anim.get_current_active_montage()
    shots.append({'name': name, 'actor_z': rider.get_actor_location().z,
        'vertical_speed': rider.get_velocity().z,
        'montage': montage.get_first_anim_reference().get_name() if montage else None,
        'montage_seconds': anim.montage_get_position(montage) if montage else None,
        'clips': [c.get_name() for c in anim.get_editor_property('clips')],
        'file': str((run_out/(name+'00000.png')).relative_to(root))})
    unreal.SystemLibrary.execute_console_command(world, 'Shot filename='+str(run_out/(name+'.png'))+' nosuffix')


def inject(name, value=unreal.Vector(1, 0, 0)):
    inputs.inject_input_vector_for_action(actions[name], value, [], [])


def impl(delta):
    global world, pc, rider, camera, inputs, actions, ready, stage, seen_air, seen_land, jump_origin, jump_time, pose_index, paused_until
    if time.monotonic()-started > 180:
        raise RuntimeError('PIE movement capture timed out')
    world = editor.get_game_world()
    if not world:
        return
    now = time.monotonic()
    if paused_until is not None:
        if now < paused_until:
            return
        unreal.GameplayStatics.set_game_paused(world, False)
        paused_until = None
    if stage == 'startup':
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        rider = pc.get_controlled_pawn()
        assert isinstance(rider, unreal.CourierCharacter)
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
        parts = {str(t).removeprefix('CC_Probe_'): a for a in actors for t in a.tags if str(t).startswith('CC_Probe_')}
        camera = parts['Camera']
        for actor in actors:
            if actor.get_components_by_class(unreal.MeshComponent) and actor not in (rider, parts['Floor']):
                actor.set_actor_hidden_in_game(True)
            if isinstance(actor, unreal.AmbientTrafficManager):
                actor.set_actor_tick_enabled(False)
            if hasattr(actor, 'set_population_active'):
                actor.set_population_active(False)
        rider.set_actor_hidden_in_game(False)
        rider.set_actor_location(unreal.Vector(0, 0, 90), False, True)
        rider.set_actor_rotation(unreal.Rotator(yaw=270), False)
        rider.character_movement.stop_movement_immediately()
        rider.mesh.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        assert rider.mesh.get_editor_property('animation_mode') == unreal.AnimationMode.ANIMATION_BLUEPRINT
        clips = rider.mesh.get_anim_instance().get_editor_property('clips')
        assert clips[0].get_name() == 'A_IdleRelaxed'
        inputs = next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
        actions = {n: unreal.load_asset('/Game/CityCourier/Input/IA_'+n) for n in ('Move', 'Sprint')}
        pc.set_control_rotation(unreal.Rotator(yaw=270))
        pc.set_view_target_with_blend(camera, 0)
        for cmd in ('t.MaxFPS 60', 'r.ScreenPercentage 100', 'r.MotionBlurQuality 0'):
            unreal.SystemLibrary.execute_console_command(world, cmd)
        stage, ready = 'warmup', now+6
    pos = rider.get_actor_location()
    offset = (unreal.Vector(100, -550, 70) if stationary else unreal.Vector(550, -35, 70)) if pose_capture else unreal.Vector(330, 0, 35) if stage == 'side' else unreal.Vector(160, -340, 45)
    camera.set_actor_location(pos+offset, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), pos+unreal.Vector(0, 0, 0)), False)
    if stage == 'warmup' and now >= ready:
        shot('native-idle-front')
        stage, ready = 'side', now+2
    elif stage == 'side' and now >= ready:
        shot('native-idle-side')
        if stationary:
            stage, ready = 'jump-ready', now+1
        else:
            stage, ready = 'jog', now+1
    elif stage == 'jump-ready' and now >= ready:
        jump_origin = pos.z
        jump_time = unreal.GameplayStatics.get_time_seconds(world)
        unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Jump true')
        stage, ready = 'jump', now+1.6
    elif stage in ('jog', 'sprint', 'jump'):
        if not stationary:
            inject('Move', unreal.Vector(0, 1, 0))
        if not stationary and stage != 'jog':
            inject('Sprint')
        anim = rider.mesh.get_anim_instance()
        montage = anim.get_current_active_montage()
        clip = montage.get_first_anim_reference() if montage else None
        airborne = rider.character_movement.is_falling()
        if stage == 'jump':
            seen_air |= airborne
            seen_land |= seen_air and not airborne
        speed = rider.get_velocity().length2d()
        rows.append({'stage': stage, 'wall_seconds': now-started, 'tick_delta_s': delta,
            'actor_cm': [pos.x, pos.y, pos.z], 'speed': speed, 'airborne': airborne, 'landed': seen_land,
            'movement_clip': clip.get_name() if clip else None, 'index': anim.get_editor_property('animation_index'),
            'feet_component_cm': {n: [v.x, v.y, v.z] for n in ('foot_l', 'foot_r')
                for v in [rider.mesh.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_COMPONENT).translation]}})
        if stage == 'jog' and now >= ready:
            assert speed > 390
            stage, ready = 'sprint', now+1
        elif stage == 'sprint' and now >= ready:
            assert speed > 650
            jump_origin = pos.z
            jump_time = unreal.GameplayStatics.get_time_seconds(world)
            unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Jump true')
            stage, ready = 'jump', now+1.6
        elif stage == 'jump' and (unreal.GameplayStatics.get_time_seconds(world)-jump_time > 1.6 if pose_capture else now >= ready):
            assert seen_air and seen_land, 'No physical running jump/landing'
            landed = [r for r in rows if r['stage'] == 'jump' and r['landed']]
            assert stationary or min(r['speed'] for r in landed) > 600, 'Landing lost sprint momentum'
            finish()
        if pose_capture and stage == 'jump' and not closing and pose_index < len(pose_times):
            elapsed = unreal.GameplayStatics.get_time_seconds(world)-jump_time
            if elapsed >= pose_times[pose_index]:
                unreal.GameplayStatics.set_game_paused(world, True)
                shot('jump-%02d-%.3f' % (pose_index, elapsed))
                pose_index += 1
                paused_until = now+1.


def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        cleanup() if closing else impl(delta)
    except Exception:
        error = traceback.format_exc()
        unreal.log_error(error)
        if closing:
            (out/'cleanup-error.txt').write_text(error, encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        else:
            finish(error)
    finally:
        busy = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/CityCourier/Maps/L_CharacterProbe')
handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
