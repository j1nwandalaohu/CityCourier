"""Author prototype scooter blueprint and place one in each project test/play map."""
import unreal

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
path = "/Game/CityCourier/Blueprints/BP_Scooter"
if assets.does_asset_exist(path):
    blueprint = unreal.load_asset(path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.ScooterPawn)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_Scooter", "/Game/CityCourier/Blueprints", unreal.Blueprint, factory)
defaults = unreal.get_default_object(blueprint.generated_class())
for i, part in enumerate(defaults.get_editor_property("prototype_parts")):
    material = "M_GrayboxRoad" if i in [1,4,5] else "M_GrayboxMerchant"
    part.set_material(0, unreal.load_asset("/Game/CityCourier/Materials/" + material))
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
assets.save_loaded_asset(blueprint)
for name, position in [("L_MovementTest", (120,0,52)), ("L_CourierCity", (950,-11550,66))]:
    if not levels.load_level("/Game/CityCourier/Maps/" + name):
        raise RuntimeError("Missing map " + name)
    matches = [actor for actor in actors.get_all_level_actors() if "CC_Scooter" in [str(t) for t in actor.tags]]
    if len(matches) > 1:
        raise RuntimeError("Duplicate project scooter")
    scooter = matches[0] if matches else actors.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(*position))
    scooter.set_editor_property("tags", [unreal.Name("CC_Scooter")])
    scooter.set_actor_label("CC_Scooter")
    scooter.set_actor_location(unreal.Vector(*position), False, False)
    scooter.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
    levels.save_current_level()
unreal.log("CityCourier prototype scooters placed")
