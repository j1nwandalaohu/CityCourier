"""Retain successful audits and list only actually migrated Epic packages."""
import csv
import hashlib
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import REPORT_ROOT, SOURCE_ROOTS
from audit_epic_character_samples import audit_sources


def record_provenance():
    root = Path(__file__).resolve().parents[2]
    output = root / "docs/art/epic-character-samples"
    output.mkdir(parents=True, exist_ok=True)
    audits = audit_sources(SOURCE_ROOTS, str(REPORT_ROOT / "sources.json"))
    migration = json.loads((REPORT_ROOT / "migration.json").read_text(encoding="utf-8"))
    for source, report in audits["sources"].items():
        (output / f"{source}.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (output / "migration.json").write_text(json.dumps(migration, indent=2) + "\n", encoding="utf-8")
    manifest = root / "docs/art/asset-manifest.csv"
    with manifest.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames
        rows = [row for row in reader if not row["asset_id"].startswith(("epic_gasp_", "epic_courier_"))]
    for package in migration["packages"]:
        source = package["source"]
        rows.append({
            "asset_id": "epic_gasp_" + hashlib.sha256(source.encode()).hexdigest()[:16],
            "source_path": "GameAnimationSample/Content/" + source.removeprefix("/Game/") + ".uasset",
            "ue_path": package["destination"],
            "author": "Epic Games",
            "source_url": "https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016",
            "license": "Epic Games sample content - acquired Epic/Fab terms apply (not CC0)",
            "is_placeholder": "false",
        })
    generation_path = REPORT_ROOT / "generation.json"
    if generation_path.exists():
        from migrate_epic_character_samples import sha256
        generation = json.loads(generation_path.read_text(encoding="utf-8"))
        source_assets = {asset["package"].rsplit("/", 1)[-1]: asset["package"] for asset in audits["sources"]["gasp"]["assets"]}
        for entry in generation["outputs"]:
            package = entry["package"]
            filename = root / "Content" / (package.removeprefix("/Game/") + ".uasset")
            if sha256(filename) != entry["sha256"]:
                raise RuntimeError("Generated asset receipt mismatch: " + package)
            name = package.rsplit("/", 1)[-1]
            source_package = source_assets.get(name.removeprefix("CC_")) if name.startswith("CC_") else None
            source_path = ("GameAnimationSample/Content/" + source_package.removeprefix("/Game/") + ".uasset; " if source_package else "")
            generator = "build_epic_courier_physics.py" if name.startswith("PHYS_") else "build_epic_courier_graph.py" if name.startswith(("ABP_", "CR_", "CHT_")) else "build_epic_courier_animation.py"
            rows.append({"asset_id": "epic_courier_" + hashlib.sha256(package.encode()).hexdigest()[:16],
                         "source_path": source_path + "Content/Python/" + generator + "; docs/art/epic-character-samples/generation.json",
                         "ue_path": package, "author": "CityCourier; Epic Games animation reference",
                         "source_url": "https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016",
                         "license": "Derived courier assets; Epic/Fab sample terms apply to source animations (not CC0)",
                         "is_placeholder": "false"})
        (output / "generation.json").write_text(json.dumps(generation, indent=2) + "\n", encoding="utf-8")
    with manifest.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    return {"audited_sources": len(audits["sources"]), "migrated_packages": len(migration["packages"])}


if __name__ == "__main__":
    print(record_provenance())
