"""Build target skeleton physics bodies, retaining generated asset ownership."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, GENERATED_ROOT, read_receipt
from migrate_epic_character_samples import sha256


def build_physics():
    import unreal
    content = Path(unreal.Paths.project_content_dir()).resolve()
    receipt_path = REPORT_ROOT / "generation.json"
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    receipt = read_receipt("generation.json")
    for entry in receipt["outputs"]:
        if sha256(content / (entry["package"].removeprefix("/Game/") + ".uasset")) != entry["sha256"]:
            raise RuntimeError("Generated asset changed: " + entry["package"])
    if receipt.get("physics_version") == 2:
        return dict(receipt, changed=[])
    config = unreal.load_asset(GENERATED_ROOT + "/DA_CourierEpicAnimation")
    if unreal.CourierEpicArtBuilder.build_physics_asset(config) is None:
        raise RuntimeError("Physics generation failed")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
        if not assets.save_asset(path, only_if_is_dirty=True):
            raise RuntimeError("Cannot save " + path)
    receipt["outputs"] = []
    for path in assets.list_assets(GENERATED_ROOT, recursive=True, include_folder=False):
        package = path.split(".")[0]
        receipt["outputs"].append({"package": package, "sha256": sha256(content / (package.removeprefix("/Game/") + ".uasset"))})
    receipt["physics_generated"] = True
    receipt["physics_version"] = 2
    receipt_path.write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    return receipt


if __name__ == "__main__":
    build_physics()
