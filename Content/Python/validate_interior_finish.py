"""Check real UE collision and accelerated fixed-step traversal in an unsaved map.

This is actual character movement with capsule sweeps, steps, gravity and live
doors, with accelerated wall-clock scheduling. It is not a wall-clock playtest
or visual acceptance. Existing retained assets, catalog and maps are read only.
"""
import json
from datetime import datetime, timezone
from pathlib import Path
import sys
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Content/Python'))
sys.path.insert(0, str(ROOT))
from interior_finish_scene import (SOURCE_KIT, build_scene, protected_snapshot,
                                   snapshot_changes, world_placement)
from tools.interiors.finish_kit import placement_bounds


def _vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def _fixture_checks(record, mesh_definitions):
    errors, results = [], []
    building = record['plan']['building']
    for placement, actor in record['placements']:
        component = actor.static_mesh_component
        collision = str(component.get_collision_profile_name())
        enabled = component.get_collision_enabled()
        required = placement['collision']
        if collision != required:
            errors.append(placement['name']+': collision profile '+collision+' != '+required)
        if required == 'BlockAll' and enabled != unreal.CollisionEnabled.QUERY_AND_PHYSICS:
            errors.append(placement['name']+': blocking collision is not QueryAndPhysics')
        if required == 'NoCollision' and enabled != unreal.CollisionEnabled.NO_COLLISION:
            errors.append(placement['name']+': thin finish unexpectedly enables collision')
        mesh_bounds = component.static_mesh.get_bounding_box()
        imported_lo, imported_hi = _vector(mesh_bounds.min), _vector(mesh_bounds.max)
        source = mesh_definitions[placement['asset']]
        mesh_error = max(abs(a-b) for a, b in zip(imported_lo+imported_hi, source['minimum_cm']+source['maximum_cm']))
        expected_lo, expected_hi = placement_bounds(world_placement(building, placement), source)
        origin, extent, _sphere_radius = unreal.SystemLibrary.get_component_bounds(component)
        actual_lo, actual_hi = [origin.x-extent.x, origin.y-extent.y, origin.z-extent.z], [origin.x+extent.x, origin.y+extent.y, origin.z+extent.z]
        world_error = max(abs(a-b) for a, b in zip(actual_lo+actual_hi, expected_lo+expected_hi))
        if mesh_error > .5 or world_error > .5:
            errors.append(placement['name']+': actual UE bounds drift (asset %.4fcm, placed %.4fcm)' % (mesh_error, world_error))
        # Store full measured geometry for collision-bearing additions; thin
        # finishes still participate in all checks above without bloating report.
        if placement['category'] in ('furniture', 'handrail'):
            results.append(dict(name=placement['name'], asset=placement['asset'], room_id=placement.get('room_id'),
                                collision_profile=collision, collision_enabled=str(enabled),
                                expected_minimum_world_cm=expected_lo, expected_maximum_world_cm=expected_hi,
                                actual_minimum_world_cm=actual_lo, actual_maximum_world_cm=actual_hi,
                                asset_bounds_error_cm=mesh_error, placed_bounds_error_cm=world_error))
    return errors, results


def _write_report(report):
    text = json.dumps(report, ensure_ascii=False, indent=2)+'\n'
    for path in (SOURCE_KIT/'physical-validation.json', ROOT/'Saved/Validation/InteriorArt/physical-validation.json'):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')


def validate():
    before = protected_snapshot()
    report = dict(schema_version=1, status='running', passed=False, started_utc=datetime.now(timezone.utc).isoformat(),
                  no_city_maps_written=True, protected_before_sha256=before, samples=[], errors=[],
                  validation_method='native_character_movement_accelerated_fixed_steps_with_live_doors',
                  wall_clock_playtest=False, visual_acceptance=False, native_old_furniture_prepared=False,
                  fixture_bounds_tolerance_cm=.5)
    _write_report(report)
    failure = None
    try:
        scene = build_scene()
        for record in scene['samples']:
            sample = record['plan']
            actor = record['building_actor']
            fixture_errors, fixtures = _fixture_checks(record, scene['mesh_definitions'])
            room_errors = [str(e) for e in actor.get_physical_room_errors()]
            walk = actor.validate_physical_traversal(False)
            actual_rooms = sorted(str(r) for r in walk.visited_rooms)
            expected_rooms = sorted(sample['building']['expected_room_ids'])
            expected_doors = len(sample['building']['doors'])
            errors = fixture_errors + room_errors + [str(e) for e in walk.errors]
            if actual_rooms != expected_rooms:
                errors.append('Visited rooms differ from complete canonical expected_room_ids')
            if walk.opened_doors != expected_doors:
                errors.append('Opened doors differ from complete canonical door count')
            if not walk.returned_to_street:
                errors.append('Character did not return to street')
            result = dict(building_id=sample['building_id'], kind=sample['kind'],
                          expected_rooms=expected_rooms, actual_visited_rooms=actual_rooms,
                          expected_room_count=len(expected_rooms), actual_room_count=len(actual_rooms),
                          expected_doors=expected_doors, opened_doors=int(walk.opened_doors),
                          visited_nodes=int(walk.visited_nodes), walked_meters=float(walk.walked_meters),
                          returned_to_street=bool(walk.returned_to_street), room_errors=room_errors,
                          fixture_count=len(record['placements']), blocking_fixture_bounds=fixtures,
                          material_overrides=record['material_overrides'], errors=errors, passed=not errors)
            report['samples'].append(result)
            report['errors'].extend(sample['building_id']+': '+e for e in errors)
            _write_report(report)
            unreal.log('INTERIOR FINISH PHYSICAL '+sample['building_id']+': rooms %d/%d, doors %d/%d, returned=%s, errors=%s' %
                       (len(actual_rooms), len(expected_rooms), walk.opened_doors, expected_doors,
                        walk.returned_to_street, errors[:3]))
    except Exception:
        failure = traceback.format_exc()
        report['errors'].append(failure)
    after = protected_snapshot()
    changed = snapshot_changes(before, after)
    report.update(protected_after_sha256=after, protected_changed=changed,
                  protected_unchanged=not changed, finished_utc=datetime.now(timezone.utc).isoformat())
    if changed:
        report['errors'].append('Retained HEAD files changed: '+str(changed))
    if len(report['samples']) != 3:
        report['errors'].append('Expected exactly three complete sample validation results')
    report['passed'] = not report['errors']
    report['status'] = 'passed' if report['passed'] else 'failed'
    _write_report(report)
    if not report['passed']:
        raise RuntimeError('Interior finish physical validation failed: '+str(report['errors'][:8]))
    unreal.log('INTERIOR FINISH PHYSICAL VALIDATION PASSED; no retained packages or maps saved')
    return report


if __name__ == '__main__':
    validate()
