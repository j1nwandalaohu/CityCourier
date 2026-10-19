"""Measure real PIE ambient feet/wheel origin against static street collision."""
import json
import time
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/AmbientGrounding'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
out.mkdir(parents=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
started, next_sample, index, busy, closing = time.monotonic(), 0., 0, False, False
samples = []
positions = [(0, -12000), (12000, 0), (0, 12000), (-12000, 0)]


def finish(error=None):
    global closing, cleanup_at, error_result
    closing, cleanup_at, error_result = True, time.monotonic(), error
    levels.editor_request_end_play()


def tick_impl(delta):
    global next_sample, index
    world = editor.get_game_world()
    now = time.monotonic()
    if closing:
        if world and now-cleanup_at < 20:
            return
        measurements = [a for s in samples for a in s['actors'] if a['ground_z'] is not None]
        report = {'error': error_result, 'samples': samples, 'measured_actor_samples': len(measurements),
                  'maximum_gap_cm': max((abs(a['gap_cm']) for a in measurements), default=None),
                  'unsupported_actor_samples': sum(a['ground_z'] is None for s in samples for a in s['actors']),
                  'floating_actor_samples': sum(a['gap_cm'] > 3 for a in measurements)}
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.log('AMBIENT GROUND PROBE '+str(out))
        return
    if now-started > 150:
        raise RuntimeError('PIE ambient probe timed out')
    if not world or now < next_sample:
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if not pc or not pc.get_controlled_pawn():
        return
    managers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientTrafficManager)
    if not managers:
        return
    pawn = pc.get_controlled_pawn()
    if index % 5 == 0:
        x, y = positions[index//5]
        pawn.set_actor_location(unreal.Vector(x, y, 100), False, True)
        managers[0].update_population(pawn.get_actor_location())
        next_sample = now+1.5
        index += 1
        return
    actors = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientVehicle):
        if actor.get_editor_property('hidden'):
            continue
        p = actor.get_actor_location()
        walker = isinstance(actor, unreal.AmbientPedestrian)
        clearance = 90 if walker else 70
        # Independent street-height query: 50cm above street datum, not a ray
        # from above awnings or vehicle roofs, and static object types only.
        hit = unreal.SystemLibrary.line_trace_single_for_objects(world,
            unreal.Vector(p.x, p.y, 50), unreal.Vector(p.x, p.y, -200),
            [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1], False, [actor], unreal.DrawDebugTrace.NONE, True)
        ground = hit.to_tuple()[5].z if hit else None
        actors.append({'name': actor.get_name(), 'walker': walker, 'x': p.x, 'y': p.y,
                       'z': p.z, 'ground_z': ground, 'gap_cm': p.z-clearance-ground if ground is not None else None,
                       'speed_cm_s': actor.get_speed_cm()})
    samples.append({'phase': index//5, 'actors': actors})
    index += 1
    next_sample = now+1.
    if index >= 20:
        finish()


def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        tick_impl(delta)
    except Exception:
        unreal.log_error(traceback.format_exc())
        finish(traceback.format_exc())
    finally:
        busy = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
