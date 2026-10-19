"""Promote preview street dressing without replacing canonical buildings or routes."""
import json
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Content/Python'))
from clear_enterable_entrances import validate_entry_props

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def transform_values(actor):
    t = actor.get_actor_transform()
    return [getattr(v, axis) for v, axes in ((t.translation, 'xyz'), (t.rotation, 'xyzw'), (t.scale3d, 'xyz')) for axis in axes]


def pose(actor):
    p, r, s = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
    return dict(location=[p.x, p.y, p.z], rotation=[r.pitch, r.yaw, r.roll], scale=[s.x, s.y, s.z])


def make_transform(p):
    return unreal.Transform(location=unreal.Vector(*p['location']),
        rotation=unreal.Rotator(pitch=p['rotation'][0], yaw=p['rotation'][1], roll=p['rotation'][2]),
        scale=unreal.Vector(*p['scale']))


assert levels.load_level('/Game/CityCourier/Maps/L_CourierArtPreview')
records = []
for actor in actors.get_all_level_actors():
    tag = next((str(t) for t in actor.tags if str(t).startswith('CC_Art_')), None)
    if not tag or tag.startswith(('CC_Art_Lamp_', 'CC_Art_LampLight_')) or actor.get_editor_property('hidden'):
        continue
    record = dict(tag=tag, cls=actor.get_class().get_path_name(), pose=pose(actor))
    if isinstance(actor, unreal.StaticMeshActor):
        c = actor.static_mesh_component
        if not c.is_visible() or not c.static_mesh:
            continue
        record.update(kind='mesh', mesh=c.static_mesh.get_path_name(),
                      materials=[m.get_path_name() if m else None for m in c.get_materials()])
    elif isinstance(actor, unreal.PointLight):
        c = actor.light_component
        record.update(kind='light', properties={p: c.get_editor_property(p) for p in
            ('intensity', 'attenuation_radius', 'cast_shadows', 'intensity_units')})
        color = c.get_editor_property('light_color')
        record['color'] = [color.r, color.g, color.b, color.a]
    elif isinstance(actor, unreal.ExponentialHeightFog):
        c = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
        record.update(kind='fog', properties={p: c.get_editor_property(p) for p in ('fog_density', 'fog_height_falloff')})
    elif isinstance(actor, unreal.PostProcessVolume):
        settings = actor.get_editor_property('settings')
        keys = ('auto_exposure_min_brightness', 'auto_exposure_max_brightness', 'bloom_intensity',
                'vignette_intensity', 'film_grain_intensity', 'ambient_occlusion_intensity')
        record.update(kind='post', settings={k: settings.get_editor_property(k) for k in keys})
        record['settings']['film_grain_intensity'] = 0
        saturation = settings.get_editor_property('color_saturation')
        record['saturation'] = [saturation.x, saturation.y, saturation.z, saturation.w]
    else:
        continue
    records.append(record)

# No UObject-backed struct views may outlive their source map. Only paths,
# numbers, lists and engine enum constants cross this boundary; reconstruct
# target struct values after load.
actor = c = settings = color = saturation = None
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
assert len({r['tag'] for r in records}) == len(records), 'Ambiguous preview art ownership'
managed = {str(t): a for a in actors.get_all_level_actors() for t in a.tags}
protected = {a.get_actor_label(): transform_values(a) for a in actors.get_all_level_actors()
             if isinstance(a, (unreal.CourierBuildingActor, unreal.DeliveryLocation))}
for r in records:
    if r['tag'].startswith('CC_Art_Market_'):
        index = int(r['tag'].rsplit('_', 1)[-1])
        legacy = managed.get('CC_Art_MarketStall_'+str(index))
        if legacy:
            r['pose'] = pose(legacy)
            r['pose']['location'][2] += 14
    actor = managed.get(r['tag'])
    if actor is None:
        actor = actors.spawn_actor_from_class(unreal.load_class(None, r['cls']), unreal.Vector())
    assert actor.get_class().get_path_name() == r['cls'], 'Managed actor class differs: ' + r['tag']
    actor.tags = [unreal.Name(r['tag'])]
    actor.set_actor_label(r['tag'])
    actor.set_actor_hidden_in_game(False)
    actor.set_actor_transform(make_transform(r['pose']), False, False)
    if r['kind'] == 'mesh':
        c = actor.static_mesh_component
        c.set_static_mesh(unreal.load_asset(r['mesh']))
        c.set_editor_property('override_materials', [unreal.load_asset(m) if m else None for m in r['materials']])
        c.set_collision_profile_name('NoCollision') # Same visual dressing contract as preview.
        c.set_visibility(True)
        c.set_editor_property('cast_shadow', r['tag'] != 'CC_Art_PhotographicSky')
        if r['tag'] == 'CC_Art_PhotographicSky':
            c.set_editor_property('affect_distance_field_lighting', False)
    elif r['kind'] == 'post':
        actor.set_editor_property('unbound', True)
        settings = actor.get_editor_property('settings')
        for key, value in r['settings'].items():
            settings.set_editor_property('override_'+key, True)
            settings.set_editor_property(key, value)
        settings.set_editor_property('override_color_saturation', True)
        settings.set_editor_property('color_saturation', unreal.Vector4(*r['saturation']))
        actor.set_editor_property('settings', settings)
    else:
        c = actor.light_component if r['kind'] == 'light' else actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
        if r['kind'] == 'light':
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_editor_property('light_color', unreal.Color(*r['color']))
        for key, value in r['properties'].items():
            c.set_editor_property(key, value)

