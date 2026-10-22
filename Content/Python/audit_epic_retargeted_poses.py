"""Inspect saved target clip dependencies and authored notify instances."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT


def audit_retargeted():
    import unreal
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    receipt = json.loads((REPORT_ROOT / "generation.json").read_text(encoding="utf-8"))
    paths = [entry["package"] for entry in receipt["outputs"] if "/Clips/" in entry["package"]]
    result = []
    failures = []
    options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=False)
    for path in paths:
        clip = unreal.load_asset(path)
        if not clip:
            raise RuntimeError("Cannot load target animation: " + path)
        notifies = list(unreal.CourierEpicArtBuilder.get_notify_classes(clip))
        dependencies = [str(p) for p in registry.get_dependencies(path, options)]
        data = registry.get_assets_by_package_name(path, False)
        skeleton = str(data[0].get_tag_value("Skeleton") or "") if data else ""
        if "CityCourier/Art/Characters/SK_Courier_Skeleton" not in skeleton:
            failures.append({"package": path, "reason": "target skeleton", "value": skeleton})
        if any(name.startswith("/Game/") or name.endswith("AnimNotifyState_PoseSearchBranchIn") for name in notifies):
            failures.append({"package": path, "reason": "sample notify", "value": notifies})
        raw_dependencies = [name for name in dependencies if name.startswith("/Game/Characters/UEFN_Mannequin/Animations/") or name.startswith("/Game/Blueprints/")]
        if raw_dependencies:
            failures.append({"package": path, "reason": "runtime sample reference", "value": raw_dependencies})
        result.append({"package": path, "skeleton": skeleton, "notifies": notifies, "hard_dependencies": dependencies})
    report = {"animation_count": len(result), "asset_fingerprint": receipt.get("asset_fingerprint"), "complete": bool(result) and not failures, "failures": failures, "assets": result}
    (REPORT_ROOT / "target-clip-audit.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    if failures:
        raise RuntimeError("Target animation closure failed: " + str(failures[:3]))
    return report


if __name__ == "__main__":
    audit_retargeted()
