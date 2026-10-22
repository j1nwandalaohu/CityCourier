"""Run in the source GASP editor after audit; never rename binary packages."""
import hashlib
import json
from pathlib import Path
from epic_character_sample_paths import REPORT_ROOT, package_file


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def preflight(entries, destination_content, receipts=None):
    changes, conflicts = [], []
    for entry in entries:
        destination = package_file(destination_content, entry["package"])
        if destination.exists():
            receipt = (receipts or {}).get(entry["package"], {})
            expected = receipt.get("destination_sha256") if receipt.get("sha256") == entry["sha256"] else entry["sha256"]
            if sha256(destination) != expected:
                conflicts.append(str(destination))
        else:
            changes.append(entry["package"])
    if conflicts:
        raise RuntimeError("Conflict: " + ", ".join(conflicts))
    return changes


def migrate_selected(inventory_path: str, destination_content: str) -> dict:
    import unreal
    inventory = json.loads(Path(inventory_path).read_text(encoding="utf-8"))
    if inventory["source"] != "gasp" or not inventory["closure_complete"]:
        raise RuntimeError("Only an audited, complete GASP dependency closure may migrate")
    entries = [e for e in inventory["assets"] if e["migrate"]]
    for entry in entries:
        if sha256(entry["file"]) != entry["sha256"]:
            raise RuntimeError("Source changed after audit: " + entry["package"])
    destination = Path(destination_content).resolve()
    if destination.name != "Content" or not destination.is_dir():
        raise ValueError("Destination must be an existing project Content directory")
    receipt_path = REPORT_ROOT / "migration.json"
    prior = json.loads(receipt_path.read_text(encoding="utf-8")) if receipt_path.exists() else {}
    receipts = {e["source"]: e for e in prior.get("packages", [])}
    changes = preflight(entries, destination, receipts)
    if changes:
        options = unreal.MigrationOptions()
        options.set_editor_property("prompt", False)
        options.set_editor_property("ignore_dependencies", True)
        options.set_editor_property("asset_conflict", unreal.AssetMigrationConflict.SKIP)
        unreal.AssetToolsHelpers.get_asset_tools().migrate_packages(changes, str(destination), options)
    # Migration APIs may return before reporting a per-file failure. Verify every output.
    missing = [e["package"] for e in entries if not package_file(destination, e["package"]).exists()]
    if missing:
        raise RuntimeError("Migration verification failed: " + ", ".join(missing))
    report = {"source": inventory["source"], "changed": changes,
              "packages": [{"source": e["package"], "destination": e["package"],
                            "sha256": e["sha256"],
                            "destination_sha256": sha256(package_file(destination, e["package"]))} for e in entries]}
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    (REPORT_ROOT / "migration.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report
