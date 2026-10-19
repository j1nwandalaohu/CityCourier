"""Build an unsaved, transient scene from the three canonical finish samples.

Shared by capture and physical validation. No city level, data asset, mesh or
material is saved. Native structure/door collision remains intact; old native
detail furniture is never prepared. Call prepare_render_doors only AFTER any
physical traversal, because its proxy angles are deliberately changed for art.
"""
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT))
DEST = '/Game/CityCourier/Art/Interiors'
SOURCE_KIT = ROOT/'AssetsSource/CityCourier/InteriorArt'


def _hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def protected_snapshot():
    """Hash retained HEAD files only; parallel NEW art does not alter this set."""
    result = subprocess.run(['git', '-c', 'safe.directory='+ROOT.as_posix(), '-C', str(ROOT),
                             'ls-tree', '-r', '-z', '--name-only', 'HEAD', '--',
                             'Content/CityCourier/Maps', 'Content/CityCourier/Data',
                             'Content/CityCourier/Art', 'Source'],
                            check=True, capture_output=True)
    names = [name.decode('utf-8') for name in result.stdout.split(b'\0') if name]
    protected = {}
    for name in sorted(names):
        if name.startswith('Content/CityCourier/Art/Interiors/'):
            continue
        path = ROOT/name
        protected[name] = _hash(path) if path.is_file() else None
    return protected


def snapshot_changes(before, after):
    return [name for name in sorted(set(before) | set(after)) if before.get(name) != after.get(name)]


def local_to_world(building, point):
    angle = math.radians(building['yaw'])
    c, s = math.cos(angle), math.sin(angle)
    origin = building['location']
    return [origin[0] + c*point[0] - s*point[1],
            origin[1] + s*point[0] + c*point[1], origin[2] + point[2]]


def world_placement(building, placement):
    pitch, yaw, roll = placement['rotation_pyr']
    return {**placement, 'location_cm': local_to_world(building, placement['location_cm']),
            'rotation_pyr': [pitch, yaw + building['yaw'], roll]}


def load_required(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('Missing required interior sample asset: '+path)
    return asset


def spawn_mesh(actors, name, mesh, position, rotation_pyr=(0, 0, 0), scale=(1, 1, 1), collision='NoCollision'):
    pitch, yaw, roll = rotation_pyr
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,
        unreal.Vector(*position), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll), transient=True)
    if not actor:
        raise RuntimeError('Could not spawn sample mesh: '+name)
    actor.set_actor_label(name)
    actor.tags = [unreal.Name(name)]
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component.set_collision_profile_name(collision)
    component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision == 'BlockAll'
                                    else unreal.CollisionEnabled.NO_COLLISION)
    return actor


def _override_structure_materials(actor):
    mapping = {'M_Plaster': 'M_IF_Plaster', 'M_Concrete': 'M_IF_TileGrey',
               'M_PaintedMetal': 'M_IF_MetalDark', 'M_Wood': 'M_IF_WoodLight'}
    overrides = []
    for component in actor.get_editor_property('structure'):
        for index in range(component.get_num_materials()):
            old = component.get_material(index)
            replacement = mapping.get(old.get_name()) if old else None
            if replacement:
                # Component override only: neither shared cube mesh nor retained
                # materials are mutated, marked for save, or resaved.
                component.set_material(index, load_required(DEST+'/Materials/'+replacement))
                overrides.append(dict(component=component.get_name(), slot=index,
                                      source=old.get_name(), replacement=replacement))
    return overrides


