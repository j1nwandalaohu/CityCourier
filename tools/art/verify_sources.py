"""Verify downloaded CC0 sources against the retained SHA-256 provenance records."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'AssetsSource/CityCourier/External'
count = 0
for directory in [root, root / 'MakeHumanPack']:
    for record in json.loads((directory / 'sources.json').read_text(encoding='utf-8')):
        path = (directory / record['path']).resolve()
        if not path.is_relative_to(directory.resolve()):
            raise RuntimeError('Source record escapes its directory: ' + record['path'])
        if hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
            raise RuntimeError('Source checksum mismatch: ' + str(path))
        count += 1
print(f'Verified {count} source files against retained SHA-256 records.')
