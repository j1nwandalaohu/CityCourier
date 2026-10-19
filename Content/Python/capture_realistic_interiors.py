"""Actual city PIE evidence: street views, food closeups and residential finishes."""
import json
import math
import os
from pathlib import Path
import sys
import time
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT))
from tools.interiors.finish_kit import _sample,placement_bounds

out=ROOT/'Saved/Validation/RealisticInteriors/Captures'
out.mkdir(parents=True,exist_ok=True)
source=ROOT/'AssetsSource/CityCourier'
inventory=json.loads((source/'Interiors/inventory.json').read_text())
bounds={a['name']:a for a in json.loads((source/'InteriorArt/asset-manifest.json').read_text())['assets']}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
jobs=[]
for ident in ('B_0_0_0_0_0','B_0_0_0_0_1','B_0_0_0_0_2','B_1_0_0_0_0'):
    seed=next(b for b in inventory['buildings'] if b['id']==ident)
    sample=_sample(seed,bounds);b=sample['building'];angle=math.radians(b['yaw'])
    def world(point):
        return [b['location'][0]+math.cos(angle)*point[0]-math.sin(angle)*point[1],
                b['location'][1]+math.sin(angle)*point[0]+math.cos(angle)*point[1],b['location'][2]+point[2]]
    if ident!='B_1_0_0_0_0':
        room=next(r for r in b['rooms'] if r['floor']==1 and r['kind']=='shop')
        primary=next(p for p in sample['placements'] if p.get('room_id')==room['id'] and
                     p['asset'] in ('SM_IF_ShopNoodleCooking','SM_IF_ShopGroceryCheckout','SM_IF_ShopRoastDisplay'))
        lo,hi=placement_bounds(primary,bounds[primary['asset']]);x=(lo[0]+hi[0])/2;y=(lo[1]+hi[1])/2
        floor=room['minimum'][2]
        target_z=150 if 'Noodle' in primary['asset'] else 140 if 'Roast' in primary['asset'] else 105
        jobs.append(dict(name=ident+'-food',building=ident,asset=primary['asset'],
            pos=world([x,max(room['minimum'][1]+30,y-165),floor+155]),
            target=world([x,y,floor+target_z]),fov=68))
        jobs.append(dict(name=ident+'-street',building=ident,asset=primary['asset'],
            pos=world([room['center'][0],-b['depth']/2-600,180]),
            target=world([room['center'][0],-b['depth']/2+150,190]),fov=75))
    else:
        for kind in ('bedroom','kitchen'):
            room=next(r for r in b['rooms'] if r['floor']==2 and r['kind']==kind)
            furniture=next(p for p in sample['placements'] if p.get('room_id')==room['id'] and p['category']=='furniture')
            lo,hi=placement_bounds(furniture,bounds[furniture['asset']])
            jobs.append(dict(name=ident+'-'+kind,building=ident,asset=furniture['asset'],
                pos=world([room['center'][0],room['minimum'][1]+40,room['minimum'][2]+160]),
                target=world([(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,room['minimum'][2]+95]),fov=85))
selection=os.environ.get('CC_REALISTIC_CAPTURE_ONLY','')
if selection:
    wanted=set(selection.split(','));jobs=[j for j in jobs if j['name'] in wanted]
    assert {j['name'] for j in jobs}==wanted,'Unknown capture selection'
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector())
camera.tags=[unreal.Name('RealisticInteriorCapture')]
state=dict(phase=0,index=0,shots=[],done=False)
start=time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()


def finish(error=None):
    if state['done']:return
    state['done']=True
    filename='result-detail.json' if selection else 'result.json'
    (out/filename).write_text(json.dumps(dict(error=error,shots=state['shots']),indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play();actors.destroy_actor(camera)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    try:
        if time.monotonic()-start>260:raise RuntimeError('Realistic interior capture timeout')
        w=editor.get_game_world()
        if state['done']:return
        if not w:
            if state['phase']>0:raise RuntimeError('PIE ended before capture completion')
            return
        if state['phase']==0:
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            if not pc or not pc.get_controlled_pawn():return
            pawn=pc.get_controlled_pawn();state['pawn']=pawn
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            pawn.set_actor_hidden_in_game(True);pawn.set_actor_enable_collision(False)
            state['camera']=next(c for c in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CameraActor)
                if 'RealisticInteriorCapture' in [str(t) for t in c.tags])
            state['buildings']={str(a.catalog.buildings[a.building_index].id):a
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CourierBuildingActor)}
            pc.set_view_target_with_blend(state['camera'],0)
            for cmd in ('t.MaxFPS 30','t.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(w,cmd)
            state['phase']=1
        elif state['phase']==1:
            job=jobs[state['index']];pos=unreal.Vector(*job['pos'])
            state['pawn'].set_actor_location(pos,False,True)
            state['camera'].set_actor_location(pos,False,True)
            state['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(*job['target'])),False)
            state['camera'].camera_component.set_field_of_view(job['fov'])
            assert state['buildings'][job['building']].prepare_details()
            state['ready']=time.monotonic()+6;state['phase']=2
        elif state['phase']==2 and time.monotonic()>state['ready']:
            state['phase']=4;job=jobs[state['index']]
            components=state['buildings'][job['building']].get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            matches=[c for c in components if c.static_mesh and c.static_mesh.get_name()==job['asset'] and c.get_instance_count()>0]
            assert matches,'Missing native resident furniture: '+job['asset']
            assert all(c.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION for c in matches)
            material_names=sorted({c.get_material(i).get_name() for c in matches for i in range(c.get_num_materials())})
            state['shots'].append(dict(**job,materials=material_names,collision='blocking'))
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1440,900,str(out/(job['name']+'.png')))
            state['phase']=3
        elif state['phase']==3 and state['task'].is_task_done():
            state['index']+=1
            if state['index']==len(jobs):finish()
            else:state['phase']=1
    except Exception as error:
        unreal.log_error(str(error));finish(str(error))


handle=unreal.register_slate_post_tick_callback(tick)
