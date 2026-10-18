"""Verify downloaded CC0 sources against the retained SHA-256 provenance records."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'AssetsSource/CityCourier/External'
count = 0
for directory in [root, root / 'MakeHumanPack', root / 'PolyHavenModels',
                  root / 'Mesh2MotionProbe', root / 'CharMorphProbe']:
    manifest = json.loads((directory / 'sources.json').read_text(encoding='utf-8'))
    records = manifest['files'] if isinstance(manifest, dict) else manifest
    if isinstance(manifest, dict) and 'legalcode_sha256' in manifest:
        legalcode = directory / 'license-evidence/CC0-1.0.txt'
        if hashlib.sha256(legalcode.read_bytes()).hexdigest() != manifest['legalcode_sha256']:
            raise RuntimeError('CC0 legalcode checksum mismatch: ' + str(legalcode))
    for record in records:
        path = (directory / record['path']).resolve()
        if not path.is_relative_to(directory.resolve()):
            raise RuntimeError('Source record escapes its directory: ' + record['path'])
        if hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
            raise RuntimeError('Source checksum mismatch: ' + str(path))
        count += 1
print(f'Verified {count} source files against retained SHA-256 records.')