def _assert_catalog_sample(definition, sample):
    b = sample['building']
    if str(definition.get_editor_property('id')) != b['id']:
        raise RuntimeError('Catalog sample ID mismatch: '+b['id'])
    origin = definition.get_editor_property('origin')
    transform_error = max(abs(a-v) for a, v in zip([origin.x, origin.y, origin.z], b['location']))
    if transform_error > .01 or abs(definition.get_editor_property('yaw')-b['yaw']) > .001:
        raise RuntimeError('Catalog/sample transform drift: '+b['id'])
    if sorted(str(x) for x in definition.get_editor_property('expected_room_ids')) != sorted(b['expected_room_ids']):
        raise RuntimeError('Catalog/sample expected room coverage drift: '+b['id'])
    if [str(d.get_editor_property('id')) for d in definition.get_editor_property('doors')] != [d['id'] for d in b['doors']]:
        raise RuntimeError('Catalog/sample door identities drift: '+b['id'])
    nodes = list(definition.get_editor_property('nodes'))
    if len(nodes) != len(b['nodes']):
        raise RuntimeError('Catalog/sample navigation count drift: '+b['id'])
    for actual, expected in zip(nodes, b['nodes']):
        point = actual.get_editor_property('position')
        if (str(actual.get_editor_property('id')) != expected['id']
                or max(abs(a-v) for a, v in zip([point.x, point.y, point.z], expected['position'])) > .01):
            raise RuntimeError('Catalog/sample navigation anchor drift: '+expected['id'])


def build_scene():
    """Return world, catalog, protected hashes and sample actor/placement pairs."""
    from tools.interiors.finish_kit import validate_plan

    before = protected_snapshot()
    plan = json.loads((SOURCE_KIT/'sample-layout.json').read_text(encoding='utf-8'))
    manifest = json.loads((SOURCE_KIT/'asset-manifest.json').read_text(encoding='utf-8'))
    definitions = {a['name']: a for a in manifest['assets']}
    errors = validate_plan(plan, definitions)
    if errors:
        raise RuntimeError('Source sample plan rejected: '+str(errors[:12]))
    catalog = load_required('/Game/CityCourier/Data/Buildings/DA_Buildings')
    if not catalog.validate_catalog():
        raise RuntimeError('Existing canonical building catalog is invalid')
    catalog_buildings = list(catalog.get_editor_property('buildings'))
    indices = {str(b.get_editor_property('id')): i for i, b in enumerate(catalog_buildings)}
    for sample in plan['samples']:
        if sample['building_id'] not in indices:
            raise RuntimeError('Existing sample absent from catalog: '+sample['building_id'])
        _assert_catalog_sample(catalog_buildings[indices[sample['building_id']]], sample)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(save_existing_map=False)
    if not world:
        raise RuntimeError('Could not create unsaved transient sample world')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # Engine cube spans +/-50cm. Ground top is exactly street z=0, beneath
    # native 40cm plinth floors and entry steps; one broad slab covers all sites.
    positions = [s['building']['location'] for s in plan['samples']]
    low = [min(p[i] for p in positions)-4000 for i in range(2)]
    high = [max(p[i] for p in positions)+4000 for i in range(2)]
    ground = spawn_mesh(actors, 'CC_IF_StreetGround', load_required('/Engine/BasicShapes/Cube'),
                        [(low[0]+high[0])/2, (low[1]+high[1])/2, -25],
                        scale=[(high[0]-low[0])/100, (high[1]-low[1])/100, .5], collision='BlockAll')
    ground.static_mesh_component.set_material(0, load_required(DEST+'/Materials/M_IF_TileGrey'))
    scene = dict(world=world, samples=[], catalog=catalog, protected_before=before,
                 ground=ground, mesh_definitions=definitions, actors=actors, plan=plan)
    mesh_cache = {}
    for sample in plan['samples']:
        b = sample['building']
        actor = actors.spawn_actor_from_class(unreal.CourierBuildingActor, unreal.Vector(*b['location']),
                                             unreal.Rotator(pitch=0, yaw=b['yaw'], roll=0), transient=True)
        if not actor or not actor.configure_building(catalog, indices[b['id']]):
            raise RuntimeError('Native sample structure failed: '+b['id'])
        actor.set_actor_label('CC_IF_'+b['id'])
        actor.tags = list(actor.tags) + [unreal.Name('CC_IF_'+b['id'])]
        if actor.get_structural_box_count() != len(b['boxes']) or actor.get_door_proxy_count() != len(b['doors']):
            raise RuntimeError('Incomplete native sample structure: '+b['id'])
        record = dict(plan=sample, building_actor=actor, placements=[],
                      material_overrides=_override_structure_materials(actor))
        for placement in sample['placements']:
            name = placement['asset']
            if name not in mesh_cache:
                mesh_cache[name] = load_required(DEST+'/'+name)
            if placement['category'] in ('furniture', 'handrail') and placement['collision'] != 'BlockAll':
                raise RuntimeError('Blocking sample fixture lost collision: '+placement['name'])
            p = world_placement(b, placement)
            placed = spawn_mesh(actors, 'CC_IF_'+placement['name'], mesh_cache[name], p['location_cm'],
                                p['rotation_pyr'], p['scale3'], p['collision'])
            record['placements'].append((placement, placed))
        scene['samples'].append(record)
        unreal.log('INTERIOR SAMPLE READY: '+b['id']+'; placements='+str(len(record['placements'])))
    changed = snapshot_changes(before, protected_snapshot())
    if changed:
        raise RuntimeError('Retained files changed while building transient samples: '+str(changed))
    return scene


