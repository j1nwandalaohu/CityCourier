"""Fail closed before importing missing, modified or unlicensed external files."""
import hashlib
from pathlib import Path


def validate_sources(manifest, root):
    root = Path(root).resolve()
    errors = []
    if manifest.get('schema_version') != 1 or not manifest.get('assets'):
        return ['Missing or unsupported source inventory']
    records = list(manifest.get('license_files',[]))
    for asset in manifest['assets']:
        if asset.get('license') not in ('CC0-1.0','CC-BY-4.0'):
            errors.append('Unapproved source license: '+asset.get('id','unknown'))
        if asset.get('license')=='CC-BY-4.0' and not asset.get('attribution'):
            errors.append('Missing attribution: '+asset.get('id','unknown'))
        if not asset.get('source','').startswith('https://') or not asset.get('files'):
            errors.append('Missing source evidence: '+asset.get('id','unknown'))
        records.extend(asset.get('files',[]))
    for record in records:
        path = (root/record.get('path','')).resolve()
        if not path.is_relative_to(root) or not path.is_file():
            errors.append('Missing or unsafe source: '+record.get('path',''))
        elif hashlib.sha256(path.read_bytes()).hexdigest() != record.get('sha256'):
            errors.append('Modified source: '+record.get('path',''))
    return errors
