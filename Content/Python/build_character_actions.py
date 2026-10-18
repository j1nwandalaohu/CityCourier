"""Idempotent functional action input setup; never reimport the artist's meshes."""
import unreal

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
root = '/Game/CityCourier/Input'
context = unreal.load_asset(root + '/IMC_Courier')
assert context, 'Existing courier input context is required'
new_actions = {}
for name in ['Jump', 'LightPunch', 'HeavyPunch']:
    path = root + '/IA_' + name
    action = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if not action:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.InputAction)
        action = tools.create_asset('IA_' + name, root, unreal.InputAction, factory)
    action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    assets.save_loaded_asset(action)
    new_actions[name] = action

mappings = [m for m in context.get_editor_property('mappings')
            if m.get_editor_property('action') not in new_actions.values()]
for name, key_name in [('Jump', 'SpaceBar'), ('LightPunch', 'LeftMouseButton'), ('HeavyPunch', 'RightMouseButton')]:
    key = unreal.Key()
    key.import_text(key_name)
    mappings.append(unreal.EnhancedActionKeyMapping(action=new_actions[name], key=key))
context.set_editor_property('mappings', mappings)
assets.save_loaded_asset(context)
unreal.log('Courier action input assets saved; existing vehicle/UI mappings retained')
