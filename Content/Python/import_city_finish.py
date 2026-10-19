"""Import validated city art into a private, cooked data asset. No map writes."""
from pathlib import Path
import hashlib
import json
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
source=root/'AssetsSource/CityCourier/InteriorArt'
report=json.loads((source/'city-plan-validation.json').read_text(encoding='utf-8'))
assert report['passed'] and not report['errors']
assert hashlib.sha256((source/'city-layout.json').read_bytes()).hexdigest()==report['layout_sha256']
assert hashlib.sha256((root/'AssetsSource/CityCourier/Interiors/inventory.json').read_bytes()).hexdigest()==report['inventory_sha256']
catalog=unreal.load_asset('/Game/CityCourier/Data/Buildings/DA_Buildings')
assert catalog and catalog.validate_catalog()
path='/Game/CityCourier/Art/Interiors/DA_InteriorFinish'
asset=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not asset:
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.CourierInteriorFinish)
    asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_InteriorFinish','/Game/CityCourier/Art/Interiors',unreal.CourierInteriorFinish,factory)
assert asset and asset.import_json_file(str(source/'city-layout.json'),catalog)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log('CITY FINISH IMPORTED: '+str(len(report['buildings']))+' buildings; '+str(report['placements'])+' instanced placements')
