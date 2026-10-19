"""Capture saved map/native PIE fixtures with unmodified gameplay exposure.

Uses the same cameras as the original brightness comparison. Never saves the
map or uses cinematic exposure, supplemental lights or runtime light overrides.
"""
import json
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root/'Saved/Validation/LightingBalance/final'
out.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector())
camera.tags = [unreal.Name('LightingBalanceCamera')]
camera.camera_component.set_field_of_view(70)
shots = [dict(name='street', pos=(-2700,-12500,210), target=(-300,-11100,650)),
         dict(name='market', pos=(12600,12100,240), target=(13800,13300,450)),
         dict(name='shop-counter', building='B_Merchant_0',
              pos=(-10759.6,-8331,190), target=(-10947.450000584126,-8351.499999046326,125)),
         dict(name='residential-kitchen', building='B_1_0_0_0_0',
              pos=(-22821.8,-10112,575), target=(-22821.999999523163,-9850.200000464916,510)),
         dict(name='office-workspace', building='B_0_1_0_0_0',
              pos=(-10778,-22896,575), target=(-10458.333332498869,-22962.750001192093,510))]
jobs = [('final', shot) for shot in shots]
state = dict(index=0, phase=0, measurements=[])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()
start = time.monotonic()


def finish(error=None):
    (out/'result.json').write_text(json.dumps(dict(error=error, measurements=state['measurements']),indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    actors.destroy_actor(camera)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    try:
        if time.monotonic()-start > 300:
            raise RuntimeError('Capture timeout')
        world = editor.get_game_world()
        if not world:
            return
        if state['phase'] == 0:
            pc = unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():
                return
            state['pawn'] = pc.get_controlled_pawn()
            state['pawn'].character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state['pawn'].set_actor_hidden_in_game(True)
            state['camera'] = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
                                   if 'LightingBalanceCamera' in [str(t) for t in a.tags])
            state['buildings'] = {str(a.catalog.buildings[a.building_index].id):a for a in
                                 unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CourierBuildingActor)}
            pc.set_view_target_with_blend(state['camera'],0)
            for cmd in ('t.MaxFPS 30','t.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            state['phase'] = 1
        elif state['phase'] == 1:
            mode, shot = jobs[state['index']]
            pos = unreal.Vector(*shot['pos'])
            state['pawn'].set_actor_location(pos,False,True)
            state['camera'].set_actor_location(pos,False,True)
            state['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(*shot['target'])),False)
            if shot.get('building'):
                assert state['buildings'][shot['building']].prepare_details()
            state['ready'] = time.monotonic()+2
            state['phase'] = 2
        elif state['phase'] == 2 and time.monotonic()>state['ready']:
            mode, shot = jobs[state['index']]
            state['ready'] = time.monotonic()+5
            state['phase'] = 3
        elif state['phase'] == 3 and time.monotonic()>state['ready']:
            # Screenshot dispatch can pump Slate ticks recursively. Park this
            # phase before dispatch so nested ticks cannot enqueue duplicates.
            state['phase'] = 5
            mode, shot = jobs[state['index']]
            lights = []
            if shot.get('building'):
                lights = [c.intensity for c in state['buildings'][shot['building']].get_components_by_class(unreal.RectLightComponent)]
                expected = {'shop-counter':240, 'residential-kitchen':180, 'office-workspace':300}[shot['name']]
                assert lights and all(abs(value-expected)<.01 for value in lights), 'Unexpected native fixture output'
            state['measurements'].append(dict(mode=mode,shot=shot,fixture_lumens=sorted(set(lights))))
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(out/(mode+'-'+shot['name']+'.png')))
            state['phase'] = 4
        elif state['phase'] == 4 and state['task'].is_task_done():
            state['index'] += 1
            if state['index'] == len(jobs):
                finish()
            else:
                state['phase'] = 1
    except Exception as error:
        unreal.log_error(str(error))
        finish(str(error))


handle = unreal.register_slate_post_tick_callback(tick)
