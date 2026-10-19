"""Whole-scene identity/geometry checks; never infer expected IDs from successes."""
import json
import hashlib
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Content/Python'))
from clear_enterable_entrances import validate_entry_props


def validate_map(catalog, physical=True, traversal=False):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    expected = set(str(x) for x in catalog.get_editor_property('expected_building_ids'))
    actual = {}
    definitions = list(catalog.get_editor_property('buildings'))
    entry_errors = validate_entry_props()
    errors = list(entry_errors)
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.CourierBuildingActor):
            continue
        index = actor.get_editor_property('building_index')
        if not 0 <= index < len(definitions):
            errors.append('Building actor has an invalid catalog index')
            continue
        definition = definitions[index]
        ident = str(definition.get_editor_property('id'))
        if ident in actual:
            errors.append('Duplicate building actor: '+ident)
        expected_boxes = len(definition.get_editor_property('boxes'))
        expected_doors = len(definition.get_editor_property('doors'))
        if actor.get_structural_box_count() != expected_boxes or actor.get_door_proxy_count() != expected_doors:
            errors.append('Incomplete resident structure or door proxies: '+ident)
        room_errors = list(actor.get_physical_room_errors()) if physical else []
        errors.extend(room_errors)
        walk = None
        if traversal:
            result = actor.validate_physical_traversal(True)
            visited = sorted(str(x) for x in result.visited_rooms)
            expected_rooms = sorted(str(x) for x in definition.get_editor_property('expected_room_ids'))
            walk = dict(visited_rooms=visited, expected_rooms=expected_rooms,
                        visited_nodes=result.visited_nodes, opened_doors=result.opened_doors,
                        walked_meters=result.walked_meters, returned_to_street=result.returned_to_street,
                        errors=list(result.errors))
            if visited != expected_rooms:
                walk['errors'].append('Actual movement coverage differs from canonical expected rooms: '+ident)
            if not walk['returned_to_street']:
                walk['errors'].append('Actual rider did not return to street: '+ident)
            errors.extend(walk['errors'])
            unreal.log(f'PHYSICAL WALK {len(actual)+1}/{len(expected)} {ident}: {len(visited)}/{len(expected_rooms)} rooms; errors={walk["errors"][:2]}')
        actual[ident] = dict(floors=len(definition.get_editor_property('floors')),
                             rooms=len(definition.get_editor_property('rooms')), doors=expected_doors,
                             boxes=expected_boxes, room_errors=room_errors, traversal=walk)
    for missing in sorted(expected - set(actual)):
        errors.append('Missing building: '+missing)
    for extra in sorted(set(actual) - expected):
        errors.append('Unexpected building: '+extra)
    return dict(expected_buildings=len(expected), actual_buildings=len(actual),
                floors=sum(x['floors'] for x in actual.values()), rooms=sum(x['rooms'] for x in actual.values()),
                doors=sum(x['doors'] for x in actual.values()), boxes=sum(x['boxes'] for x in actual.values()),
                buildings=actual, errors=errors, entrance_prop_errors=entry_errors, physical_standing_checked=physical,
                continuous_walking_checked=traversal,
                physically_visited_rooms=sum(len(x['traversal']['visited_rooms']) for x in actual.values()) if traversal else 0,
                physically_opened_doors=sum(x['traversal']['opened_doors'] for x in actual.values()) if traversal else 0,
                walked_meters=sum(x['traversal']['walked_meters'] for x in actual.values()) if traversal else 0,
                source_digest=catalog.get_editor_property('source_digest'),
                inventory_sha256=hashlib.sha256((ROOT/'AssetsSource/CityCourier/Interiors/inventory.json').read_bytes()).hexdigest(),
                passed=not errors)


if __name__ == '__main__':
    catalog = unreal.load_asset('/Game/CityCourier/Data/Buildings/DA_Buildings')
    assert catalog and catalog.validate_catalog()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    reports = {}
    for name in ['L_CourierCity', 'L_CourierArtPreview']:
        assert levels.load_level('/Game/CityCourier/Maps/'+name)
        reports[name] = validate_map(catalog,traversal=True)
    output = ROOT/'Saved/Validation/building-interior-coverage.json'
    output.write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
    assert all(r['passed'] for r in reports.values()), 'Incomplete whole-city interior coverage: '+str(output)
    unreal.log('WHOLE CITY STRUCTURAL COVERAGE PASSED: '+str(output))
