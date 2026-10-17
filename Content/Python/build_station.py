"""Migrate only the generated station actor and add the R input mapping."""
import unreal
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
city = unreal.load_asset('/Game/CityCourier/Data/DA_CityDefinition')
tag = 'CC_City_Location_station'
existing = [a for a in actors.get_all_level_actors() if tag in [str(t) for t in a.tags]]
assert len(existing) == 1
old = existing[0]
if not isinstance(old, unreal.RiderStation):
    replacement = actors.spawn_actor_from_class(unreal.RiderStation, old.get_actor_location(), old.get_actor_rotation())
    replacement.set_editor_property('tags', old.tags)
    replacement.set_editor_property('location_id', old.get_editor_property('location_id'))
    replacement.set_editor_property('city_definition', city)
    replacement.get_editor_property('marker').set_material(0, old.get_editor_property('marker').get_material(0))
    actors.destroy_actor(old)
    replacement.set_actor_label(tag)
assert levels.save_current_level()
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = '/Game/CityCourier/Input/IA_Recover'
if assets.does_asset_exist(path):
    action = unreal.load_asset(path)
else:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.InputAction)
    action = tools.create_asset('IA_Recover', '/Game/CityCourier/Input', unreal.InputAction, factory)
action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
assets.save_loaded_asset(action)
context = unreal.load_asset('/Game/CityCourier/Input/IMC_Courier')
mappings = [m for m in context.get_editor_property('mappings') if m.action.get_name() != 'IA_Recover']
key = unreal.Key()
key.import_text('R')
mappings.append(unreal.EnhancedActionKeyMapping(action=action, key=key))
context.set_editor_property('mappings', mappings)
assets.save_loaded_asset(context)
unreal.log('CityCourier station and recovery input ready')
