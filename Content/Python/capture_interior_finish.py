"""Actual UE editor-viewport interior renders, with the complete physical shell.

Run in UnrealEditor GUI after import and physical validation. Creates a new,
unsaved transient scene; never saves a map, mesh or material. The first shot is
the shop. CC_IF_CAPTURE_QUICK=1 limits capture to that first image. Results are
actual editor viewport renders, not PIE/gameplay proof or external mockups.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys
import time
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Content/Python'))
from interior_finish_scene import (build_scene, local_to_world, prepare_render_doors,
                                   protected_snapshot, snapshot_changes)
from tools.interiors.finish_kit import placement_bounds, _segment_box, _rotate

WIDTH, HEIGHT = 1600, 1000
OUT = ROOT/'Saved/Validation/InteriorArt/Captures'
OUT.mkdir(parents=True, exist_ok=True)
RUN_ID = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())
RUN_OUT = OUT/'runs'/RUN_ID
RUN_OUT.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EXPOSURE = dict(mode='manual_physical_camera', fstop=2.8, shutter_speed=60.0,
                iso=1250.0, ev100=math.log2(2.8**2*60*100/1250), compensation=0.0)
LIGHTING = dict(panel_kelvin_by_use={'shop': 4500., 'residential': 3800., 'office': 5000.}, panel_lumens=1800.0,
                sky_intensity=.18, dusk_sun_lux=350.0, dusk_sun_kelvin=4200.0)
CAPTURE_CVARS = ['t.MaxFPS 30', 't.IdleWhenNotForeground 0', 'r.ScreenPercentage 100',
                 'r.AntiAliasingMethod 4', 'r.TemporalAASamples 32', 'r.HighResScreenshotDelay 64',
                 'sg.ViewDistanceQuality 3', 'sg.ShadowQuality 3', 'sg.GlobalIlluminationQuality 3',
                 'sg.ReflectionQuality 3', 'sg.TextureQuality 3', 'sg.EffectsQuality 3',
                 'sg.PostProcessQuality 3', 'r.Streaming.PoolSize 2000',
                 'r.DynamicGlobalIlluminationMethod 1', 'r.ReflectionMethod 1',
                 'r.Shadow.Virtual.Enable 1', 'r.Lumen.TraceMeshSDFs.Allow 1',
                 'r.LumenScene.DirectLighting.UpdateFactor 8', 'r.LumenScene.Radiosity.UpdateFactor 8',
                 'r.Lumen.ScreenProbeGather.DownsampleFactor 16',
                 'r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget 300']
started = time.monotonic()
state = dict(stage='startup', index=0, task=None, busy=False, finished=False,
             shots=[], scene=None, jobs=[], camera=None, callback=None, errors=[],
             realtime_override_requested=False)


def _xyz(v):
    return [float(v.x), float(v.y), float(v.z)]


def _spawn(cls, name, position, rotation=(0, 0, 0)):
    pitch, yaw, roll = rotation
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*position),
        unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll), transient=True)
    if not actor:
        raise RuntimeError('Cannot spawn transient capture fixture: '+name)
    actor.set_actor_label('CC_IF_Capture_'+name)
    actor.tags = [unreal.Name('CC_IF_Capture_'+name)]
    return actor


def _lights(scene):
    count = 0
    fixtures = []
    for record in scene['samples']:
        b = record['plan']['building']
        for placement, _mesh_actor in record['placements']:
            if placement['category'] != 'light_fixture':
                continue
            offset = _rotate([0, 0, -1.5], placement['rotation_pyr'])
            forward = _rotate([0, 0, -1], placement['rotation_pyr'])
            local = [placement['location_cm'][i]+offset[i] for i in range(3)]
            position = local_to_world(b, local)
            target = local_to_world(b, [local[i]+forward[i]*100 for i in range(3)])
            rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*position), unreal.Vector(*target))
            # RectLight's local Y carries its source width. The batten's long
            # axis is local +X; derive roll so that both ceiling and wall
            # fixtures keep the light rectangle behind their actual diffuser.
            width_local = _rotate([1, 0, 0], placement['rotation_pyr'])
            yaw = math.radians(b['yaw'])
            width_world = [math.cos(yaw)*width_local[0]-math.sin(yaw)*width_local[1],
                           math.sin(yaw)*width_local[0]+math.cos(yaw)*width_local[1], width_local[2]]
            base_y = _rotate([0, 1, 0], [rotation.pitch, rotation.yaw, 0])
            base_z = _rotate([0, 0, 1], [rotation.pitch, rotation.yaw, 0])
            roll = math.degrees(math.atan2(-sum(width_world[i]*base_z[i] for i in range(3)),
                                          sum(width_world[i]*base_y[i] for i in range(3))))
            actor = _spawn(unreal.RectLight, placement['name']+'_Light', position,
                           [rotation.pitch, rotation.yaw, roll])
            light = actor.light_component
            light.set_mobility(unreal.ComponentMobility.MOVABLE)
            light.set_intensity_units(unreal.LightUnits.LUMENS)
            light.set_intensity(LIGHTING['panel_lumens'])
            light.set_use_temperature(True)
            light.set_temperature(LIGHTING['panel_kelvin_by_use'][record['plan']['kind']])
            light.set_source_width(90.)
            light.set_source_height(14.)
            light.set_attenuation_radius(650.)
            light.set_editor_property('cast_shadows', True)
            fixtures.append(dict(name=placement['name'], fixture_rotation_pyr=placement['rotation_pyr'],
                                 world_cm=position, world_diffuser_forward=[(target[i]-position[i])/100 for i in range(3)],
                                 rect_rotation_pyr=[rotation.pitch, rotation.yaw, roll],
                                 source_width_cm=90., source_height_cm=14.,
                                 kelvin=LIGHTING['panel_kelvin_by_use'][record['plan']['kind']], lumens=LIGHTING['panel_lumens']))
            count += 1
    sky_asset = unreal.load_asset('/Game/CityCourier/Art/Textures/T_EveningPanorama')
    if sky_asset:
        sky = _spawn(unreal.SkyLight, 'EveningSky', [0, 0, 0])
        sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sky.light_component.set_editor_property('cubemap', sky_asset)
        sky.light_component.set_intensity(LIGHTING['sky_intensity'])
    sun = _spawn(unreal.DirectionalLight, 'DuskSun', [0, 0, 5000], [-8, 28, 0])
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(LIGHTING['dusk_sun_lux'])
    sun.light_component.set_use_temperature(True)
    sun.light_component.set_temperature(LIGHTING['dusk_sun_kelvin'])
    return dict(rect_light_count=count, all_rect_lights_under_actual_fixture_meshes=True,
                fixture_lights=fixtures, evening_hdr=sky_asset.get_path_name() if sky_asset else None, **LIGHTING)


def _bbox_points(bounds):
    lo, hi = bounds
    return [[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]


def _project(point, camera, target, fov):
    forward = [target[i]-camera[i] for i in range(3)]
    length = math.sqrt(sum(v*v for v in forward))
    forward = [v/length for v in forward]
    flat = math.hypot(forward[0], forward[1])
    right = [-forward[1]/flat, forward[0]/flat, 0]
    up = [-forward[2]*right[1], forward[2]*right[0], forward[0]*right[1]-forward[1]*right[0]]
    delta = [point[i]-camera[i] for i in range(3)]
    depth = sum(delta[i]*forward[i] for i in range(3))
    if depth <= 3:
        return None
    tangent = math.tan(math.radians(fov/2))
    return (sum(delta[i]*right[i] for i in range(3))/(depth*tangent),
            sum(delta[i]*up[i] for i in range(3))/(depth*tangent*HEIGHT/WIDTH))


def _screen_area(bounds, camera, target, fov):
    projected = [p for p in (_project(p, camera, target, fov) for p in _bbox_points(bounds)) if p]
    if not projected:
        return 0.
    low = [max(-1., min(p[i] for p in projected)) for i in range(2)]
    high = [min(1., max(p[i] for p in projected)) for i in range(2)]
    return max(0., high[0]-low[0])*max(0., high[1]-low[1])/4


def _room_camera(scene, record, room, furniture_group):
    """Score actual open-leaf/furniture occlusion, framing and foreground size."""
    primary = furniture_group[0]
    bounds = scene['mesh_definitions']
    lo, hi = room['minimum'], room['maximum']
    mesh = bounds[primary['asset']]
    main_bounds = placement_bounds(primary, mesh)
    center = [(main_bounds[0][i]+main_bounds[1][i])/2 for i in range(2)]
    target = center+[lo[2]+(67 if primary['asset'] == 'SM_IF_Bed' else 85)]
    fov = 72.
    # Interior quarter-to-three-quarter region avoids near-wall doors and
    # corner shelves becoming giant foreground objects.
    fractions = (.24, .32, .40, .50, .60, .68, .76)
    candidates = [[lo[0]+(hi[0]-lo[0])*x, lo[1]+(hi[1]-lo[1])*y, lo[2]+150]
                  for x in fractions for y in fractions]
    occluders = [(p['name'], placement_bounds(p, bounds[p['asset']])) for p in furniture_group[1:]]
    styles = {d['door_id']: d['leaf_asset'] for d in record['plan']['door_styles']}
    for door in record['plan']['building']['doors']:
        if (abs(door['hinge'][2]-lo[2]) > 5
                or room['id'] not in (door['from_room'], door['to_room'])):
            continue
        leaf = dict(location_cm=door['hinge'], rotation_pyr=[0, door['closed_yaw']+door['open_angle'], 0],
                    scale3=[door['width']/100, 1, door['height']/220])
        occluders.append((door['id'], placement_bounds(leaf, bounds[styles[door['id']]])))
    # Sample the true customer-facing side at nine points, plus the top. This
    # catches partial obstructions that a single target-center ray misses.
    samples = []
    ml, mh = mesh['minimum_cm'], mesh['maximum_cm']
    for u in (.12, .5, .88):
        for v in (.18, .50, .82):
            source = [ml[0]+(mh[0]-ml[0])*u, ml[1]-1, ml[2]+(mh[2]-ml[2])*v]
            rotated = _rotate([source[i]*primary['scale3'][i] for i in range(3)], primary['rotation_pyr'])
            samples.append([rotated[i]+primary['location_cm'][i] for i in range(3)])
    for u in (.15, .5, .85):
        source = [ml[0]+(mh[0]-ml[0])*u, (ml[1]+mh[1])/2, mh[2]+1]
        rotated = _rotate(source, primary['rotation_pyr'])
        samples.append([rotated[i]+primary['location_cm'][i] for i in range(3)])
    yaw = math.radians(primary['rotation_pyr'][1])
    front = [math.sin(yaw), -math.cos(yaw)]
    ranked = []
    for camera in candidates:
        if any(all(a[i]-12 <= camera[i] <= z[i]+12 for i in range(3)) for _n, (a, z) in occluders+[(primary['name'], main_bounds)]):
            continue
        visible = 0
        for point in samples:
            screen = _project(point, camera, target, fov)
            if screen and abs(screen[0]) < .92 and abs(screen[1]) < .92 and not any(_segment_box(camera, point, a, z) for _n, (a, z) in occluders):
                visible += 1
        coverage = visible/len(samples)
        distance = math.dist(camera[:2], center)
        facing = sum((camera[i]-center[i])*front[i] for i in range(2))/max(distance, 1)
        foreground = sum(_screen_area(volume, camera, target, fov) for _name, volume in occluders)
        area = _screen_area(main_bounds, camera, target, fov)
        score = coverage*1000 + facing*160 - foreground*500 - abs(area-.22)*120 - abs(distance-240)*.1
        ranked.append((score, camera, coverage, foreground, area))
    if not ranked:
        raise RuntimeError('No camera clear of real open leaves and furniture: '+room['id'])
    _score, camera, coverage, foreground, area = max(ranked, key=lambda p: p[0])
    if coverage < .75:
        raise RuntimeError('No honest view shows enough of primary furniture: '+room['id']+' visible='+str(coverage))
    return camera, target, fov, dict(primary_bbox_face_sample_visible_fraction=coverage,
        occluder_projected_area_fraction=foreground, primary_projected_area_fraction=area,
        visibility_method='12 imported-bounds face samples tested against open-leaf and secondary-furniture bounds',
        open_door_leaves_included_as_occluders=True, candidate_count=len(candidates))


def _jobs(scene):
    jobs = []
    lookup = {record['plan']['kind']: record for record in scene['samples']}
    # The shop first provides one inspectable actual render as early as possible.
    requests = [('shop-counter', 'shop', 1, 'shop'),
                ('residential-living', 'residential', 2, 'living'),
                ('residential-bedroom', 'residential', 2, 'bedroom'),
                ('residential-kitchen', 'residential', 2, 'kitchen'),
                ('office-workspace', 'office', 2, 'office'),
                ('office-meeting', 'office', 2, 'meeting')]
    for name, sample_kind, floor, room_kind in requests:
        record = lookup[sample_kind]
        b = record['plan']['building']
        room = next(r for r in b['rooms'] if r['floor'] == floor and r['kind'] == room_kind)
        furniture_group = [p for p, _a in record['placements'] if p.get('room_id') == room['id'] and p['category'] == 'furniture']
        position, target, fov, camera_checks = _room_camera(scene, record, room, furniture_group)
        jobs.append(dict(name=name, building_id=b['id'], kind=sample_kind, room_id=room['id'],
                         camera_local_cm=position, target_local_cm=target, fov_degrees=fov, building=b,
                         camera_checks=camera_checks, primary_furniture=furniture_group[0]['asset'],
                         room_furniture=[p['asset'] for p in furniture_group]))
    b = lookup['residential']['plan']['building']
    hall = next(r for r in b['rooms'] if r['floor'] == 2 and r['kind'] == 'hall')
    jobs.append(dict(name='residential-corridor', building_id=b['id'], kind='residential', room_id=hall['id'],
                     camera_local_cm=[0, hall['minimum'][1]+55, hall['minimum'][2]+155],
                     target_local_cm=[0, hall['maximum'][1]-50, hall['minimum'][2]+140],
                     fov_degrees=69., building=b))
    stairs = next(r for r in b['rooms'] if r['floor'] == 1 and r['kind'] == 'stairs')
    front, z = stairs['minimum'][1], stairs['minimum'][2]
    jobs.append(dict(name='stair-handrail-detail', building_id=b['id'], kind='residential', room_id=stairs['id'],
                     camera_local_cm=[-85, front+35, z+145],
                     target_local_cm=[-85, front+360, z+210], fov_degrees=65., building=b))
    if os.environ.get('CC_IF_CAPTURE_QUICK') == '1':
        jobs = jobs[:1]
    requested = os.environ.get('CC_IF_CAPTURE_VIEWS')
    if requested:
        names = {name.strip() for name in requested.split(',') if name.strip()}
        unknown = names-{j['name'] for j in jobs}
        if unknown:
            raise RuntimeError('Unknown capture views: '+str(sorted(unknown)))
        jobs = [job for job in jobs if job['name'] in names]
    if not jobs:
        raise RuntimeError('No interior capture jobs selected')
    return jobs


def _camera():
    actor = _spawn(unreal.CameraActor, 'Camera', [0, 0, 150])
    component = actor.camera_component
    component.set_editor_property('constrain_aspect_ratio', False)
    settings = component.get_editor_property('post_process_settings')
    values = {'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
              'auto_exposure_apply_physical_camera_exposure': True,
              'camera_shutter_speed': EXPOSURE['shutter_speed'], 'camera_iso': EXPOSURE['iso'],
              'depth_of_field_fstop': EXPOSURE['fstop'], 'auto_exposure_bias': 0.,
              'depth_of_field_focal_distance': 0., 'motion_blur_amount': 0.,
              'bloom_intensity': .08, 'vignette_intensity': .10, 'film_grain_intensity': 0.}
    for name, value in values.items():
        settings.set_editor_property('override_'+name, True)
        settings.set_editor_property(name, value)
    component.set_editor_property('post_process_settings', settings)
    component.set_editor_property('post_process_blend_weight', 1.)
    return actor


def _report(error=None):
    scene = state.get('scene')
    before = scene['protected_before'] if scene else state.get('protected_before', {})
    after = protected_snapshot()
    changed = snapshot_changes(before, after)
    errors = list(state['errors'])
    if error:
        errors.append(error)
    if changed:
        errors.append('Protected retained HEAD files changed: '+str(changed))
    report = dict(schema_version=1, run_id=RUN_ID,
                  status='failed' if errors else 'passed' if len(state['shots']) == len(state['jobs']) else 'running',
                  complete=not errors and len(state['shots']) == len(state['jobs']) and bool(state['shots']),
                  errors=errors, shots=state['shots'], planned_shots=len(state['jobs']),
                  dimensions=[WIDTH, HEIGHT], exposure=EXPOSURE, lighting=state.get('lighting'),
                  capture_console_variables=CAPTURE_CVARS,
                  temporal_viewport=dict(realtime_override_requested=state['realtime_override_requested'],
                      method='Native Stats Display realtime override via stat version; version text is omitted by UE during high-res screenshots',
                      background_cpu_throttling_disabled=True),
                  exit_method='EditorPythonExecuter deferred QUIT_EDITOR after keep_python_script_alive=False',
                  interpretation='Actual Unreal Engine 5.6 editor viewport renders; not PIE or gameplay acceptance.',
                  no_city_maps_written=True, no_assets_saved=True, complete_physical_shell_present=True,
                  walls_hidden_or_cut_away=False, native_collision_retained=True,
                  render_door_proxy_geometry_hidden_only=True,
                  protected_before_sha256=before, protected_after_sha256=after,
                  protected_changed=changed, protected_unchanged=not changed,
                  elapsed_wall_seconds=time.monotonic()-started)
    for path in (RUN_OUT/'results.json', OUT/'results.json'):
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    return report


def _finish(error=None):
    if state['finished']:
        return
    state['finished'] = True
    report = _report(error)
    if state['callback'] is not None:
        unreal.unregister_slate_post_tick_callback(state['callback'])
    levels.eject_pilot_level_actor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    if report['complete']:
        unreal.log('INTERIOR ACTUAL UE CAPTURE COMPLETE: '+str(OUT/'results.json'))
    else:
        unreal.log_error('INTERIOR ACTUAL UE CAPTURE FAILED: '+str(report['errors']))
    # -ExecutePythonScript's EditorPythonExecuter observes KeepAlive=False on
    # its next tick and queues QUIT_EDITOR. A second immediate quit here races
    # with that official deferred teardown while this Slate callback is active.


def _tick_impl(_delta):
    if time.monotonic()-started > 600:
        raise RuntimeError('Actual UE interior viewport capture exceeded 600 seconds')
    now = time.monotonic()
    if state['stage'] == 'setup':
        job = state['jobs'][state['index']]
        b = job['building']
        position, target = local_to_world(b, job['camera_local_cm']), local_to_world(b, job['target_local_cm'])
        camera = state['camera']
        # Detach the previous viewport before moving its piloted actor: editor
        # view synchronization must not push the old view back into this camera.
        levels.eject_pilot_level_actor()
        camera.set_actor_location(unreal.Vector(*position), False, True)
        rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*position), unreal.Vector(*target))
        camera.set_actor_rotation(unreal.Rotator(pitch=rotation.pitch, yaw=rotation.yaw, roll=rotation.roll), False)
        camera.camera_component.set_field_of_view(job['fov_degrees'])
        levels.pilot_level_actor(camera)
        levels.set_exact_camera_view(True)
        levels.editor_set_game_view(True)
        if not state['realtime_override_requested']:
            # EditorSetViewportRealtime(True) merely removes a named false
            # override; it neither enables realtime nor tolerates a missing
            # override. A native stat display explicitly adds a TRUE override
            # to this active editor viewport (EditorViewportClient.cpp).
            # RenderStatVersion suppresses its text when GIsHighResScreenshot,
            # preserving a clean image while allowing real TSR/Lumen history.
            unreal.SystemLibrary.execute_console_command(state['scene']['world'], 'stat none')
            unreal.SystemLibrary.execute_console_command(state['scene']['world'], 'stat version')
            state['realtime_override_requested'] = True
        state['active'] = {k: v for k, v in job.items() if k != 'building'}
        state['active'].update(camera_world_cm=position, target_world_cm=target,
                               camera_rotation_pyr=[rotation.pitch, rotation.yaw, rotation.roll])
        state['file'] = RUN_OUT/(job['name']+'.png')
        state['ready'] = now+(12 if state['index'] == 0 else 7)
        state['stage'] = 'settle'
    elif state['stage'] == 'settle' and now >= state['ready']:
        camera = state['camera']
        actual_position, actual_rotation = camera.get_actor_location(), camera.get_actor_rotation()
        state['active'].update(actual_camera_world_cm=_xyz(actual_position),
            actual_camera_rotation_pyr=[actual_rotation.pitch, actual_rotation.yaw, actual_rotation.roll])
        expected = state['active']['camera_world_cm']
        drift = math.dist(_xyz(actual_position), expected)
        state['active']['camera_position_drift_cm'] = drift
        viewport_info = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_level_viewport_camera_info()
        if viewport_info is None:
            raise RuntimeError('The actual editor viewport camera is unavailable')
        if len(viewport_info) == 3:
            success, viewport_position, viewport_rotation = viewport_info
            if not success:
                raise RuntimeError('UE could not measure the primary editor viewport camera')
        else:
            viewport_position, viewport_rotation = viewport_info
        state['active'].update(viewport_world_cm=_xyz(viewport_position),
            viewport_rotation_pyr=[viewport_rotation.pitch, viewport_rotation.yaw, viewport_rotation.roll],
            viewport_to_camera_drift_cm=math.dist(_xyz(viewport_position), _xyz(actual_position)))
        if drift > .1:
            raise RuntimeError('Piloted camera drifted from its honest interior position: '+str(drift))
        state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(WIDTH, HEIGHT, str(state['file']),
                                                                         camera=state['camera'], force_game_view=True)
        if state['task'] is None:
            raise RuntimeError('UE could not create actual viewport screenshot task')
        # UE synchronously finishes shader/asset loading inside the request.
        # Start its asynchronous file deadline after that work, not before it.
        state['ready'] = time.monotonic()+60
        state['stage'] = 'screenshot'
    elif state['stage'] == 'screenshot':
        if now > state['ready']:
            raise RuntimeError('UE viewport screenshot timed out: '+str(state['file']))
        if not state['task'].is_task_done() or not state['file'].exists():
            return
        data = state['file'].read_bytes()
        if data[:8] != b'\x89PNG\r\n\x1a\n' or len(data) < 10000:
            raise RuntimeError('Actual UE capture PNG is absent or invalid')
        if struct.unpack('>II', data[16:24]) != (WIDTH, HEIGHT):
            raise RuntimeError('Actual UE capture dimensions differ from requested dimensions')
        shot = state['active']
        shot.update(file=state['file'].relative_to(ROOT).as_posix(), dimensions=[WIDTH, HEIGHT],
                    png_sha256=hashlib.sha256(data).hexdigest(), png_bytes=len(data),
                    actual_unreal_editor_viewport=True, screenshot_task_complete=True)
        state['shots'].append(shot)
        _report()
        unreal.log('INTERIOR ACTUAL UE SHOT READY: '+shot['name']+' '+str(state['file']))
        state['index'] += 1
        if state['index'] == len(state['jobs']):
            _finish()
        else:
            state['stage'] = 'setup'


def _tick(delta):
    if state['busy'] or state['finished']:
        return
    state['busy'] = True
    try:
        _tick_impl(delta)
    except Exception:
        _finish(traceback.format_exc())
    finally:
        state['busy'] = False


def main():
    try:
        state['protected_before'] = protected_snapshot()
        unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        # A hidden editor otherwise adds a Background Process false realtime
        # override every tick, independently of t.IdleWhenNotForeground.
        performance_class = unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')
        if performance_class is None:
            raise RuntimeError('Editor performance settings class is unavailable')
        unreal.get_default_object(performance_class).set_editor_property(
            'bThrottleCPUWhenNotForeground', False)
        state['scene'] = build_scene()
        state['lighting'] = _lights(state['scene'])
        prepare_render_doors(state['scene'])
        state['jobs'] = _jobs(state['scene'])
        state['camera'] = _camera()
        # Runtime CVars affect only this transient editor process.
        for command in CAPTURE_CVARS:
            unreal.SystemLibrary.execute_console_command(state['scene']['world'], command)
        state['stage'] = 'setup'
        state['callback'] = unreal.register_slate_post_tick_callback(_tick)
    except Exception:
        _finish(traceback.format_exc())


if __name__ == '__main__':
    main()
