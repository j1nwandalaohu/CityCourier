"""Create the owned Epic graph; save a fresh receipt after every authorized stage."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, GENERATED_ROOT, GRAPH_VERSION, read_receipt
from migrate_epic_character_samples import sha256


def build_graph():
    import unreal
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    content = Path(unreal.Paths.project_content_dir()).resolve()
    receipt_path = REPORT_ROOT / "generation.json"
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    receipt = read_receipt("generation.json")
    for entry in receipt["outputs"]:
        filename = content / (entry["package"].removeprefix("/Game/") + ".uasset")
        if sha256(filename) != entry["sha256"]:
            raise RuntimeError("Generated asset changed: " + entry["package"])
    graph_path = GENERATED_ROOT + "/ABP_CourierEpic"
    if assets.does_asset_exist(graph_path):
        if receipt.get("graph_version") == GRAPH_VERSION:
            return dict(receipt, changed=[])
        if not receipt.get("graph_generated"):
            raise RuntimeError("Existing graph has no creation receipt")
    config = unreal.load_asset(GENERATED_ROOT + "/DA_CourierEpicAnimation")
    unreal.load_module("ControlRigEditor")
    rig_path = GENERATED_ROOT + "/CR_CourierContact"
    if not assets.does_asset_exist(rig_path):
        factory = unreal.ControlRigBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.CourierContactRig)
        rig = unreal.AssetToolsHelpers.get_asset_tools().create_asset("CR_CourierContact", GENERATED_ROOT, unreal.ControlRigBlueprint, factory)
        target = unreal.load_asset("/Game/CityCourier/Art/Characters/SK_Courier")
        rig.get_hierarchy_controller().import_bones_from_skeletal_mesh(target, "None", True, True, False, False, False)
        for name in ("LeftHandGoal", "RightHandGoal", "LeftFootGoal", "RightFootGoal"):
            rig.add_member_variable(name, "/Script/CoreUObject.Transform", True, False, "")
        for name in ("LeftHandAlpha", "RightHandAlpha", "LeftFootAlpha", "RightFootAlpha", "ContactPelvisOffset", "ContactEnabled"):
            rig.add_member_variable(name, "float", True, False, "1" if name == "ContactEnabled" else "0")
        rig.get_controller().add_unit_node_from_struct_path("/Script/ControlRig.RigUnit_BeginExecution", "Execute", unreal.Vector2D(0, 0), "Begin")
        rig.recompile_vm()
        assets.save_loaded_asset(rig)
    config.set_editor_property("contact_rig", unreal.load_class(None, rig_path + ".CR_CourierContact_C"))
    sequences = [ref for ref in config.get_editor_property("retargeted_sequences")]
    montages = [ref for ref in config.get_editor_property("retargeted_montages")]
    # Recovered exports may have been saved by RenameAssets before notify cleanup.
    if not unreal.CourierEpicArtBuilder.reuse_retargeted_assets(config, sequences + montages):
        raise RuntimeError("Cannot normalize generated courier animations")
    if unreal.CourierEpicArtBuilder.build_pose_search_databases(config) is None:
        raise RuntimeError("Pose Search index refresh failed")
    target = unreal.load_asset("/Game/CityCourier/Art/Characters/SK_Courier")
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CourierAnimInstance)
    factory.set_editor_property("target_skeleton", target.get_editor_property("skeleton"))
    blueprint = unreal.load_asset(graph_path) if assets.does_asset_exist(graph_path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset("ABP_CourierEpic", GENERATED_ROOT, unreal.AnimBlueprint, factory)
    if not blueprint or unreal.CourierEpicArtBuilder.build_animation_blueprint(config, blueprint) is None:
        raise RuntimeError("Epic graph generation failed; inspect compiler/pin diagnostics")
    defaults = unreal.get_default_object(blueprint.generated_class())
    if defaults.get_editor_property("epic_graph_version") != GRAPH_VERSION:
        raise RuntimeError("Editor binary did not generate the required grounded idle branch; rebuild before regenerating")
    for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
        if not assets.save_asset(path, only_if_is_dirty=True):
            raise RuntimeError("Cannot save " + path)
    receipt["outputs"] = []
    for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
        package = path.split(".")[0]
        receipt["outputs"].append({"package": package, "sha256": sha256(content / (package.removeprefix("/Game/") + ".uasset"))})
    receipt["graph_generated"] = True
    receipt["graph_version"] = GRAPH_VERSION
    receipt["player_enabled"] = False
    receipt["epic_graph_enabled"] = False
    receipt_path.write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    return receipt


if __name__ == "__main__":
    build_graph()