def prepare_render_doors(scene, openness=1.0):
    """Capture-only open leaves; keep native proxy collision at matching angle.

    Do not call before physical validation. Native validation creates temporary
    real moving doors; visual leaves here intentionally have no collision.
    """
    if scene.get('render_door_visuals'):
        raise RuntimeError('Capture door visuals have already been prepared')
    if not 0 <= openness <= 1:
        raise ValueError('Door openness must be between zero and one')
    visuals = []
    for record in scene['samples']:
        sample = record['plan']
        b = sample['building']
        styles = {style['door_id']: style['leaf_asset'] for style in sample['door_styles']}
        proxy = record['building_actor'].get_editor_property('door_proxies')
        # RebuildStructure adds every door in canonical index order. Its private
        # bookkeeping array is not exposed to Python; verify each actual closed
        # proxy before using that freshly-built order instead of reaching into it.
        if proxy.get_instance_count() != len(b['doors']):
            raise RuntimeError('Render scene door proxy count changed before capture')
        for door_index, door in enumerate(b['doors']):
            initial = proxy.get_instance_transform(door_index, world_space=False)
            closed = math.radians(door['closed_yaw'])
            expected = [door['hinge'][0]+math.cos(closed)*door['width']/2,
                        door['hinge'][1]+math.sin(closed)*door['width']/2,
                        door['hinge'][2]+door['height']/2]
            if initial is None or max(abs(v-e) for v,e in zip(
                    [initial.translation.x,initial.translation.y,initial.translation.z], expected)) > .01:
                raise RuntimeError('Fresh proxy order/closed transform differs: '+door['id'])
            angle = door['closed_yaw'] + door['open_angle']*openness
            a = math.radians(angle)
            h = door['hinge']
            center = [h[0]+math.cos(a)*door['width']/2, h[1]+math.sin(a)*door['width']/2, h[2]+door['height']/2]
            transform = unreal.Transform(location=unreal.Vector(*center),
                rotation=unreal.Rotator(pitch=0, yaw=angle, roll=0),
                scale=unreal.Vector(door['width']/100, .05, door['height']/100))
            if not proxy.update_instance_transform(door_index, transform, world_space=False,
                                                   mark_render_state_dirty=True, teleport=True):
                raise RuntimeError('Could not position native render door proxy: '+door['id'])
            visual = spawn_mesh(scene['actors'], 'CC_IF_VisualLeaf_'+door['id'],
                load_required(DEST+'/'+styles[door['id']]), local_to_world(b, h),
                [0, angle+b['yaw'], 0], [door['width']/100, 1, door['height']/220])
            visuals.append(visual)
        proxy.set_visibility(False, propagate_to_children=False)
    scene['render_door_visuals'] = visuals
    return visuals
