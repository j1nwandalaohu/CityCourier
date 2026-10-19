"""Real Enhanced Input jumps onto the hood/roof, then native based movement."""
import hashlib
import json
import math
import time
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/VehicleTraversal'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
out.mkdir(parents=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
started, stage, busy, closing = time.monotonic(), 'startup', False, False
rows, shots, errors = [], [], []
ready, air_seen, hood_seen, roof_seen, drive_started = 0., False, False, False, False
max_drift = 0.
source_hashes = {name: hashlib.sha256((root/name).read_bytes()).hexdigest() for name in (
    'Source/CityCourier/Ambient/AmbientVehicle.cpp', 'Source/CityCourier/Player/CourierCharacter.cpp')}


def finish(error=None):
    global closing, cleanup_started
    if error:
        errors.append(error)
    closing, cleanup_started = True, time.monotonic()
    world = editor.get_game_world()
    if world:
        unreal.SystemLibrary.execute_console_command(world, 'Input.-action IA_Jump')
        levels.editor_request_end_play()


def base_component():
    return rider.character_movement.get_movement_base()


def local_position():
    p, c = rider.get_actor_location(), car.get_actor_location()
    yaw = math.radians(car.get_actor_rotation().yaw)
    dx, dy = p.x-c.x, p.y-c.y
    return [dx*math.cos(yaw)+dy*math.sin(yaw), -dx*math.sin(yaw)+dy*math.cos(yaw), p.z-c.z]


def shot(name):
    path = out/(name+'.png')
    unreal.SystemLibrary.execute_console_command(world, 'Shot showui filename='+str(path)+' nosuffix')
    shots.append({'name': name, 'file': str(path.with_name(path.stem+'00000.png'))})


def jump():
    global air_seen, jump_at
    air_seen, jump_at = False, time.monotonic()
    unreal.SystemLibrary.execute_console_command(world, 'Input.+action IA_Jump true')


def move_forward():
    inputs.inject_input_vector_for_action(move_action, unreal.Vector(0, 1, 0), [], [])


def impl(delta):
    global world, pc, rider, car, inputs, move_action, stage, ready, air_seen, hood_seen, roof_seen
    global drive_started, drive_at, drive_origin, relative_origin, max_drift, velocity_before_jump
    world = editor.get_game_world()
    now = time.monotonic()
    if closing:
        if world and now-cleanup_started < 20:
            return
        report = {'complete': not errors and hood_seen and roof_seen and drive_started,
                  'errors': errors, 'input': 'Production Enhanced Input', 'source_sha256': source_hashes,
                  'hood_landed': hood_seen, 'roof_landed': roof_seen, 'maximum_passenger_drift_cm': max_drift,
                  'map_or_asset_saved': False, 'shots': shots, 'frames': rows}
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        (unreal.log_error if errors else unreal.log)('VEHICLE TRAVERSAL '+str(errors or out))
        return
    if now-started > 150:
        raise RuntimeError('Vehicle traversal capture timed out at '+stage)
    if not world:
        return
    if stage == 'startup':
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        rider = pc.get_controlled_pawn()
        for manager in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientTrafficManager):
            manager.set_actor_tick_enabled(False)
        population = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientVehicle)
        if not population:
            return
        for actor in population:
            actor.set_population_active(False)
        car = next(a for a in population if not isinstance(a, unreal.AmbientPedestrian))
        car.set_patrol([unreal.Vector(-8000, -12220, 74), unreal.Vector(-4000, -12220, 74),
                        unreal.Vector(-4000, -11780, 74), unreal.Vector(-8000, -11780, 74)])
        car.set_population_active(True)
        car.set_actor_tick_enabled(False)
        rider.set_actor_location(unreal.Vector(-7710, -12220, 94), False, True)
        rider.set_actor_rotation(unreal.Rotator(yaw=180), False)
        rider.character_movement.stop_movement_immediately()
        pc.set_control_rotation(unreal.Rotator(pitch=-12, yaw=180))
        inputs = next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
        move_action = unreal.load_asset('/Game/CityCourier/Input/IA_Move')
        unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
        ready, stage = now+2., 'ready'
        return
    base = base_component()
    pos = rider.get_actor_location()
    feet = pos.z-rider.capsule_component.get_scaled_capsule_half_height()
    rows.append({'stage': stage, 'local': local_position(), 'feet_z': feet,
                 'base': base.get_name() if base else None, 'car_yaw': car.get_actor_rotation().yaw,
                 'car_speed': car.get_speed_cm(), 'falling': rider.character_movement.is_falling(),
                 'velocity': [rider.get_velocity().x, rider.get_velocity().y, rider.get_velocity().z]})
    if stage == 'ready' and now >= ready:
        shot('approach')
        jump()
        stage = 'hood-jump'
    elif stage in ('hood-jump', 'roof-jump'):
        move_forward()
        air_seen |= rider.character_movement.is_falling()
        if now-jump_at > .1:
            unreal.SystemLibrary.execute_console_command(world, 'Input.-action IA_Jump')
        if air_seen and rider.character_movement.is_moving_on_ground():
            expected = 'WalkableHood' if stage == 'hood-jump' else 'WalkableRoof'
            assert base and base.get_owner() == car and base.get_name() == expected, 'Wrong physical landing: '+str(rows[-1])
            if stage == 'hood-jump':
                hood_seen = True
                shot('landed-hood')
                ready, stage = now+.4, 'hood-pause'
            else:
                roof_seen = True
                ready, stage = now+.4, 'roof-pause'
        elif now-jump_at > 3:
            raise RuntimeError('No correct landing during '+stage+': '+str(rows[-1]))
    elif stage == 'hood-pause' and now >= ready:
        jump()
        stage = 'roof-jump'
    elif stage == 'roof-pause' and now >= ready:
        pc.set_control_rotation(unreal.Rotator(pitch=-20, yaw=135))
        rider.camera_boom.set_editor_property('target_arm_length', 550.)
        shot('landed-roof')
        relative_origin, drive_origin = local_position(), car.get_actor_location()
        drive_at, drive_started, stage = now, True, 'driving'
        car.set_actor_tick_enabled(True)
    elif stage == 'driving':
        assert base and base.get_owner() == car, 'Lost native movement base during driving'
        drift = math.dist(relative_origin, local_position())
        max_drift = max(max_drift, drift)
        assert drift < 5, 'Passenger drift '+str(drift)
        if abs(car.get_actor_rotation().yaw) > 30:
            assert car.get_actor_location().distance(drive_origin) > 1000
            shot('riding-turn')
            velocity_before_jump = rider.get_velocity()
            jump()
            ready, stage = now+.3, 'jump-off'
        elif now-drive_at > 20:
            raise RuntimeError('Occupied car failed to drive and turn')
    elif stage == 'jump-off':
        if rider.character_movement.is_falling() and not base:
            assert rider.get_velocity().length2d() > 50, 'Jump off lost vehicle momentum'
            shot('jump-off')
            ready, stage = now+1.5, 'finish-wait'
        elif now > ready+3:
            raise RuntimeError('Jump off did not release base: '+str(rows[-1]))
    elif stage == 'finish-wait' and now >= ready:
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
