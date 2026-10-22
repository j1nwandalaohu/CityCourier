"""Read source packages in their own UE editor. No source assets are saved.

Commandlet env: CITYCOURIER_EPIC_SOURCE=gasp|stack_o_bot|content_examples.
Optional CITYCOURIER_EPIC_MIGRATE=absolute target Content path migrates GASP.
The combined report requires all three source commandlets to have finished.
"""
import json
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import SOURCE_ROOTS, REFERENCES, REPORT_ROOT, package_file
from migrate_epic_character_samples import sha256


def audit_current(source: str) -> dict:
    import unreal
    root = Path(SOURCE_ROOTS[source])
    project = next(root.glob("*.uproject"))
    version = json.loads(project.read_text(encoding="utf-8-sig"))["EngineAssociation"]
    if version != "5.6":
        raise RuntimeError(f"Unsupported engine: {version}")
    if Path(unreal.Paths.project_dir()).resolve() != root.resolve():
        raise RuntimeError("Audit must run in the corresponding source project")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    selected = set()
    if source == "gasp":
        for data in registry.get_assets_by_path("/Game/Characters/UEFN_Mannequin", recursive=True):
            cls = str(data.asset_class_path.asset_name)
            # These two shipped roll clips reference a missing FoleyEvent_Roll notify.
            # Exclude broken content rather than fabricate a replacement dependency.
            if "_Land_Roll_" in str(data.package_name):
                continue
            if cls in {"AnimSequence", "AnimMontage", "IKRigDefinition", "Skeleton", "SkeletalMesh"}:
                selected.add(str(data.package_name))
    roots = selected | set(REFERENCES[source])
    hard_options = unreal.AssetRegistryDependencyOptions(
        include_hard_package_references=True, include_soft_package_references=False,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    soft_options = unreal.AssetRegistryDependencyOptions(
        include_hard_package_references=False, include_soft_package_references=True,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    closure, missing, pending = set(), [], list(selected)
    while pending:
        package = pending.pop()
        if package in closure or not package.startswith("/Game/"):
            continue
        closure.add(package)
        path = package_file(root / "Content", package)
        if not path.exists():
            missing.append(package)
            continue
        pending.extend(str(p) for p in registry.get_dependencies(package, hard_options))
    records = []
    for package in sorted(roots | closure):
        assets = registry.get_assets_by_package_name(package)
        path = package_file(root / "Content", package)
        if not assets or not path.exists():
            missing.append(package)
            continue
        data = assets[0]
        skeleton = str(data.get_tag_value("Skeleton") or "")
        records.append({"package": package, "file": str(path), "sha256": sha256(path),
                        "class": str(data.asset_class_path.asset_name), "skeleton": skeleton,
                        "hard_dependencies": sorted(str(p) for p in registry.get_dependencies(package, hard_options)),
                        "soft_dependencies": sorted(str(p) for p in registry.get_dependencies(package, soft_options)),
                        "migrate": source == "gasp" and package in closure})
    report = {"source": source, "project": str(project), "engine": version,
              "assets": records, "missing": sorted(set(missing)),
              "excluded": ["*_Land_Roll_*: missing BP_AnimNotify_FoleyEvent_Roll"] if source == "gasp" else [],
              "closure_complete": not missing, "selection": sorted(selected)}
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    (REPORT_ROOT / f"{source}.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    if missing:
        raise RuntimeError("Missing source packages: " + ", ".join(sorted(set(missing))))
    return report


def audit_sources(source_roots: dict[str, str], output_path: str) -> dict:
    reports = {}
    for source, location in source_roots.items():
        project = next(Path(location).glob("*.uproject"))
        if json.loads(project.read_text(encoding="utf-8-sig"))["EngineAssociation"] != "5.6":
            raise RuntimeError("UE 5.6 source required: " + str(project))
        report = json.loads((REPORT_ROOT / f"{source}.json").read_text(encoding="utf-8"))
        if Path(report["project"]).resolve() != project.resolve() or not report["closure_complete"]:
            raise RuntimeError("Stale or incomplete audit: " + source)
        reports[source] = report
    result = {"sources": reports, "source_assets_audited": True, "runtime_validated": False}
    Path(output_path).write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    source = os.environ["CITYCOURIER_EPIC_SOURCE"]
    audit_current(source)
    destination = os.environ.get("CITYCOURIER_EPIC_MIGRATE")
    if destination:
        from migrate_epic_character_samples import migrate_selected
        migrate_selected(str(REPORT_ROOT / f"{source}.json"), destination)
