"""Compile full canonical data into a native cooked asset, without editing maps."""
import json
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT))
from tools.interiors.catalog import to_native_catalog


def build_catalog():
    source = json.loads((ROOT / 'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
    native = to_native_catalog(source)
    output = ROOT / 'Saved/Validation/compiled-building-catalog.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(native, ensure_ascii=False, separators=(',', ':'), allow_nan=False), encoding='utf-8')
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    path = '/Game/CityCourier/Data/Buildings/DA_Buildings'
    catalog = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if not catalog:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.CityBuildingCatalog)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_Buildings', '/Game/CityCourier/Data/Buildings', unreal.CityBuildingCatalog, factory)
    assert catalog.import_json_file(str(output)), 'Native building validation failed; existing catalog was not replaced'
    assert catalog.validate_catalog()
    assert assets.save_loaded_asset(catalog)
    unreal.log('BUILDING CATALOG SAVED: ' + str(len(native['buildings'])) + ' buildings; digest=' + native['sourceDigest'])
    return catalog


if __name__ == '__main__':
    build_catalog()
