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

blueprint = unreal.load_asset('/Game/CityCourier/Blueprints/ABP_Courier')
assert blueprint and unreal.CourierArtBuilder.build_animation_blueprint(blueprint), 'Action slot graph setup failed'
assets.save_loaded_asset(blueprint)
mesh = unreal.load_asset('/Game/CityCourier/Art/Characters/SK_Courier')
assets.save_loaded_asset(mesh.get_editor_property('skeleton'))
for name in ['A_JumpStart', 'A_JumpLoop', 'A_Land', 'A_LightPunch_L', 'A_LightPunch_R',
             'A_HeavyPunch_R', 'A_HitReact_Front', 'A_HitReact_Back', 'A_RunStart', 'A_RunStop']:
    if not assets.does_asset_exist('/Game/CityCourier/Art/Characters/' + name):
        unreal.log('Pending artist delivery: ' + name)