# Update the seventh existing stall too. Keep the original physical footprint
# at every market position instead of drawing two interleaved rows of stalls.
for i in range(7):
    legacy = managed.get('CC_Art_MarketStall_'+str(i))
    if not legacy:
        continue
    legacy.static_mesh_component.set_visibility(False)
    legacy.set_actor_hidden_in_game(True)
    tag = 'CC_Art_Market_'+str(i)
    visual = next((a for a in actors.get_all_level_actors() if unreal.Name(tag) in a.tags), None)
    if visual is None:
        visual = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
        visual.tags = [unreal.Name(tag)]
        visual.set_actor_label(tag)
        p = pose(legacy)
        p['location'][2] += 14
        visual.set_actor_transform(make_transform(p), False, False)
        visual.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/CityCourier/Art/StreetProps/SM_MarketStall'))
        visual.static_mesh_component.set_collision_profile_name('NoCollision')

for tag, actor in managed.items():
    if tag.startswith(('CC_Art_FrontageSign_', 'CC_Art_Lamp_', 'CC_Art_LampLight_')):
        # Existing lamps already use the same delivered mesh. Keep their tested
        # collision positions; hide duplicate preview poles and floating old signs.
        actor.set_actor_hidden_in_game(True)

for actor in actors.get_all_level_actors():
    tags = [str(t) for t in actor.tags]
    c = actor.get_component_by_class(unreal.StaticMeshComponent)
    material = ('M_RoadWorld' if any(t.startswith('CC_City_Road_') for t in tags) else
                'M_PavementWorld' if 'CC_BootstrapGround' in tags or any(t.startswith('CC_City_Walk_') for t in tags) else None)
    if c and material:
        c.set_material(0, unreal.load_asset('/Game/CityCourier/Art/Materials/'+material))
    if isinstance(actor, unreal.SkyLight) and 'CC_BootstrapSkyLight' in tags:
        c = actor.light_component
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('real_time_capture', False)
        c.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        c.set_editor_property('cubemap', unreal.load_asset('/Game/CityCourier/Art/Textures/T_EveningPanorama'))
        # Lift shaded streets without raising exposure inside rooms.
        c.set_editor_property('intensity', 1.65)
    if isinstance(actor, unreal.DirectionalLight) and 'CC_BootstrapSun' in tags:
        actor.set_actor_rotation(unreal.Rotator(pitch=-12, yaw=-52), False)
        c = actor.light_component
        c.set_editor_property('intensity', 5.5)
        c.set_editor_property('use_temperature', True)
        c.set_editor_property('temperature', 4600)

after = {a.get_actor_label(): transform_values(a) for a in actors.get_all_level_actors()
         if isinstance(a, (unreal.CourierBuildingActor, unreal.DeliveryLocation))}
if after != protected:
    (ROOT/'Saved/Validation/environment-protected-difference.json').write_text(json.dumps(
        {k: {'before': protected.get(k), 'after': after.get(k)} for k in set(after)|set(protected)
         if after.get(k) != protected.get(k)}, indent=2), encoding='utf-8')
assert after == protected, 'Building or gameplay location changed'
assert not validate_entry_props(), 'Existing physical entrance clearance changed'
assert levels.save_current_level()
from validate_environment_integration import validate
validate()
(ROOT/'Saved/Validation/environment-art-integration.json').write_text(json.dumps(
    {'promoted_actors': len(records), 'protected_buildings_and_locations': len(protected),
     'protected_unchanged': True, 'entry_clearance_passed': True}, indent=2), encoding='utf-8')
