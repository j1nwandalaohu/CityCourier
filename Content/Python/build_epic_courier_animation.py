"""Editor-only retarget generation; requires the new CityCourier editor module.

This step intentionally does not enable the Epic graph. Rig, graph, traversal
selection and final rendered validation are later gates of the approved plan.
"""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, GENERATED_ROOT, read_receipt
from migrate_epic_character_samples import sha256


def build_retarget_assets():
    import unreal
    if not hasattr(unreal, "CourierEpicArtBuilder"):
        raise RuntimeError("New CityCourier native editor helper requires an authorized Editor build")
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    inventory = read_receipt("gasp.json")
    migration = read_receipt("migration.json")
    if not inventory["closure_complete"]:
        raise RuntimeError("Incomplete GASP source audit")
    project_content = Path(unreal.Paths.project_content_dir()).resolve()
    for entry in migration["packages"]:
        path = project_content / (entry["destination"].removeprefix("/Game/") + ".uasset")
        if sha256(path) != entry["destination_sha256"]:
            raise RuntimeError("Imported source package edited after migration: " + str(path))
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    existing = assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False)
    completed_path = REPORT_ROOT / "generation.json"
    try:
        completed = read_receipt("generation.json") if existing else None
    except FileNotFoundError:
        completed = None
    if completed is not None:
        receipts = {entry["package"]: entry["sha256"] for entry in completed["outputs"]}
        for path in existing:
            package = path.split(".")[0]
            filename = project_content / (package.removeprefix("/Game/") + ".uasset")
            if package not in receipts or sha256(filename) != receipts[package]:
                raise RuntimeError("Generated asset changed after receipt: " + path)
        return dict(completed, changed=[])
    progress_path = REPORT_ROOT / "generation-in-progress.json"
    progress = json.loads(progress_path.read_text(encoding="utf-8")) if progress_path.exists() else {}
    prior_outputs = {e["package"]: e for e in progress.get("outputs", [])}
    for path in existing:
        package = path.split(".")[0]
        filename = project_content / (package.removeprefix("/Game/") + ".uasset")
        if package not in prior_outputs or sha256(filename) != prior_outputs[package]["sha256"]:
            raise RuntimeError("Existing generated asset has no matching ownership receipt: " + path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    def create(name, cls, factory):
        asset = tools.create_asset(name, GENERATED_ROOT, cls, factory)
        if not asset:
            raise RuntimeError("Asset creation failed: " + name)
        return asset

    source = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
    target = unreal.load_asset("/Game/CityCourier/Art/Characters/SK_Courier")
    if not source or not target:
        raise RuntimeError("Source mannequin and existing courier mesh are required")
    rig_factory = unreal.IKRigDefinitionFactory()
    source_rig = create("IK_SourceUEFN", unreal.IKRigDefinition, rig_factory)
    target_rig = create("IK_Courier", unreal.IKRigDefinition, rig_factory)
    chains = [
        ("Spine", "spine_01", "spine_05", "spine", "chest"),
        ("Head", "neck_01", "head", "neck", "head"),
        ("LeftArm", "upperarm_l", "hand_l", "upperarm_l", "hand_l"),
        ("RightArm", "upperarm_r", "hand_r", "upperarm_r", "hand_r"),
        ("LeftLeg", "thigh_l", "foot_l", "thigh_l", "foot_l"),
        ("RightLeg", "thigh_r", "foot_r", "thigh_r", "foot_r"),
    ]
    for rig, mesh, offset in [(source_rig, source, 0), (target_rig, target, 2)]:
        controller = unreal.IKRigController.get_controller(rig)
        if not controller.set_skeletal_mesh(mesh) or not controller.set_retarget_root("pelvis"):
            raise RuntimeError("Rig setup rejected mesh/pelvis: " + mesh.get_path_name())
        for name, src_start, src_end, dst_start, dst_end in chains:
            start, end = (src_start, src_end) if offset == 0 else (dst_start, dst_end)
            added = controller.add_retarget_chain(name, start, end, "None")
            if str(added) == "None":
                raise RuntimeError("Missing courier/UEFN chain bones: " + name)
    retargeter = create("RTG_UEFN_Courier", unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CourierEpicAnimationConfig)
    config = create("DA_CourierEpicAnimation", unreal.CourierEpicAnimationConfig, factory)
    config.set_editor_property("source_mesh", source)
    config.set_editor_property("target_mesh", target)
    config.set_editor_property("retargeter", retargeter)
    selected = set(inventory["selection"])
    animations = [unreal.load_asset(e["package"]) for e in inventory["assets"]
                  if e["package"] in selected and e["class"] in {"AnimSequence", "AnimMontage"}]
    if not animations or any(a is None for a in animations):
        raise RuntimeError("Audited animation selection cannot be loaded")
    config.set_editor_property("source_animations", animations)
    if existing:
        clips = [unreal.load_asset(path) for path in existing]
        if not unreal.CourierEpicArtBuilder.reuse_retargeted_assets(config, clips):
            raise RuntimeError("Recovered clips do not match the courier skeleton")
        result = unreal.CourierEpicArtBuilder.build_pose_search_databases(config)
    else:
        progress_path.write_text(json.dumps({"phase": "started", "outputs": []}, indent=2), encoding="utf-8")
        result = unreal.CourierEpicArtBuilder.build_retarget_assets(config)
    # Unreal exposes bool + one out parameter as Text on success, None on failure.
    if result is None:
        raise RuntimeError("Retarget/database generation failed; inspect the native log")
    for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
        if not assets.save_asset(path, only_if_is_dirty=False):
            raise RuntimeError("Cannot save generated asset: " + path)
    outputs = []
    for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
        package = path.split(".")[0]
        filename = project_content / (package.removeprefix("/Game/") + ".uasset")
        outputs.append({"package": package, "sha256": sha256(filename)})
    report = {"outputs": outputs, "retarget_generated": True, "epic_graph_enabled": False,
              "pending": ["graph", "chooser", "traversal montage selection", "contact rig", "physics", "rendered validation"]}
    (REPORT_ROOT / "generation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


if __name__ == "__main__":
    build_retarget_assets()
