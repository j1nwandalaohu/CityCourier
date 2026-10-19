"""Idempotently migrate both maps after every canonical structure is prepared."""
import hashlib
import json
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Content/Python'))
from validate_enterable_city import validate_map
from clear_enterable_entrances import clear_current_map


def protected_snapshot(actors, exempt):
    result = {}
    for actor in actors.get_all_level_actors():
        tags = [str(t) for t in actor.tags]
        if any(t in exempt or t.startswith('CC_Enterable_') for t in tags):
            continue
        if not any(t.startswith(('CC_City_', 'CC_Bootstrap')) for t in tags):
            continue
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        t = actor.get_actor_transform()
        pose = [t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,
                t.scale3d.x,t.scale3d.y,t.scale3d.z]
        result[actor.get_actor_label()] = dict(transform=pose,
            mesh=comp.static_mesh.get_path_name() if comp and comp.static_mesh else None,
            collision=str(comp.get_collision_enabled()) if comp else None,
            location_id=str(actor.get_editor_property('location_id')) if isinstance(actor,unreal.DeliveryLocation) else None)
    return result


def build_map(name, catalog, source):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert levels.load_level('/Game/CityCourier/Maps/'+name)
    definitions = list(catalog.get_editor_property('buildings'))
    expected = set(source['expected_building_ids'])
    legacy_tags = set()
    for b in source['buildings']:
        legacy_tags.update([b['source_tag'], b['source_collision_tag']])
    legacy_tags.update(g['source_tag'] for g in source['gateways'])
    legacy_tags.update('CC_Production_Shop_'+str(i) for i in range(4))
    before = protected_snapshot(actors, legacy_tags)
    managed = {}
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.CourierBuildingActor):
            continue
        index = actor.get_editor_property('building_index')
        assert 0 <= index < len(definitions), 'Stale building actor has invalid source index'
        ident = str(definitions[index].get_editor_property('id'))
        assert ident not in managed, 'Duplicate managed building '+ident
        assert ident in expected, 'Unexpected managed building requires explicit source migration: '+ident
        managed[ident] = actor
    for i, b in enumerate(definitions):
        ident = str(b.get_editor_property('id'))
        actor = managed.get(ident)
        if actor is None:
            actor = actors.spawn_actor_from_class(unreal.CourierBuildingActor,
                b.get_editor_property('origin'), unreal.Rotator(yaw=b.get_editor_property('yaw')))
        assert actor.configure_building(catalog, i), 'Structure preparation failed: '+ident
        managed[ident] = actor
        if i % 25 == 0:
            unreal.log(f'INTERIOR STRUCTURES {name}: {i+1}/{len(definitions)}')
    assert set(managed) == expected, 'Refusing to replace original collision with an incomplete building set'
    # Only now replace original opaque volumes. Unrelated scene geometry, roads,
    # markers, traffic paths and manually authored actors remain unchanged.
    for actor in actors.get_all_level_actors():
        if not any(str(t) in legacy_tags for t in actor.tags):
            continue
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        if comp:
            comp.set_collision_profile_name('NoCollision')
            comp.set_visibility(False)
            actor.set_actor_hidden_in_game(True)
    assert protected_snapshot(actors, legacy_tags) == before, 'Unrelated gameplay actor changed during migration'
    clearance = clear_current_map(name)
    report = validate_map(catalog)
    report['entrance_prop_clearance'] = clearance
    report['protected_gameplay_digest'] = hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest()
    output = ROOT/f'Saved/Validation/{name}-interior-migration.json'
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if report['errors']:
        raise RuntimeError(f'Physical structure validation failed for {name}: '+str(report['errors'][:12])+'; full report: '+str(output))
    assert levels.save_current_level(), 'Failed to save complete interior map'
    unreal.log(f'INTERIOR MAP SAVED {name}: '+str({k:report[k] for k in ['actual_buildings','floors','rooms','doors','boxes']}))
    return report


if __name__ == '__main__':
    catalog = unreal.load_asset('/Game/CityCourier/Data/Buildings/DA_Buildings')
    assert catalog and catalog.validate_catalog()
    source = json.loads((ROOT/'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
    for name in ['L_CourierCity', 'L_CourierArtPreview']:
        build_map(name, catalog, source)
