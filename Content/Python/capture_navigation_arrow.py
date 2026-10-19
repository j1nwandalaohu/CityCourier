"""Capture the native route arrow while changing the real gameplay camera."""
import hashlib
import json
import time
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/NavigationArrow'/(time.strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8])
out.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
started, stage, index, busy, closing = time.monotonic(), 'startup', 0, False, False
shots = []
jobs = [('no-target', None), ('camera-forward', 0.), ('camera-right', 90.), ('camera-left', -90.), ('camera-back', 180.), ('map-hides-arrow', None), ('gameplay-restores-arrow', 0.)]
source_hash = hashlib.sha256((root/'Source/CityCourier/UI/CourierNavigationArrowWidget.cpp').read_bytes()).hexdigest()


def finish(error=None):
    global closing, error_result, cleanup_started
    closing, error_result, cleanup_started = True, error, time.monotonic()
    if editor.get_game_world():
        levels.editor_request_end_play()


def cleanup():
    if editor.get_game_world() and time.monotonic()-cleanup_started < 20:
        return
    report = {'complete': error_result is None and len(shots) == len(jobs), 'error': error_result,
              'shots': shots, 'source_sha256': source_hash, 'native_gameplay_hud': True,
              'note': 'Camera bearings changed through the real PlayerController; no map or widget asset saved.'}
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    (unreal.log_error if error_result else unreal.log)('NAVIGATION ARROW CAPTURE '+str(error_result or out))


def impl(delta):
    global pc, rider, arrow, stage, ready, index
    if time.monotonic()-started > 180:
        raise RuntimeError('Navigation arrow capture timed out')
    world = editor.get_game_world()
    if not world:
        return
    now = time.monotonic()
    if stage == 'startup':
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        rider = pc.get_controlled_pawn()
        arrows = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.CourierNavigationArrowWidget, False)
        if not arrows:
            return
        assert len(arrows) == 1
        arrow = arrows[0]
        unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
        stage, ready = 'wait', now+8
    if now < ready:
        return
    if stage == 'wait':
        name, yaw = jobs[index]
        if name == 'camera-forward':
            assert pc.courier_order(1), 'Could not accept an actual offer'
        if name == 'map-hides-arrow':
            pc.toggle_map()
        elif name == 'gameplay-restores-arrow':
            pc.toggle_map()
        if yaw is not None:
            pc.set_control_rotation(unreal.Rotator(pitch=-12., yaw=yaw))
        stage, ready = 'capture', now+1.5
    elif stage == 'capture':
        name, yaw = jobs[index]
        if name == 'map-hides-arrow':
            assert arrow.get_visibility() == unreal.SlateVisibility.COLLAPSED
        elif name != 'no-target':
            assert arrow.get_visibility() != unreal.SlateVisibility.COLLAPSED
        path = out/(name+'.png')
        unreal.SystemLibrary.execute_console_command(world, 'Shot showui filename='+str(path)+' nosuffix')
        shots.append({'name': name, 'file': str(path.with_name(path.stem+'00000.png')),
                      'camera_yaw': pc.player_camera_manager.get_camera_rotation().yaw,
                      'target': str(pc.get_delivery_system().get_target_id()), 'visibility': str(arrow.get_visibility())})
        index += 1
        if index == len(jobs):
            finish()
        else:
            stage, ready = 'wait', now+1


def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        cleanup() if closing else impl(delta)
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
