"""Export selected, already downloaded Epic reference clips without saving source packages."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'AssetsSource/CityCourier/External/CombatRecovery/Exports/Source'
OUT.mkdir(parents=True, exist_ok=True)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
selected = [
    '/Game/ExampleContent/ControlRig/Animations/MontageExample_Punch_1',
    '/Game/ExampleContent/ControlRig/Animations/MontageExample_Punch_2',
    '/Game/Characters/Mannequins/Animations/Manny/MM_HitReact_Front_Med_01',
    '/Game/Characters/Mannequins/Animations/Manny/MM_HitReact_Back_Med_01',
    '/Game/Characters/Mannequins/Animations/Manny/MM_Death_Front_01',
    '/Game/Characters/Mannequins/Animations/Manny/MM_Death_Back_01',
]
selected = sorted(set(selected) | {str(data.package_name) for data in registry.get_assets_by_path('/Game/Characters/Mannequins/Animations/Manny', recursive=False)
                                 if str(data.asset_name).startswith('MM_Death_')})
results = []
for package in selected:
    animation = unreal.load_asset(package)
    if not isinstance(animation, unreal.AnimSequence):
        raise RuntimeError('Expected animation sequence: ' + package)
    target = OUT / (animation.get_name() + '.fbx')
    task = unreal.AssetExportTask()
    task.object = animation
    task.filename = str(target)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.AnimSequenceExporterFBX()
    if not unreal.Exporter.run_asset_export_task(task) or not target.exists():
        raise RuntimeError('Cannot export source clip: ' + package)
    skeleton = animation.get_editor_property('skeleton')
    results.append({'source_package': package, 'source_skeleton': skeleton.get_path_name(),
                    'duration_seconds': animation.get_play_length(), 'path': target.relative_to(ROOT).as_posix(),
                    'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
report = {'source_project': unreal.Paths.project_dir(), 'source_assets_saved': False, 'clips': results}
(OUT / 'export.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('COMBAT_SOURCE_EXPORT ' + json.dumps(report))
