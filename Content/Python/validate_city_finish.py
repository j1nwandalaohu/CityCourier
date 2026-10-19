"""Verify cooked art coverage and real traversal on every production building."""
import json
import os
from pathlib import Path
import sys
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(root/'Content/Python'))
from validate_enterable_city import validate_map

catalog=unreal.load_asset('/Game/CityCourier/Data/Buildings/DA_Buildings')
assert catalog and catalog.validate_catalog()
assert unreal.load_asset('/Game/CityCourier/Art/Interiors/DA_InteriorFinish')
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
reports={}
for name in os.environ.get('CC_FINISH_MAPS','L_CourierCity,L_CourierArtPreview').split(','):
    assert levels.load_level('/Game/CityCourier/Maps/'+name)
    records={}
    for actor in actors.get_all_level_actors():
        if not isinstance(actor,unreal.CourierBuildingActor): continue
        index=actor.get_editor_property('building_index')
        definition=catalog.get_editor_property('buildings')[index]
        ident=str(definition.get_editor_property('id'))
        # Real residency path, with HISM collision; no per-piece preview actors.
        # prepare/release are exposed through the traversal entry point.
        walk=actor.validate_physical_traversal(True)
        visited=sorted(str(x) for x in walk.visited_rooms)
        expected=sorted(str(x) for x in definition.get_editor_property('expected_room_ids'))
        errors=[str(x) for x in walk.errors]
        if visited!=expected or not walk.returned_to_street or walk.opened_doors!=len(definition.get_editor_property('doors')):
            errors.append('Incomplete room/door/return coverage')
        records[ident]=dict(rooms=len(visited),doors=int(walk.opened_doors),walked_meters=float(walk.walked_meters),
                            returned_to_street=bool(walk.returned_to_street),errors=errors)
        unreal.log('CITY FINISH WALK '+name+' '+str(len(records))+'/293 '+ident+' '+str(errors[:1]))
    structure=validate_map(catalog,physical=True,traversal=False)
    errors=structure['errors']+[ident+': '+str(r['errors']) for ident,r in records.items() if r['errors']]
    if len(records)!=len(catalog.get_editor_property('buildings')): errors.append('Missing building art validation')
    reports[name]=dict(passed=not errors, errors=errors,buildings=records,
        building_count=len(records),rooms=sum(r['rooms'] for r in records.values()),doors=sum(r['doors'] for r in records.values()),
        walked_meters=sum(r['walked_meters'] for r in records.values()),
        method='native_fixed_step_character_movement_live_doors_blocking_instanced_finish',wall_clock_performance_checked=False)
    out=root/'Saved/Validation/city-finish-runtime.json'
    out.write_text(json.dumps(reports,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    assert not errors,str(errors[:10])
unreal.log('CITY FINISH RUNTIME PASSED: '+str(out))
