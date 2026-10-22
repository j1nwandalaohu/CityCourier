"""Regenerate both player graphs after the current native Editor build."""
import hashlib
import json
import sys
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(Path(__file__).resolve().parent))
from epic_character_sample_paths import read_receipt, REPORT_ROOT, GRAPH_VERSION


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def succeeded(result):
    return bool(result[0]) if isinstance(result, tuple) else bool(result)


if not hasattr(unreal.CourierAction, 'KNOCKDOWN'):
    raise RuntimeError('Build the updated Editor before connecting library actions')
receipt = read_receipt('generation.json')
for entry in receipt['outputs']:
    if digest(root/'Content'/(entry['package'].removeprefix('/Game/')+'.uasset')) != entry['sha256']:
        raise RuntimeError('Generated package was edited: '+entry['package'])
definitions = json.loads((root/'AssetsSource/CityCourier/library_actions.json').read_text(encoding='utf-8'))
imports = json.loads((root/'Saved/Validation/LibraryActions/import.json').read_text(encoding='utf-8'))
for name, definition in definitions['clips'].items():
    if imports.get(name, {}).get('source_fbx_sha256') != definition['fbx_sha256']:
        raise RuntimeError('Run import_library_actions.py for current exports before connection: '+name)
    package = root/'Content/CityCourier/Animation/Library'/f'{name}.uasset'
    if digest(package) != imports[name].get('runtime_package_sha256'):
        raise RuntimeError('Imported library package changed after its receipt: '+name)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
config = unreal.load_asset('/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation')
epic = unreal.load_asset('/Game/CityCourier/Animation/Epic/ABP_CourierEpic')
if not config or not epic:
    raise RuntimeError('Existing Epic integration must be present')
if not config.get_editor_property('validation_fingerprint'):
    raise RuntimeError('Existing Epic config is not validated')
result = unreal.CourierEpicArtBuilder.build_pose_search_databases(config)
if not succeeded(result):
    raise RuntimeError('Forward gait database generation failed: '+str(result))
result = unreal.CourierEpicArtBuilder.build_animation_blueprint(config, epic)
if not succeeded(result):
    raise RuntimeError('Graph regeneration failed: '+str(result))
if unreal.get_default_object(epic.generated_class()).get_editor_property('epic_graph_version') != GRAPH_VERSION:
    raise RuntimeError('Editor binary did not generate the required stationary idle branch')
chooser = unreal.load_asset('/Game/CityCourier/Animation/Epic/CHT_CourierDatabase')
for path in assets.list_assets('/Game/CityCourier/Animation/Epic', recursive=True, include_folder=False):
    if path.split('.')[0].endswith('/DA_CourierEpicAnimation'):
        continue  # Save configuration only after fresh player validation.
    if not assets.save_asset(path, only_if_is_dirty=True):
        raise RuntimeError('Could not save generated gait/graph asset: '+path)
legacy = unreal.load_asset('/Game/CityCourier/Blueprints/ABP_Courier')
defaults = unreal.get_default_object(legacy.generated_class())
paths = [
    '/Game/CityCourier/Animation/Library/A_IdleRelaxed',
    '/Game/CityCourier/Art/Characters/A_Jog',
    '/Game/CityCourier/Art/Characters/A_Sprint',
    '/Game/CityCourier/Animation/Library/A_Ride',
]
clips = [unreal.load_asset(p) for p in paths]
if not all(clips):
    raise RuntimeError('A legacy fallback library clip is missing')
defaults.set_editor_property('clips', clips)
unreal.BlueprintEditorLibrary.compile_blueprint(legacy)
if not assets.save_loaded_asset(legacy):
    raise RuntimeError('Could not save legacy animation defaults')
# Revalidate the changed graph and hash the actual referenced library assets.
# Never restore the old fingerprint onto changed outputs.
fingerprint_entries = []
legacy_package = '/Game/CityCourier/Blueprints/ABP_Courier'
fingerprint_entries.append({'package': legacy_package, 'sha256': digest(root/'Content/CityCourier/Blueprints/ABP_Courier.uasset')})
for folder in ('/Game/CityCourier/Animation/Epic', '/Game/CityCourier/Animation/Library'):
    for path in assets.list_assets(folder, recursive=True, include_folder=False):
        package = path.split('.')[0]
        if not package.endswith('/DA_CourierEpicAnimation'):
            fingerprint_entries.append({'package': package, 'sha256': digest(root/'Content'/(package.removeprefix('/Game/')+'.uasset'))})
fingerprint = hashlib.sha256(json.dumps(sorted(fingerprint_entries, key=lambda e: e['package']), sort_keys=True).encode()).hexdigest()
player = unreal.load_asset('/Game/CityCourier/Blueprints/BP_CourierCharacter')
result = unreal.CourierEpicArtBuilder.enable_on_player(config, player, fingerprint)
if not succeeded(result):
    raise RuntimeError('Changed graph did not pass native player validation: '+str(result))
for asset in (config, player):
    if not assets.save_loaded_asset(asset):
        raise RuntimeError('Could not save validated player configuration')
receipt['outputs'] = [dict(package=p.split('.')[0], sha256=digest(root/'Content'/(p.split('.')[0].removeprefix('/Game/')+'.uasset')))
    for p in assets.list_assets('/Game/CityCourier/Animation/Epic', recursive=True, include_folder=False)]
receipt.update(graph_version=GRAPH_VERSION, library_graph_version=1, asset_fingerprint=fingerprint,
    player_sha256=digest(root/'Content/CityCourier/Blueprints/BP_CourierCharacter.uasset'),
    pending=['library animation automation tests', 'rendered library pose and riding contact checks'])
REPORT_ROOT.mkdir(parents=True, exist_ok=True)
(REPORT_ROOT/'generation.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
out = root/'Saved/Validation/LibraryActions'
out.mkdir(parents=True, exist_ok=True)
(out/'connected.json').write_text(json.dumps({'legacy_clips': paths, 'epic_graph': epic.get_path_name(),
    'stationary_posture_index': 6, 'asset_fingerprint': fingerprint,
    'status': 'graphs_regenerated_awaiting_tests'}, indent=2)+'\n', encoding='utf-8')
unreal.log('LIBRARY_GRAPHS_CONNECTED')
