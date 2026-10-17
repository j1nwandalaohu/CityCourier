"""Create the task-1 scene with UE's editor Python interpreter.

Safe to rerun: locate actors by a dedicated tag and never delete world content.
This makes a minimal engine-verification scene, not the final courier district.
"""
import unreal

MAP_PATH = "/Game/CityCourier/Maps/L_CourierCity"


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if assets.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("Cannot open existing startup map; nothing was replaced")
    else:
        if not levels.new_level(MAP_PATH):
            raise RuntimeError("Cannot create startup map")

    def ensure_actor(actor_class, tag, location, rotation=None):
        matches = [a for a in actors.get_all_level_actors()
                   if tag in [str(t) for t in a.tags]]
        if len(matches) > 1:
            raise RuntimeError("Duplicate managed actors: " + tag)
        if matches:
            actor = matches[0]
            if not isinstance(actor, actor_class):
                raise RuntimeError("Wrong class for managed actor: " + tag)
            return actor
        actor = actors.spawn_actor_from_class(
            actor_class, location, rotation or unreal.Rotator(0, 0, 0))
        if actor is None:
            raise RuntimeError("Actor creation failed: " + tag)
        actor.set_actor_label(tag)
        actor.set_editor_property("tags", [unreal.Name(tag)])
        return actor

    ground = ensure_actor(unreal.StaticMeshActor, "CC_BootstrapGround",
                          unreal.Vector(0, 0, -50))
    mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if mesh is None:
        raise RuntimeError("Engine cube mesh is unavailable")
    ground.static_mesh_component.set_static_mesh(mesh)
    ground.static_mesh_component.set_collision_profile_name("BlockAll")
    ground.set_actor_scale3d(unreal.Vector(100, 100, 1))

    ensure_actor(unreal.PlayerStart, "CC_BootstrapPlayerStart",
                 unreal.Vector(-500, 0, 150))
    sun = ensure_actor(unreal.DirectionalLight, "CC_BootstrapSun",
                       unreal.Vector(0, 0, 1500), unreal.Rotator(-35, -35, 0))
    sun.light_component.set_editor_property("intensity", 4.0)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    skylight = ensure_actor(unreal.SkyLight, "CC_BootstrapSkyLight",
                            unreal.Vector(0, 0, 1000))
    skylight.light_component.set_editor_property("intensity", 1.0)
    skylight.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    skylight.light_component.set_editor_property("real_time_capture", True)
    ensure_actor(unreal.SkyAtmosphere, "CC_BootstrapAtmosphere",
                 unreal.Vector(0, 0, 0))
    if not levels.save_current_level():
        raise RuntimeError("Startup map could not be saved")
    unreal.log("CityCourier: startup scene saved to " + MAP_PATH)


main()
