"""PIE evidence of native, resident first-floor shop furniture; no map writes."""
import json
import math
from pathlib import Path
import sys
import time
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(root))
from tools.interiors.finish_kit import _sample, placement_bounds

source = root/'AssetsSource/CityCourier'
inventory = json.loads((source/'Interiors/inventory.json').read_text(encoding='utf-8'))
bounds = {a['name']:a for a in json.loads((source/'InteriorArt/asset-manifest.json').read_text())['assets']}
out = root/'Saved/Validation/ShopFurnishings/Captures'
out.mkdir(parents=True,exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
camera = actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector())
camera.tags = [unreal.Name('ShopFurnishingCapture')]
camera.camera_component.set_field_of_view(95)
jobs = []
for ident in ('B_Merchant_0','B_Merchant_1','B_Merchant_2','B_1_0_0_0_0'):
    seed = next(s for s in inventory['buildings'] if s['id']==ident)
    sample = _sample(seed,bounds)
    b = sample['building']
    room = next(r for r in b['rooms'] if r['floor']==1 and r['kind']=='shop')
    angle = math.radians(b['yaw'])
    def world(p):
        return [b['location'][0]+math.cos(angle)*p[0]-math.sin(angle)*p[1],
                b['location'][1]+math.sin(angle)*p[0]+math.cos(angle)*p[1], b['location'][2]+p[2]]
    for p in sample['placements']:
        if p.get('room_id')!=room['id'] or not p['asset'].startswith('SM_IF_Shop'):
            continue
        lo,hi = placement_bounds(p,bounds[p['asset']])
        # Back up within the actual room to include tall hoods/displays, while
        # keeping the camera out of every blocking furniture assembly.
        midpoint = [(lo[i]+hi[i])/2 for i in range(2)]
        direction = [room['center'][i]-midpoint[i] for i in range(2)]
        length = max(math.hypot(*direction),1)
        position = room['center'][:2]
        furniture = [placement_bounds(q,bounds[q['asset']]) for q in sample['placements']
                     if q.get('room_id')==room['id'] and q['category']=='furniture']
        for distance in (40,70,100,150):
            candidate = [max(room['minimum'][i]+25,min(room['maximum'][i]-25,
                         room['center'][i]+direction[i]/length*distance)) for i in range(2)]
            if not any(all(a[i]-20<=candidate[i]<=z[i]+20 for i in range(2)) for a,z in furniture):
                position = candidate
        jobs.append(dict(name=ident+'-'+p['asset'],building=ident,asset=p['asset'],
            pos=world(position+[room['minimum'][2]+160]),
            target=world([(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,lo[2]+min(120,(hi[2]-lo[2])/2)])))
state = dict(phase=0,index=0,shots=[],finished=False)
start = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()


def finish(error=None):
    if state['finished']:
        return
    state['finished'] = True
    (out/'result.json').write_text(json.dumps(dict(error=error,shots=state['shots']),indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    actors.destroy_actor(camera)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    try:
        if time.monotonic()-start>240:
            raise RuntimeError('Shop capture timeout')
        world = editor.get_game_world()
        if not world or state['finished']:
            return
        if state['phase']==0:
            pc = unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():
                return
            state['pawn'] = pc.get_controlled_pawn()
            state['pawn'].character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state['pawn'].set_actor_hidden_in_game(True)
            state['camera'] = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
                                   if 'ShopFurnishingCapture' in [str(t) for t in c.tags])
            state['buildings'] = {str(a.catalog.buildings[a.building_index].id):a for a in
                unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CourierBuildingActor)}
            pc.set_view_target_with_blend(state['camera'],0)
            for cmd in ('t.MaxFPS 30','t.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            state['phase'] = 1
        elif state['phase']==1:
            j = jobs[state['index']]
            pos = unreal.Vector(*j['pos'])
            state['pawn'].set_actor_location(pos,False,True)
            state['camera'].set_actor_location(pos,False,True)
            state['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(*j['target'])),False)
            assert state['buildings'][j['building']].prepare_details()
            state['ready'] = time.monotonic()+6
            state['phase'] = 2
        elif state['phase']==2 and time.monotonic()>state['ready']:
            state['phase'] = 4  # Screenshot dispatch can recursively pump Slate.
            j = jobs[state['index']]
            components = state['buildings'][j['building']].get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            matches = [c for c in components if c.static_mesh and c.static_mesh.get_name()==j['asset'] and c.get_instance_count()>0]
            assert matches, 'Native resident shop assembly missing: '+j['name']
            assert all(c.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION for c in matches)
            state['shots'].append(j)
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(out/(j['name']+'.png')))
            state['phase'] = 3
        elif state['phase']==3 and state['task'].is_task_done():
            state['index'] += 1
            if state['index']==len(jobs):
                finish()
            else:
                state['phase'] = 1
    except Exception as error:
        unreal.log_error(str(error))
        finish(str(error))


handle = unreal.register_slate_post_tick_callback(tick)
