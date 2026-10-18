"""Whole-scene identity/geometry checks; never infer expected IDs from successes."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()


def validate_map(catalog, physical=True):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    expected = set(str(x) for x in catalog.get_editor_property('expected_building_ids'))
    actual = {}
    definitions = list(catalog.get_editor_property('buildings'))
    errors = []
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
        actual[ident] = dict(floors=len(definition.get_editor_property('floors')),
                             rooms=len(definition.get_editor_property('rooms')), doors=expected_doors,
                             boxes=expected_boxes, room_errors=room_errors)
    for missing in sorted(expected - set(actual)):
        errors.append('Missing building: '+missing)
    for extra in sorted(set(actual) - expected):
        errors.append('Unexpected building: '+extra)
    return dict(expected_buildings=len(expected), actual_buildings=len(actual),
                floors=sum(x['floors'] for x in actual.values()), rooms=sum(x['rooms'] for x in actual.values()),
                doors=sum(x['doors'] for x in actual.values()), boxes=sum(x['boxes'] for x in actual.values()),
                buildings=actual, errors=errors, physical_standing_checked=physical, passed=not errors)


if __name__ == '__main__':
    catalog = unreal.load_asset('/Game/CityCourier/Data/Buildings/DA_Buildings')
    assert catalog and catalog.validate_catalog()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    reports = {}
    for name in ['L_CourierCity', 'L_CourierArtPreview']:
        assert levels.load_level('/Game/CityCourier/Maps/'+name)
        reports[name] = validate_map(catalog)
    output = ROOT/'Saved/Validation/building-interior-coverage.json'
    output.write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
    assert all(r['passed'] for r in reports.values()), 'Incomplete whole-city interior coverage: '+str(output)
    unreal.log('WHOLE CITY STRUCTURAL COVERAGE PASSED: '+str(output))
