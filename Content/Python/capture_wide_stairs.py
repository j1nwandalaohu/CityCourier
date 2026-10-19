"""Compare native stairs in PIE using the same gameplay camera and exposure."""
import json
import math
import os
from pathlib import Path
import time
import unreal

root = Path(unreal.Paths.project_dir())
stage = os.environ.get('CC_STAIR_STAGE','after')
assert stage in ('before','after')
out = root/'Saved/Validation/WideStairs'/stage
out.mkdir(parents=True,exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
building = next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.CourierBuildingActor) and a.get_actor_label()=='B_Merchant_0')
b = building.catalog.buildings[building.building_index]
step = next(x for x in b.boxes if str(x.id).endswith('F01_up_0'))
landing = next(x for x in b.boxes if str(x.id).endswith('F01_half_landing'))
z = b.floors[0].elevation
angle = math.radians(b.yaw)
def world(p):
    return [b.origin.x+math.cos(angle)*p[0]-math.sin(angle)*p[1],
            b.origin.y+math.sin(angle)*p[0]+math.cos(angle)*p[1], b.origin.z+p[2]]
jobs = [dict(name='flight',pos=world([step.center.x,step.center.y-90,z+150]),
             target=world([step.center.x,landing.center.y,z+180])),
        dict(name='turn',pos=world([0,landing.center.y+45,landing.center.z+160]),
             target=world([0,step.center.y+20,landing.center.z+90]))]
measurements = dict(flight_width_cm=step.size.x,turn_depth_cm=landing.size.y,turn_width_cm=landing.size.x)
camera = actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector())
camera.tags = [unreal.Name('WideStairCapture')]
camera.camera_component.set_field_of_view(85)
state = dict(phase=0,index=0,done=False)
start = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()

def finish(error=None):
    if state['done']: return
    state['done'] = True
    (out/'result.json').write_text(json.dumps(dict(error=error,measurements=measurements,jobs=jobs),indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    actors.destroy_actor(camera)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(_delta):
    try:
        if time.monotonic()-start>150: raise RuntimeError('Stair capture timeout')
        w = editor.get_game_world()
        if not w or state['done']: return
        if state['phase']==0:
            pc = unreal.GameplayStatics.get_player_controller(w,0)
            if not pc or not pc.get_controlled_pawn(): return
            state['pawn'] = pc.get_controlled_pawn()
            state['pawn'].character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state['pawn'].set_actor_hidden_in_game(True)
            state['camera'] = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CameraActor)
                                    if 'WideStairCapture' in [str(t) for t in a.tags])
            state['building'] = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CourierBuildingActor)
                                     if str(a.catalog.buildings[a.building_index].id)=='B_Merchant_0')
            pc.set_view_target_with_blend(state['camera'],0)
            for cmd in ('t.MaxFPS 30','t.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(w,cmd)
            state['phase'] = 1
        elif state['phase']==1:
            j = jobs[state['index']]
            pos = unreal.Vector(*j['pos'])
            state['pawn'].set_actor_location(pos,False,True)
            state['camera'].set_actor_location(pos,False,True)
            state['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(*j['target'])),False)
            assert state['building'].prepare_details()
            state['ready'] = time.monotonic()+6
            state['phase'] = 2
        elif state['phase']==2 and time.monotonic()>state['ready']:
            state['phase'] = 4
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(out/(jobs[state['index']]['name']+'.png')))
            state['phase'] = 3
        elif state['phase']==3 and state['task'].is_task_done():
            state['index'] += 1
            if state['index']==len(jobs): finish()
            else: state['phase'] = 1
    except Exception as error:
        unreal.log_error(str(error))
        finish(str(error))

handle = unreal.register_slate_post_tick_callback(tick)
