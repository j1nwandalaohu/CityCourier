"""Offline receipts and fresh build/test evidence; source presence is not proof."""
import json
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, SOURCE_ROOTS, GRAPH_VERSION, package_file
from migrate_epic_character_samples import sha256

PREFIXES = ("CityCourier.EpicAnimation", "CityCourier.EpicInteraction", "CityCourier.Traversal",
            "CityCourier.PhysicalReaction", "CityCourier.EpicCharacterIntegration",
            "CityCourier.CharacterActions", "CityCourier.Interaction", "CityCourier.Scooter",
            "CityCourier.VehicleTraversal")


def validate_integration(output_path: str) -> dict:
    root = Path(__file__).resolve().parents[2]
    def read(path):
        return json.loads(path.read_text(encoding="utf-8-sig")) if path.exists() else {}
    sources = {name: bool(read(REPORT_ROOT / f"{name}.json").get("closure_complete")) for name in SOURCE_ROOTS}
    migration = read(REPORT_ROOT / "migration.json")
    generation = read(REPORT_ROOT / "generation.json")
    conflicts = []
    for entry in migration.get("packages", []):
        path = package_file(root / "Content", entry["destination"])
        if not path.exists() or sha256(path) != entry["destination_sha256"]:
            conflicts.append(entry["destination"])
    for entry in generation.get("outputs", []):
        path = package_file(root / "Content", entry["package"])
        if not path.exists() or sha256(path) != entry["sha256"]:
            conflicts.append(entry["package"])
    for name, relative, digest in (("player", "CityCourier/Blueprints/BP_CourierCharacter.uasset", "player_sha256"),
                                    ("map", "CityCourier/Maps/L_EpicCharacterValidation.umap", "map_sha256")):
        path = root / "Content" / relative
        if not path.exists() or sha256(path) != generation.get(digest):
            conflicts.append(name)
    code_files = list((root / "Source").rglob("*.cpp")) + list((root / "Source").rglob("*.h")) + list((root / "Source").rglob("*.cs")) + [root / "CityCourier.uproject"]
    code_time = max(path.stat().st_mtime for path in code_files)
    builds = sorted((root / "Saved/Validation").glob("Build-Editor-*/build.log"), key=lambda path: path.stat().st_mtime)
    build = builds[-1] if builds else None
    build_ok = bool(build and "Result: Succeeded" in build.read_text(encoding="utf-8", errors="replace") and build.stat().st_mtime >= code_time)
    latest = {}
    for report in sorted((root / "Saved/Validation").glob("Automation-*/Report/index.json"), key=lambda path: path.stat().st_mtime):
        if not build_ok or report.stat().st_mtime < build.stat().st_mtime:
            continue
        data = read(report)
        for test in data.get("tests", []):
            name = test.get("fullTestPath", "")
            if name.startswith(PREFIXES):
                latest[name] = {"state": test.get("state"), "report": str(report.relative_to(root))}
    expected = set()
    for path in (root / "Source/CityCourier/Tests").rglob("*.cpp"):
        for name in re.findall(r'"(CityCourier\.[A-Za-z0-9_.]+)"', path.read_text(encoding="utf-8-sig")):
            if name.startswith(PREFIXES):
                expected.add(name)
    missing = sorted(name for name in expected if latest.get(name, {}).get("state") not in {"Success", "SuccessWithWarnings"})
    clip_audit = read(REPORT_ROOT / "target-clip-audit.json")
    repeat = read(REPORT_ROOT / "idempotence.json")
    complete_assets = bool(generation.get("player_enabled") and generation.get("landing_version") == 1 and generation.get("graph_version") == GRAPH_VERSION and generation.get("physics_version") == 2 and not conflicts and all(sources.values()) and clip_audit.get("complete") and clip_audit.get("animation_count") == 919 and repeat.get("identical") and clip_audit.get("asset_fingerprint") == generation.get("asset_fingerprint") == repeat.get("asset_fingerprint"))
    capture = REPORT_ROOT / "Contact/state.json"
    rendered = read(capture)
    render_ok = bool(rendered and rendered.get("error_cm", 999) <= 5 and rendered.get("press_count") == 1 and (REPORT_ROOT / "Contact/contact.png").exists())
    result = {"sources": sources,
              "assets": {"complete": complete_assets, "source_packages": len(migration.get("packages", [])), "generated_packages": len(generation.get("outputs", [])), "conflicts": conflicts},
              "code": {"complete": build_ok and not missing},
              "build": {"status": "passed" if build_ok else "missing_failed_or_stale", "log": str(build.relative_to(root)) if build else None},
              "runtime": {"status": "passed" if expected and not missing and render_ok else "incomplete", "expected_tests": len(expected), "missing_or_failed": missing, "tests": latest, "offscreen_contact": rendered},
              "packaging": "not_requested", "game_window": "not_launched"}
    result["complete"] = complete_assets and build_ok and bool(expected) and not missing and render_ok
    Path(output_path).write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    result = validate_integration(str(REPORT_ROOT / "integration-status.json"))
    print(json.dumps({key: value for key, value in result.items() if key != "runtime"}, indent=2))
    print("Missing/failed:", result["runtime"]["missing_or_failed"])
