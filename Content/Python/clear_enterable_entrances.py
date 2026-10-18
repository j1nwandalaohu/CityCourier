"""Keep street props, collision and building inventory; clear real entry lanes."""
import json
import sys
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT))
from tools.interiors.layout import compile_building
from tools.interiors.entrance_clearance import entry_zones,relocate_props,overlaps


def geometry():
    source=json.loads((ROOT/'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
    zones,footprints=entry_zones([compile_building(b) for b in source['buildings']])
    city=unreal.load_asset('/Game/CityCourier/Data/DA_CityDefinition')
    graph=city.get_editor_property('roads')
    nodes={n.get_editor_property('id'):n.get_editor_property('position') for n in graph.get_editor_property('nodes')}
    roads=[((nodes[e.get_editor_property('from')].x,nodes[e.get_editor_property('from')].y),(nodes[e.get_editor_property('to')].x,nodes[e.get_editor_property('to')].y)) for e in graph.get_editor_property('edges')]
    return zones,footprints,roads


def props():
    result=[]; actors={}
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        mesh=actor.get_component_by_class(unreal.StaticMeshComponent)
        if not mesh or not mesh.static_mesh or not mesh.static_mesh.get_path_name().startswith('/Game/CityCourier/Art/StreetProps/'): continue
        if mesh.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION: continue
        center,extent=actor.get_actor_bounds(True)
        if extent.x<=0 or extent.y<=0: continue
        key=actor.get_actor_label(); p=actor.get_actor_location()
        assert key not in actors, 'Ambiguous street prop identity: '+key
        actors[key]=actor
        result.append(dict(id=key,position=(p.x,p.y,p.z),minimum=(center.x-extent.x,center.y-extent.y,center.z-extent.z),
                           maximum=(center.x+extent.x,center.y+extent.y,center.z+extent.z),tree='Planter' in mesh.static_mesh.get_name()))
    return sorted(result,key=lambda p:p['id']),actors


def validate_entry_props():
    zones,_,_=geometry(); current,_=props()
    return [p['id']+' blocks '+z['id'] for p in current for z in zones if overlaps(p,z)]


def clear_current_map(name):
    zones,footprints,roads=geometry(); before,actors=props()
    after,changes=relocate_props(before,zones,footprints,roads)
    assert len(before)==len(after)
    for change in changes:
        actor=actors[change['id']]
        assert actor.set_actor_location(unreal.Vector(*change['to_position']),False,True)
        unreal.log('ENTRY PROP CLEARANCE '+str(change))
    remaining=validate_entry_props(); assert not remaining, remaining[:10]
    path=ROOT/'AssetsSource/CityCourier/Interiors/entrance-prop-placement.json'
    records=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
    previous=records.get(name,[])
    records[name]=previous+changes
    path.write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return dict(props=len(before),moved=len(changes),external_doors=len(zones),errors=remaining)


if __name__=='__main__':
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    for name in ['L_CourierCity','L_CourierArtPreview']:
        assert levels.load_level('/Game/CityCourier/Maps/'+name)
        unreal.log('ENTRY CLEARANCE RESULT '+name+' '+str(clear_current_map(name)))
        assert levels.save_current_level()
