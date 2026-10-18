"""Read-only scene inventory; writes source data, never edits either map."""
import json
import math
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()


def export_inventory():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level('/Game/CityCourier/Maps/L_CourierArtPreview')
    buildings = []
    found = set()
    expected = set()
    widths = [12000, 6000, 6000, 6000, 6000, 12000]
    for row in range(4):
        for col, interval in enumerate(widths):
            modules = max(1, round(((interval - 2000) / 3 - 180) / 800))
            for front in range(2):
                for number in range(3):
                    expected.update(f'CC_Art_Building_{row}_{col}_{front}_{number}_{i}' for i in range(modules))
    for actor in actors.get_all_level_actors():
        tag = next((str(t) for t in actor.tags if str(t).startswith('CC_Art_Building_')), None)
        if not tag:
            continue
        if tag in found:
            raise RuntimeError('Duplicate source building ' + tag)
        found.add(tag)
        suffix = tag.removeprefix('CC_Art_Building_')
        row, col, front, number, module = map(int, suffix.split('_'))
        scale, p = actor.get_actor_scale3d(), actor.get_actor_location()
        buildings.append(dict(id='B_' + suffix, source_tag=tag,
                              source_collision_tag=f'CC_City_Block_{row}_{col}_{front}_{number}',
                              source_asset=actor.static_mesh_component.static_mesh.get_path_name(),
                              source_yaw=actor.get_actor_rotation().yaw,
                              width=800 * abs(scale.x), depth=1150 * abs(scale.y),
                              location=[p.x, p.y, p.z], yaw=(actor.get_actor_rotation().yaw + 180) % 360,
                              variant=(row + col + number + module) % 3, kind='residential'))
    if found != expected:
        raise RuntimeError(f'Whole-city source coverage differs: missing={sorted(expected-found)}, extra={sorted(found-expected)}')

    city = unreal.load_asset('/Game/CityCourier/Data/DA_CityDefinition')
    bindings = []
    for i, (place, variant) in enumerate(zip(city.get_editor_property('merchants'), (0, 2, 1, 0))):
        p = place.position
        ident = f'B_Merchant_{i}'
        buildings.append(dict(id=ident, source_tag=f'CC_Art_Entrance_{i}', source_collision_tag=f'CC_Art_Entrance_{i}',
                              source_asset='/Game/CityCourier/Art/Architecture/SM_ShopHouse' + chr(65 + variant),
                              source_yaw=180, width=800, depth=1150,
                              location=[p.x, p.y + 776, 0], yaw=0, variant=variant, kind='shop'))
        bindings.append(dict(location_id=str(place.location_id), building_id=ident,
                             room_suffix='F01_A01', street_position=[p.x, p.y, p.z], role='merchant'))
    # A gate is an entrance structure rather than an enclosed multi-floor house.
    # Preserve its provenance and bind its address to a real nearby building.
    gateways = []
    for i, place in enumerate(city.get_editor_property('destinations')):
        p = place.position
        def distance(b):
            angle = math.radians(b['yaw'])
            x = b['location'][0] + math.sin(angle) * (b['depth'] / 2 + 140)
            y = b['location'][1] - math.cos(angle) * (b['depth'] / 2 + 140)
            return math.hypot(p.x - x, p.y - y)
        building = min((b for b in buildings if b['id'].startswith('B_') and not b['id'].startswith('B_Merchant')), key=distance)
        is_office = str(place.location_id).startswith(('office_', 'studio_'))
        if is_office:
            building['kind'] = 'office'
        floor_count = 1 + round(((1770, 1440, 2090)[building['variant']] - 410) / 330)
        room_suffix = f'F{floor_count:02}_B02' if is_office else f'F{floor_count:02}_A02'
        bindings.append(dict(location_id=str(place.location_id), building_id=building['id'], room_suffix=room_suffix,
                             street_position=[p.x, p.y, p.z], role='destination'))
        gateways.append(dict(source_tag=f'CC_Art_Entrance_{i+4}', building_id=building['id'],
                             role='address_gateway', enclosed_storeys=0))
    p = city.get_editor_property('station').position
    buildings.append(dict(id='B_Station', source_tag='CC_Art_Entrance_12', source_collision_tag='CC_Art_Entrance_12',
                          source_asset='/Game/CityCourier/Art/Architecture/SM_Shop', source_yaw=180,
                          width=650, depth=500, location=[p.x, p.y+450, 0], yaw=0, variant=0,
                          kind='station', storeys=1, height=400))
    bindings.append(dict(location_id='station', building_id='B_Station', room_suffix='F01_A01',
                         street_position=[p.x, p.y, p.z], role='station'))
    result = dict(schema_version=1, source_map='/Game/CityCourier/Maps/L_CourierArtPreview',
                  expected_source_tags=sorted(expected), expected_building_ids=sorted(b['id'] for b in buildings),
                  buildings=sorted(buildings, key=lambda b: b['id']), bindings=bindings, gateways=gateways)
    output = ROOT / 'AssetsSource/CityCourier/Interiors/inventory.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    unreal.log(f'BUILDING INVENTORY: {len(found)} street modules, {len(buildings)} buildings, {len(bindings)} address bindings, {len(gateways)} gateways')
    return result


if __name__ == '__main__':
    export_inventory()
