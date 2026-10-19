"""Render native instance groups and their native lights in the production map.

Editor viewport evidence only: details are prepared explicitly at the three
sample sites, without saving the map or altering door states. No preview-piece
actors, supplemental light rig, shell cutaways or image edits are used.
"""
import json
from pathlib import Path
import sys
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(root/'Content/Python'))
import capture_interior_finish as capture


def scene():
    before=capture.protected_snapshot()
    assert capture.levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
    plan=json.loads((root/'AssetsSource/CityCourier/InteriorArt/sample-layout.json').read_text(encoding='utf-8'))
    manifest=json.loads((root/'AssetsSource/CityCourier/InteriorArt/asset-manifest.json').read_text(encoding='utf-8'))
    buildings={str(a.get_editor_property('catalog').get_editor_property('buildings')[a.get_editor_property('building_index')].get_editor_property('id')): a
               for a in capture.actors.get_all_level_actors() if isinstance(a,unreal.CourierBuildingActor)}
    assert len(buildings)==293
    records=[]
    for sample in plan['samples']:
        actor=buildings[sample['building_id']]
        assert actor.prepare_details()
        assert not actor.get_physical_room_errors()
        records.append(dict(plan=sample,building_actor=actor,placements=[(p,None) for p in sample['placements']]))
    return dict(world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),
        samples=records,protected_before=before,mesh_definitions={a['name']:a for a in manifest['assets']},
        source_map='/Game/CityCourier/Maps/L_CourierCity',native_details_explicitly_prepared=True)


capture.OUT=root/'Saved/Validation/CityFinish/Captures'
capture.RUN_OUT=capture.OUT/'runs'/capture.RUN_ID
capture.RUN_OUT.mkdir(parents=True,exist_ok=True)
capture.main(scene_builder=scene,
    lighting_builder=lambda _s:dict(source='native_runtime_fixture_lights_and_existing_map_lighting',supplemental_lights=0),
    door_preparer=lambda _s:None)
for job in capture.state['jobs']:
    if 'camera_checks' in job:
        job['camera_checks']=dict(selection_basis='approved sample camera candidates; native closed doors retained',
                                  actual_visibility_acceptance='manual inspection of exported images')
