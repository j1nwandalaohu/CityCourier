"""Fetch immutable CC0 Vitruvian assets; GPL CharMorph authoring code is not bundled.

Run with Python 3: python tools/art/fetch_charmorph_probe.py
All downloaded bytes are preserved, checked against Git blobs/LFS object hashes,
and recorded with SHA256. No README photograph is used as texture evidence.
"""
import concurrent.futures
import hashlib
import json
import urllib.request
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / 'AssetsSource/CityCourier/External/CharMorphProbe'
REPO = 'Upliner/CharMorph-Vitruvian'
REVISION = 'c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44'
SELECTED = {'README.md', 'config.yaml', '.gitattributes', 'char.blend',
            'assets/Shirt.blend', 'assets/authors.yaml', 'eyebrows.blend',
            'morphs/L1/Default.npy', 'morphs/L2/morphs.json',
            'hairstyles/mind_eyebrows_11_Default.npz',
            'morphs/L2/Gender_Male.npz', 'morphs/L2/Race_EastAsian.npz'}


def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'CityCourierArt/1.0'})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                return response.read()
        except OSError:
            if attempt == 3:
                raise
            time.sleep(1 + attempt)


def fetch_file(entry):
    name = entry['path']
    # Preserve the upstream attributes as evidence without letting its LFS
    # filter rewrite our vendored .blend bytes into repository pointers.
    stored_name = 'license-evidence/upstream.gitattributes.txt' if name == '.gitattributes' else name
    target = ROOT / stored_name
    target.parent.mkdir(parents=True, exist_ok=True)
    raw_url = f'https://raw.githubusercontent.com/{REPO}/{REVISION}/{name}'
    # GitHub raw exposes a pointer for LFS assets. Retain the pointer separately.
    pointer = ROOT / 'license-evidence/lfs-pointers' / (name + '.txt')
    raw = pointer.read_bytes() if pointer.exists() else (get(raw_url) if entry['size'] < 200 and not target.exists() else None)
    if raw is None and target.exists() and entry['size'] < 200:
        raw = target.read_bytes()
    lfs_hash = None
    if raw and raw.startswith(b'version https://git-lfs.github.com/spec/v1'):
        lfs_hash = raw.decode().split('oid sha256:')[1].splitlines()[0]
        pointer.parent.mkdir(parents=True, exist_ok=True)
        pointer.write_bytes(raw)
        url = f'https://media.githubusercontent.com/media/{REPO}/{REVISION}/{name}'
    else:
        url = raw_url
    if not target.exists():
        target.write_bytes(get(url) if raw is None or lfs_hash else raw)
    data = target.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    git_hash = hashlib.sha1(f'blob {len(data)}\0'.encode() + data).hexdigest()
    if lfs_hash:
        if digest != lfs_hash:
            raise RuntimeError(f'LFS hash mismatch: {name}')
    elif git_hash != entry['sha']:
        raise RuntimeError(f'Git blob hash mismatch: {name}')
    print(name, len(data), digest, flush=True)
    return dict(path=stored_name, upstream_path=name, bytes=len(data), sha256=digest, git_blob_sha1=entry['sha'],
                lfs_sha256=lfs_hash, source=f'https://github.com/{REPO}',
                revision=REVISION, download=url, license='CC0-1.0',
                license_evidence='config.yaml', original_bytes=True)


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    tree_url = f'https://api.github.com/repos/{REPO}/git/trees/{REVISION}?recursive=1'
    tree_data = (ROOT / 'upstream-tree.json').read_bytes() if (ROOT / 'upstream-tree.json').exists() else get(tree_url)
    (ROOT / 'upstream-tree.json').write_bytes(tree_data)
    tree = json.loads(tree_data)
    if tree['truncated']:
        raise RuntimeError('Incomplete upstream tree')
    entries = [e for e in tree['tree'] if e['type'] == 'blob'
               and (e['path'] in SELECTED or e['path'].startswith('textures/'))]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        records = sorted(pool.map(fetch_file, entries), key=lambda item: item['path'])
    if b'license: CC0' not in (ROOT / 'config.yaml').read_bytes():
        raise RuntimeError('Expected explicit upstream CC0 declaration absent')
    evidence = ROOT / 'license-evidence'
    evidence.mkdir(exist_ok=True)
    license_path = evidence / 'CC0-1.0.txt'
    if not license_path.exists():
        license_path.write_bytes(get('https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt'))
    manifest = dict(repository=f'https://github.com/{REPO}', revision=REVISION,
                    license='CC0-1.0', license_evidence='config.yaml',
                    upstream_has_separate_license_file=False,
                    legalcode_sha256=hashlib.sha256(license_path.read_bytes()).hexdigest(),
                    scope='Selected character/garment/morph assets and ALL upstream source textures; no GPL addon code',
                    files=records)
    (ROOT / 'sources.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
