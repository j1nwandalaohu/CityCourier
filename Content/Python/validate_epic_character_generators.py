"""Repeat all generators and prove the delivered assets remain byte-identical."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, read_receipt
from migrate_epic_character_samples import sha256
from build_epic_courier_animation import build_retarget_assets
from build_epic_courier_graph import build_graph
from build_epic_courier_physics import build_physics
from build_epic_character_validation import build_validation


def validate_generators():
    root = Path(__file__).resolve().parents[2]
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    receipt = read_receipt("generation.json")
    files = [root / "Content" / (entry["package"].removeprefix("/Game/") + ".uasset") for entry in receipt["outputs"]]
    files += [root / "Content/CityCourier/Blueprints/BP_CourierCharacter.uasset", root / "Content/CityCourier/Maps/L_EpicCharacterValidation.umap"]
    before = {str(path.relative_to(root)): sha256(path) for path in files}
    stages = {}
    for function in (build_retarget_assets, build_graph, build_physics, build_validation):
        result = function()
        if result.get("changed") != []:
            raise RuntimeError("Generator did not take the verified reuse path: " + function.__name__)
        stages[function.__name__] = "unchanged"
    after = {str(path.relative_to(root)): sha256(path) for path in files}
    if before != after:
        raise RuntimeError("Generator repeat changed delivered assets")
    result = {"stages": stages, "asset_count": len(files), "asset_fingerprint": receipt.get("asset_fingerprint"), "identical": True}
    (REPORT_ROOT / "idempotence.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    validate_generators()
