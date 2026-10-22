"""Validate owned animation assets, connect the production player and build a lab."""
import hashlib
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, GENERATED_ROOT, GRAPH_VERSION, read_receipt
from migrate_epic_character_samples import sha256


def build_validation():
    import unreal
    content = Path(unreal.Paths.project_content_dir()).resolve()
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    receipt_path = REPORT_ROOT / "generation.json"
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    receipt = read_receipt("generation.json")
    for entry in receipt["outputs"]:
        if sha256(content / (entry["package"].removeprefix("/Game/") + ".uasset")) != entry["sha256"]:
            raise RuntimeError("Generated asset changed: " + entry["package"])
    if receipt.get("graph_version") != GRAPH_VERSION or receipt.get("physics_version") != 2:
        raise RuntimeError("Complete graph and target body generation first")
    player_path = "/Game/CityCourier/Blueprints/BP_CourierCharacter"
    player_file = content / "CityCourier/Blueprints/BP_CourierCharacter.uasset"
    map_path = "/Game/CityCourier/Maps/L_EpicCharacterValidation"
    map_file = content / "CityCourier/Maps/L_EpicCharacterValidation.umap"
    if receipt.get("player_enabled") and receipt.get("landing_version") == 1:
        if sha256(player_file) != receipt["player_sha256"] or sha256(map_file) != receipt["map_sha256"]:
            raise RuntimeError("Player or validation map was edited; preserve those edits")
        return dict(receipt, changed=[])
    existing_map = assets.does_asset_exist(map_path)
    if existing_map and (not receipt.get("map_sha256") or sha256(map_file) != receipt["map_sha256"]):
        raise RuntimeError("Validation map exists without an ownership receipt")
    if receipt.get("player_sha256") and sha256(player_file) != receipt["player_sha256"]:
        raise RuntimeError("Production player was edited after receipt")

    def finish_receipt(fingerprint):
        receipt["outputs"] = []
        for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
            package = path.split(".")[0]
            receipt["outputs"].append({"package": package, "sha256": sha256(content / (package.removeprefix("/Game/") + ".uasset"))})
        receipt.update(player_enabled=True, epic_graph_enabled=True, landing_version=1, asset_fingerprint=fingerprint,
                       player_sha256=sha256(player_file), map_sha256=sha256(map_file), pending=["final gameplay and rendered validation"])
        receipt_path.write_text(json.dumps(receipt, indent=2), encoding="utf-8")
        return receipt
    config = unreal.load_asset(GENERATED_ROOT + "/DA_CourierEpicAnimation")
    for prop, name in (("standing_landing", "CC_M_Neutral_Jump_F_Land_Stand_Light_Lfoot"),
                       ("moving_landing", "CC_M_Neutral_Jump_F_Land_Run_Light_Lfoot")):
        config.set_editor_property(prop, unreal.load_asset(GENERATED_ROOT + "/Clips/" + name))
    for prop, name in (("vault_montage", "CC_AM_M_Neutral_Traversal_Vault_1_0_walk_F_Rfoot"),
                       ("mantle_montage", "CC_AM_M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot")):
        config.set_editor_property(prop, unreal.load_asset(GENERATED_ROOT + "/Clips/" + name))
    fingerprint = hashlib.sha256(json.dumps([entry for entry in receipt["outputs"] if not entry["package"].endswith("DA_CourierEpicAnimation")], sort_keys=True).encode()).hexdigest()
    player = unreal.load_asset(player_path)
    result = unreal.CourierEpicArtBuilder.enable_on_player(config, player, fingerprint)
    if result is None:
        raise RuntimeError("Production player enable validation failed")
    if not assets.save_loaded_asset(config) or not assets.save_loaded_asset(player):
        raise RuntimeError("Cannot save validated player configuration")
    # Retain a recovery receipt before level generation, which can fail after
    # Unreal has already serialized the player/configuration packages.
    progress = dict(receipt, validation_phase="player_saved", player_sha256=sha256(player_file))
    progress["outputs"] = [{"package": entry["package"], "sha256": sha256(content / (entry["package"].removeprefix("/Game/") + ".uasset"))} for entry in receipt["outputs"]]
    (REPORT_ROOT / "validation-in-progress.json").write_text(json.dumps(progress, indent=2), encoding="utf-8")
    if existing_map:
        return finish_receipt(fingerprint)
    if not levels.new_level(map_path):
        raise RuntimeError("Cannot create validation level")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mode = unreal.load_asset("/Game/CityCourier/Blueprints/BP_CourierGameMode")
    world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())

    def spawn(cls, tag, location):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location))
        if not actor:
            raise RuntimeError("Cannot create " + tag)
        actor.set_actor_label(tag)
        actor.set_editor_property("tags", [unreal.Name("EpicCourierValidation"), unreal.Name(tag)])
        return actor

    def box(tag, location, scale):
        actor = spawn(unreal.StaticMeshActor, tag, location)
        actor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
        actor.static_mesh_component.set_collision_profile_name("BlockAll")
        actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    box("Ground", (0, 0, -50), (100, 100, 1))
    spawn(unreal.PlayerStart, "Start", (0, 0, 100))
    box("Vault90cm", (500, 0, 45), (.8, 3, .9))
    box("Mantle105cm", (500, 500, 52.5), (3, 3, 1.05))
    for index in range(4):
        height = 12 * (index + 1)
        box("Step" + str(index), (900 + index * 60, -500, height / 2), (.6, 2, height / 100))
    spawn(unreal.CourierGrabbable, "GrabCrate", (100, -100, 40))
    spawn(unreal.CourierInteractionButton, "HandButton", (150, 150, 110))
    spawn(unreal.CourierTrainingTarget, "PunchTarget", (0, 500, 90))
    spawn(unreal.ScooterPawn, "Scooter", (-150, -200, 52))
    sun = spawn(unreal.DirectionalLight, "Sun", (0, 0, 1200))
    sun.set_actor_rotation(unreal.Rotator(pitch=-35, yaw=-35), False)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("intensity", 4)
    sky = spawn(unreal.SkyLight, "Sky", (0, 0, 1000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, "Atmosphere", (0, 0, 0))
    if not levels.save_current_level():
        raise RuntimeError("Cannot save validation level")
    return finish_receipt(fingerprint)


if __name__ == "__main__":
    build_validation()
