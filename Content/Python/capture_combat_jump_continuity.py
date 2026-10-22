"""Production-input PIE sprint punches and phased jump; saves no game assets."""
import hashlib
import json
import time
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/Validation/CombatJump'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
protected = list((root/'Content/CityCourier/Art/Characters').glob('*.uasset'))
protected += list((root/'Content/CityCourier/Animation/Epic').rglob('*.uasset'))
protected += [root/'Content/CityCourier/Maps/L_CharacterProbe.umap']
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {p: digest(p) for p in protected}
capture_hash = digest(root/'Content/Python/capture_combat_jump_continuity.py')
rows, shots = [], []
started, stage, busy, closing = time.monotonic(), 'startup', False, False
error = None
shot_names = set()

def finish(failure=None):
    global closing, error
    error, closing = failure, True
    if editor.get_game_world():
        levels.editor_request_end_play()

def cleanup():
    if editor.get_game_world():
        return
    changed = [str(p.relative_to(root)) for p, h in before.items() if digest(p) != h]
    for entry in shots:
        path = root/entry['file']
        if not path.exists():
            matches = list(out.glob(path.stem+'*.png'))
            if matches:
                entry['file'] = str(matches[0].relative_to(root))
    report = {'complete': error is None and not changed, 'error': error,
              'protected_packages_changed': changed, 'production_input': True,
              'single_node_playback': False, 'frames': rows, 'shots': shots,
              'code_sha256': capture_hash, 'visual_acceptance': 'Review captures and interactive movement'}
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    (unreal.log_error if not report['complete'] else unreal.log)('COMBAT_JUMP_CAPTURE '+str(error or out))

def shot(name):
    if name in shot_names:
        return
    shot_names.add(name)
    path = out/(name+'.png')
    unreal.SystemLibrary.execute_console_command(world, 'Shot filename='+str(path)+' nosuffix')
    shots.append({'name': name, 'file': str(path.relative_to(root))})

def inject(name, value=unreal.Vector(1, 0, 0)):
    inputs.inject_input_vector_for_action(actions[name], value, [], [])

def advance(name):
    global stage, stage_time
    stage, stage_time = name, unreal.GameplayStatics.get_time_seconds(world)
    if name in ('light', 'heavy', 'jump', 'standing-jump'):
        inject({'light': 'LightPunch', 'heavy': 'HeavyPunch', 'jump': 'Jump', 'standing-jump': 'Jump'}[name])

def run(delta):
    global world, rider, camera, inputs, actions, stage, stage_time
    assert time.monotonic()-started < 150, 'Capture timed out'
    world = editor.get_game_world()
    if not world:
        return
    if stage == 'startup':
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        rider = pc.get_controlled_pawn()
        if not rider.mesh.get_anim_instance().is_epic_animation_enabled():
            return  # Use the production cold-index retry.
        assert rider.mesh.get_anim_instance().get_editor_property('epic_graph_version') == 11
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
        parts = {str(t).removeprefix('CC_Probe_'): a for a in actors for t in a.tags if str(t).startswith('CC_Probe_')}
        camera = parts['Camera']
        floor_scale = parts['Floor'].get_actor_scale3d()
        parts['Floor'].set_actor_scale3d(unreal.Vector(floor_scale.x*10, floor_scale.y*10, floor_scale.z))
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
        inputs = next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
        actions = {n: unreal.load_asset('/Game/CityCourier/Input/IA_'+n) for n in ('Move', 'Sprint', 'LightPunch', 'HeavyPunch', 'Jump')}
        assert all(actions.values())
        pc.set_control_rotation(unreal.Rotator(yaw=270))
        pc.set_view_target_with_blend(camera, 0)
        for command in ('t.MaxFPS 60', 'r.MotionBlurQuality 0'):
            unreal.SystemLibrary.execute_console_command(world, command)
        advance('warmup')
    pos = rider.get_actor_location()
    camera.set_actor_location(pos+unreal.Vector(390, -240, 65), False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), pos), False)
    now = unreal.GameplayStatics.get_time_seconds(world)
    elapsed = now-stage_time
    if stage not in ('warmup', 'settle', 'standing-jump'):
        inject('Move', unreal.Vector(0, 1, 0))
        inject('Sprint')
    anim = rider.mesh.get_anim_instance()
    montage = anim.get_current_active_montage()
    clip = montage.get_first_anim_reference() if montage else None
    velocity = rider.get_velocity()
    rows.append({'stage': stage, 'elapsed': elapsed, 'speed': velocity.length2d(), 'vertical_speed': velocity.z,
                 'airborne': rider.character_movement.is_falling(), 'actor_z': pos.z,
                 'clip': clip.get_name() if clip else None,
                 'montage_time': anim.montage_get_position(montage) if montage else None,
                 'leg_weight': anim.get_editor_property('locomotion_leg_weight'),
                 'action': str(anim.get_editor_property('current_action')),
                 'epic_enabled': anim.is_epic_animation_enabled(),
                 'feet': {name: [point.x, point.y, point.z] for name in ('foot_l', 'foot_r')
                          for point in [rider.mesh.get_socket_location(name)]}})
    if stage == 'warmup' and elapsed > 3:
        advance('sprint')
    elif stage == 'sprint' and elapsed > 1:
        assert velocity.length2d() > 650
        advance('light')
    elif stage in ('light', 'heavy'):
        expected = 'A_LightPunch_L' if stage == 'light' else 'A_HeavyPunch_R'
        if clip and clip.get_name() == expected:
            assert velocity.length2d() > 650, 'Punch truncated sprint momentum'
            if elapsed > .10:
                assert anim.get_editor_property('locomotion_leg_weight') == 1, 'Leg blend lost during punch: '+str(rows[-1])
            if elapsed >= (.18 if stage == 'light' else .40):
                shot('sprint-'+stage)
        if elapsed > (.9 if stage == 'light' else 1.3):
            assert any(r['stage'] == stage and r['clip'] == expected for r in rows), 'Punch never played'
            advance('heavy' if stage == 'light' else 'jump')
    elif stage == 'settle' and elapsed > .4 and velocity.length2d() < 5:
        advance('standing-jump')
    elif stage in ('jump', 'standing-jump'):
        for threshold, name in ((.08, 'takeoff'), (.30, 'ascent'), (.60, 'descent'), (.95, 'landing')):
            if elapsed >= threshold and elapsed < threshold+.12:
                shot(stage+'-'+name)
        if elapsed > 1.6:
            jump = [r for r in rows if r['stage'] == stage]
            clips = {r['clip'] for r in jump}
            start_clip = 'CC_M_Neutral_Jump_F_Start_'+('Sprint' if stage == 'jump' else 'Stand')+'_Lfoot'
            assert {start_clip, 'CC_M_Neutral_Jump_F_Off_Run_Lfoot', 'CC_M_Neutral_Jump_Loop_Fall'} <= clips, 'Missing physical jump phases: '+str(clips)
            assert any(r['airborne'] for r in jump) and not jump[-1]['airborne'], 'No complete physical jump'
            if stage == 'jump':
                assert min(r['speed'] for r in jump) > 650, 'Jump/landing lost sprint momentum'
                advance('settle')
            else:
                assert max(r['speed'] for r in jump) < 5, 'Stationary jump acquired horizontal motion'
                finish()

def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        cleanup() if closing else run(delta)
    except Exception:
        failure = traceback.format_exc()
        unreal.log_error(failure)
        finish(failure)
    finally:
        busy = False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/CityCourier/Maps/L_CharacterProbe')
handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
