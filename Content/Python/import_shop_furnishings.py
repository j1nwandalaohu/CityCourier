"""Import only the six retained-source shop assemblies using verified shaders."""
import json
import sys
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(root/'Content/Python'))
from import_interior_finish import InteriorImporter, protected_snapshot, _hash

source = root/'AssetsSource/CityCourier/InteriorArt'
importer = InteriorImporter()
definitions, palette = importer._preflight()
previous = json.loads((source/'import-validation.json').read_text(encoding='utf-8'))
before = protected_snapshot()
importer._reuse_palette(palette,previous)
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
shops = [d for d in definitions if d['name'].startswith('SM_IF_Shop')]
assert len(shops)==6
for definition in shops:
    importer._mesh(definition)
after = protected_snapshot()
assert before==after, 'Retained maps, building catalog or exterior assets changed'
importer.report.update(status='passed', protected_unchanged=True,
    manifest_sha256=_hash(source/'asset-manifest.json'))
report = source/'shop-import-validation.json'
report.write_text(json.dumps(importer.report,indent=2)+'\n',encoding='utf-8')
unreal.log('SHOP IMPORT PASS: six blocking assemblies, verified palette, protected assets unchanged')
