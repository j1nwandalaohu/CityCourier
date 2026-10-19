"""Inspect delivered shop art from the street, through actual front glazing."""
import json
import math
import os
from pathlib import Path
import time
import unreal

root = Path(unreal.Paths.project_dir())
stage = os.environ.get('CC_STREET_SHOPS_STAGE','after')
assert stage in ('before','after')
out = root/'Saved/Validation/StreetShops'/stage
out.mkdir(parents=True,exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
jobs = []
for ident in ('B_0_0_0_0_0','B_0_0_0_0_1','B_0_0_0_0_2'):
    a = next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.CourierBuildingActor) and a.get_actor_label()==ident)
    b = a.catalog.buildings[a.building_index]
    room = next(r for r in b.rooms if r.floor==1 and str(r.kind)=='shop')
    angle = math.radians(b.yaw)
    def world(p):
        return [b.origin.x+math.cos(angle)*p[0]-math.sin(angle)*p[1],
                b.origin.y+math.sin(angle)*p[0]+math.cos(angle)*p[1],b.origin.z+p[2]]
    jobs.append(dict(name=ident,building=ident,pos=world([room.standing_point.x,-b.size.y/2-600,180]),
                     target=world([room.standing_point.x,-b.size.y/2+150,190])))
camera = actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector())
camera.tags = [unreal.Name('StreetShopCapture')]
camera.camera_component.set_field_of_view(75)
state = dict(phase=0,index=0,shots=[],done=False)
start = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()

def finish(error=None):
    if state['done']: return
    state['done'] = True
    (out/'result.json').write_text(json.dumps(dict(error=error,shots=state['shots']),indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    actors.destroy_actor(camera)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(_delta):
    try:
        if time.monotonic()-start>200: raise RuntimeError('Street shop capture timeout')
        w = editor.get_game_world()
        if state['done']: return
        if not w:
            if state['phase']>0: raise RuntimeError('PIE ended before all street shots were captured')
            return
        if state['phase']==0:
            pc = unreal.GameplayStatics.get_player_controller(w,0)
            if not pc or not pc.get_controlled_pawn(): return
            state['pawn'] = pc.get_controlled_pawn()
            state['pawn'].character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state['pawn'].set_actor_hidden_in_game(True)
            state['pawn'].set_actor_enable_collision(False)
            state['camera'] = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CameraActor)
                                    if 'StreetShopCapture' in [str(t) for t in c.tags])
            state['buildings'] = {str(a.catalog.buildings[a.building_index].id):a for a in
                unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CourierBuildingActor)}
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
            assert state['buildings'][j['building']].prepare_details()
            state['ready'] = time.monotonic()+5
            state['phase'] = 2
        elif state['phase']==2 and time.monotonic()>state['ready']:
            state['phase'] = 4
            j = jobs[state['index']]
            glass = [c for c in state['buildings'][j['building']].get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
                     if c.get_material(0) and c.get_material(0).get_name()=='M_ShopGlass']
            if stage=='after':
                assert glass and all(c.get_material(0).get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT for c in glass)
                assert all(c.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION for c in glass)
            state['shots'].append(dict(**j,transparent_native_glass_instances=sum(c.get_instance_count() for c in glass)))
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(out/(j['name']+'.png')))
            state['phase'] = 3
        elif state['phase']==3 and state['task'].is_task_done():
            state['index'] += 1
            if state['index']==len(jobs): finish()
            else: state['phase'] = 1
    except Exception as error:
        unreal.log_error(str(error))
        finish(str(error))

handle = unreal.register_slate_post_tick_callback(tick)
