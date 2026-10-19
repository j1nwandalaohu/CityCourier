"""PIE proof: production Enhanced Input punches, civilian poses and recovery."""
import hashlib
import json
import time
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/PedestrianBoxing'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
out.mkdir(parents=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
stage, busy, closing = 'startup', False, False
started = time.monotonic()
rows, shots, errors = [], [], []
seen = set()
sources = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in (
    'Source/CityCourier/Ambient/AmbientPedestrian.cpp',
    'Source/CityCourier/Ambient/CitizenAnimInstance.cpp',
    'Source/CityCourier/Player/CourierActionComponent.cpp')}


def cmd(value):
    unreal.SystemLibrary.execute_console_command(world, value)


def shot(name):
    path = out/(name+'.png')
    cmd('Shot showui filename='+str(path)+' nosuffix')
    shots.append(str(path.with_name(path.stem+'00000.png')))


def finish(error=None):
    global closing, cleanup_at
    if error:
        errors.append(error)
    closing, cleanup_at = True, time.monotonic()
    if editor.get_game_world():
        cmd('Input.-action IA_LightPunch')
        cmd('Input.-action IA_HeavyPunch')
        levels.editor_request_end_play()


def impl(delta):
    global world, pc, rider, ped, stage, ready, origin, punch_at, down_at, recover_at
    world = editor.get_game_world()
    now = time.monotonic()
    if closing:
        if world and now-cleanup_at < 15:
            return
        (out/'results.json').write_text(json.dumps({'complete': not errors and stage == 'complete',
            'errors': errors, 'input': 'Production Enhanced Input', 'source_sha256': sources,
            'seen': sorted(seen), 'shots': shots, 'frames': rows, 'map_saved': False}, indent=2)+'\n')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        (unreal.log_error if errors else unreal.log)('PEDESTRIAN_BOXING '+str(errors or out))
        return
    if now-started > 120:
        raise RuntimeError('Boxing capture timed out in '+stage)
    if not world:
        return
    if stage == 'startup':
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not isinstance(pc.get_controlled_pawn(), unreal.CourierCharacter):
            return
        rider = pc.get_controlled_pawn()
        for manager in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientTrafficManager):
            manager.set_actor_tick_enabled(False)
        population = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientVehicle)
        if not any(isinstance(p, unreal.AmbientPedestrian) for p in population):
            return
        for actor in population:
            actor.set_population_active(False)
        ped = next(p for p in population if isinstance(p, unreal.AmbientPedestrian))
        ped.set_patrol([unreal.Vector(-7900, -12220, 94), unreal.Vector(-3900, -12220, 94)])
        ped.set_population_active(True)
        ped.set_actor_tick_enabled(False)
        origin = ped.get_actor_location()
        rider.set_actor_location(origin+unreal.Vector(-90, 0, 0), False, True)
        rider.set_actor_rotation(unreal.Rotator(yaw=0), False)
        rider.character_movement.stop_movement_immediately()
        pc.set_control_rotation(unreal.Rotator(pitch=-22, yaw=0))
        pc.set_ignore_look_input(True)
        rider.camera_boom.set_editor_property('target_arm_length', 420.)
        cmd('t.MaxFPS 60')
        ready, stage = now+2., 'light-ready'
        return
    reaction = str(ped.get_reaction()).split('.')[-1].split(':')[0].strip('<> ')
    elapsed = ped.get_reaction_elapsed()
    p = ped.get_actor_location()
    person = ped.get_editor_property('Person')
    mesh_floor = person.get_world_location().z
    head = person.get_socket_location('head').z
    rows.append({'stage': stage, 'reaction': reaction, 'elapsed': elapsed,
        'position': [p.x, p.y, p.z], 'mesh_floor_z': mesh_floor, 'head_z': head,
        'speed': ped.get_speed_cm(),
        'player_feet_z': rider.get_actor_location().z-rider.capsule_component.get_scaled_capsule_half_height(),
        'player_action': str(rider.mesh.get_anim_instance().get_editor_property('CurrentAction')),
        'camera_yaw': pc.get_control_rotation().yaw,
        'rider_yaw': rider.get_actor_rotation().yaw})
    if len(rows) % 90 == 0:
        unreal.log('BOXING_PROGRESS '+str(rows[-1]))
    seen.add(reaction)
    assert abs(mesh_floor-4.) < 2, 'Citizen mesh floor lost street alignment: '+str(rows[-1])
    if stage == 'light-ready' and now >= ready:
        shot('before-hit')
        cmd('Input.+action IA_LightPunch true')
        punch_at, stage = now, 'light-contact'
    elif stage == 'light-contact':
        if now-punch_at > .08:
            cmd('Input.-action IA_LightPunch')
        if 'STAGGERING' in reaction:
            ped.set_actor_tick_enabled(True)
            shot('light-stagger')
            stage = 'light-recover'
        elif now-punch_at > 2:
            raise RuntimeError('Production light punch failed to stagger pedestrian')
    elif stage == 'light-recover' and 'WALKING' in reaction:
        # Stop route only for deterministic spacing before the second input.
        ped.set_actor_tick_enabled(False)
        origin = ped.get_actor_location()
        rider.set_actor_location(ped.get_actor_location()+unreal.Vector(-75, 0, 0), False, True)
        rider.character_movement.stop_movement_immediately()
        ready, stage = now+.3, 'heavy-ready'
    elif stage == 'heavy-ready' and now >= ready:
        cmd('Input.+action IA_HeavyPunch true')
        punch_at, stage = now, 'heavy-contact'
    elif stage == 'heavy-contact':
        if now-punch_at > .08:
            cmd('Input.-action IA_HeavyPunch')
        if 'FALLING' in reaction:
            ped.set_actor_tick_enabled(True)
            stage = 'falling'
        elif now-punch_at > 2:
            raise RuntimeError('Production heavy punch failed to knock down pedestrian')
    elif stage == 'falling' and reaction.endswith('DOWN'):
        down_at, stage = now, 'down'
        shot('knocked-down')
        assert head < mesh_floor+55, 'Down state did not evaluate a down skeletal pose'
    elif stage == 'down':
        assert abs(p.x-origin.x) < 2 and abs(p.y-origin.y) < 2, 'Down citizen continued patrol'
        if elapsed > .2:
            assert rows[-1]['player_feet_z'] < mesh_floor+5, 'Fallen collision lifted the player'
        if 'GETTING_UP' in reaction:
            recover_at, stage = now, 'getting-up'
    elif stage == 'getting-up':
        if elapsed > 1.4 and 'recovery-shot' not in seen:
            seen.add('recovery-shot')
            shot('getting-up')
        if 'WALKING' in reaction:
            ready, stage = now+1.2, 'walk-resumed'
    elif stage == 'walk-resumed' and now >= ready:
        assert ped.get_speed_cm() > 10 and p.distance(origin) > 25, 'Citizen failed to resume original patrol'
        shot('walking-again')
        stage = 'complete'
        finish()


def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        impl(delta)
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
