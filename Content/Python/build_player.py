"""Create native-backed player blueprints, Enhanced Input assets and a movement test map."""
import unreal

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
root = "/Game/CityCourier"


def asset(name, folder, cls, factory):
    path = root + "/" + folder + "/" + name
    return unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name, root + "/" + folder, cls, factory)


actions = {}
def data_factory(cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return factory

for name, value_type in [("Move", unreal.InputActionValueType.AXIS2D), ("Look", unreal.InputActionValueType.AXIS2D),
                         ("Sprint", unreal.InputActionValueType.BOOLEAN), ("Interact", unreal.InputActionValueType.BOOLEAN),
                         ("Crouch", unreal.InputActionValueType.BOOLEAN),
                         ("Prone", unreal.InputActionValueType.BOOLEAN),
                         ("Brake", unreal.InputActionValueType.BOOLEAN), ("Phone", unreal.InputActionValueType.BOOLEAN),
                         ("Map", unreal.InputActionValueType.BOOLEAN), ("Pause", unreal.InputActionValueType.BOOLEAN),
                         ("Recover", unreal.InputActionValueType.BOOLEAN),
                         ("Jump", unreal.InputActionValueType.BOOLEAN),
                         ("LightPunch", unreal.InputActionValueType.BOOLEAN),
                         ("HeavyPunch", unreal.InputActionValueType.BOOLEAN)]:
    action = asset("IA_" + name, "Input", unreal.InputAction, data_factory(unreal.InputAction))
    action.set_editor_property("value_type", value_type)
    if name in ["Phone", "Map", "Pause"]:
        action.set_editor_property("trigger_when_paused", True)
    assets.save_loaded_asset(action)
    actions[name] = action

context = asset("IMC_Courier", "Input", unreal.InputMappingContext, data_factory(unreal.InputMappingContext))
mappings = []
for name, key, swizzle, negate in [("Move", "W", True, False), ("Move", "S", True, True),
        ("Move", "D", False, False), ("Move", "A", False, True),
        ("Look", "MouseX", False, False), ("Look", "MouseY", True, True),
        ("Sprint", "RightShift", False, False), ("Prone", "LeftShift", False, False), ("Crouch", "LeftControl", False, False), ("Interact", "E", False, False), ("Brake", "SpaceBar", False, False),
        ("Jump", "SpaceBar", False, False), ("LightPunch", "LeftMouseButton", False, False), ("HeavyPunch", "RightMouseButton", False, False),
        ("Phone", "Tab", False, False), ("Map", "M", False, False), ("Pause", "Escape", False, False), ("Recover", "R", False, False)]:
    modifiers = []
    if negate:
        modifiers.append(unreal.new_object(unreal.InputModifierNegate, context))
    if swizzle:
        modifier = unreal.new_object(unreal.InputModifierSwizzleAxis, context)
        modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
        modifiers.append(modifier)
    input_key = unreal.Key()
    input_key.import_text(key)
    mappings.append(unreal.EnhancedActionKeyMapping(action=actions[name], key=input_key, modifiers=modifiers))
context.set_editor_property("mappings", mappings)
assets.save_loaded_asset(context)


def blueprint(name, parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return asset(name, "Blueprints", unreal.Blueprint, factory)


character = blueprint("BP_CourierCharacter", unreal.CourierCharacter)
char_defaults = unreal.get_default_object(character.generated_class())
for part in char_defaults.get_editor_property("prototype_parts"):
    part.set_material(0, unreal.load_asset(root + "/Materials/M_GrayboxMerchant"))
unreal.BlueprintEditorLibrary.compile_blueprint(character)
assets.save_loaded_asset(character)
mode = blueprint("BP_CourierGameMode", unreal.CourierGameMode)
unreal.get_default_object(mode.generated_class()).set_editor_property("default_pawn_class", character.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(mode)
assets.save_loaded_asset(mode)

levels.load_level(root + "/Maps/L_CourierCity")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
levels.save_current_level()

test_map = root + "/Maps/L_MovementTest"
if assets.does_asset_exist(test_map):
    levels.load_level(test_map)
else:
    levels.new_level(test_map)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
existing = {str(tag): actor for actor in actors.get_all_level_actors() for tag in actor.tags}


def ensure(cls, tag, position, scale=None):
    actor = existing.get(tag) or actors.spawn_actor_from_class(cls, unreal.Vector(*position))
    actor.set_actor_label(tag)
    actor.set_editor_property("tags", [unreal.Name(tag)])
    actor.set_actor_location(unreal.Vector(*position), False, False)
    if scale:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


for tag, position, scale in [("CC_TestFloor", (0,0,-50), (120,120,1)),
        ("CC_TestWallLeft", (2000,-190,150), (30,.5,3)),
        ("CC_TestWallRight", (2000,190,150), (30,.5,3)),
        ("CC_TestWallEnd", (3500,0,150), (.5,4,3))]:
    actor = ensure(unreal.StaticMeshActor, tag, position, scale)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    actor.static_mesh_component.set_material(0, unreal.load_asset(root + "/Materials/M_GrayboxSidewalk"))
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
ensure(unreal.PlayerStart, "CC_TestStart", (0,0,100))
target = ensure(unreal.DeliveryLocation, "CC_TestInteraction", (600,0,14))
target.set_editor_property("location_id", unreal.Name("test_door"))
target.get_editor_property("marker").set_material(0, unreal.load_asset(root + "/Materials/M_GrayboxDestination"))
sun = ensure(unreal.DirectionalLight, "CC_TestSun", (0,0,1200))
sun.set_actor_rotation(unreal.Rotator(pitch=-35, yaw=-35, roll=0), False)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property("intensity", 4.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = ensure(unreal.SkyLight, "CC_TestSky", (0,0,1000))
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("real_time_capture", True)
ensure(unreal.SkyAtmosphere, "CC_TestAtmosphere", (0,0,0))
levels.save_current_level()
unreal.log("CityCourier player assets and movement map saved")
