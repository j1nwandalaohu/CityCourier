"""Fetch a small, reproducible CC0 set for the native interior kit.

Only original provider endpoints are used. Preserve authored UVs/PBR files,
provider checksums, license snapshots and a SHA256 inventory for redistribution.
"""
import concurrent.futures
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT/'AssetsSource/CityCourier/External/RealisticInteriors'
MODELS = ('food_apple_01', 'food_ginger_01', 'wooden_cutting_board')
TEXTURES = ('tiled_floor_001', 'wood_table_001')


def fetch(url, path, md5=None):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file():
        data = path.read_bytes()
    else:
        request = urllib.request.Request(url, headers={'User-Agent':'CityCourierArt/1.0'})
        with urllib.request.urlopen(request, timeout=90) as response:
            data = response.read()
        if md5 and hashlib.md5(data).hexdigest() != md5:
            raise RuntimeError('Provider checksum mismatch: '+str(path))
        path.write_bytes(data)
    if md5 and hashlib.md5(data).hexdigest() != md5:
        raise RuntimeError('Cached provider checksum mismatch: '+str(path))
    return dict(path=path.relative_to(DEST).as_posix(), url=url,
                sha256=hashlib.sha256(data).hexdigest(), bytes=len(data), provider_md5=md5)


def polyhaven(name, model):
    folder = DEST/name
    meta = fetch('https://api.polyhaven.com/files/'+name, folder/'metadata.json')
    data = json.loads((folder/'metadata.json').read_text())
    if model:
        entry = data['gltf']['1k']['gltf']
        files = {name+'_1k.gltf':entry, **entry['include']}
    else:
        files = {}
        for channel in ('Diffuse','Rough','nor_gl'):
            entry = data[channel]['2k']['jpg']
            files[entry['url'].rsplit('/',1)[1]] = entry
    records = [meta]
    for relative, entry in files.items():
        path = (folder/relative).resolve()
        if not path.is_relative_to(folder.resolve()):
            raise RuntimeError('Unsafe provider include path')
        records.append(fetch(entry['url'], path, entry['md5']))
    print('VERIFIED CC0',name,sum(r['bytes'] for r in records),'bytes',flush=True)
    return dict(id=name, provider='Poly Haven', source='https://polyhaven.com/a/'+name,
                license='CC0-1.0', files=records)


def ambientcg():
    name = 'Metal009'
    folder = DEST/name
    meta = fetch('https://ambientcg.com/api/v2/full_json?id=Metal009&include=downloadData',
                 folder/'asset-metadata.json')
    metadata = json.loads((folder/'asset-metadata.json').read_text())
    asset = next(a for a in metadata['foundAssets'] if a['assetId']==name)
    downloads = asset['downloadFolders']['default']['downloadFiletypeCategories']['zip']['downloads']
    entry = next(d for d in downloads if d['attribute']=='2K-JPG')
    archive = fetch(entry['fullDownloadPath'],folder/entry['fileName'])
    records = [meta,archive]
    with zipfile.ZipFile(io.BytesIO((folder/entry['fileName']).read_bytes())) as z:
        for member in z.infolist():
            if not member.filename.lower().endswith('.jpg'):
                continue
            path = (folder/member.filename).resolve()
            if not path.is_relative_to(folder.resolve()):
                raise RuntimeError('Unsafe archive member')
            content = z.read(member)
            path.write_bytes(content)
            records.append(dict(path=path.relative_to(DEST).as_posix(), url=archive['url'],
                sha256=hashlib.sha256(content).hexdigest(), bytes=len(content),
                archive_member=member.filename))
    print('VERIFIED CC0',name,'2K PBR set',flush=True)
    return dict(id=name,provider='ambientCG',source='https://ambientcg.com/a/'+name,
                license='CC0-1.0',files=records)


def main():
    DEST.mkdir(parents=True,exist_ok=True)
    jobs = [(n,True) for n in MODELS]+[(n,False) for n in TEXTURES]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        futures = [pool.submit(polyhaven,*job) for job in jobs]+[pool.submit(ambientcg)]
        assets = [f.result() for f in futures]
    licenses = [fetch('https://polyhaven.com/license',DEST/'polyhaven-license.html'),
                fetch('https://docs.ambientcg.com/license/',DEST/'ambientcg-license.html')]
    manifest = dict(schema_version=1,assets=assets,license_files=licenses,
        unavailable=[dict(id='45f6d12dc97d41a0a8f405171579d5b7',name='Whole Roasted Chicken',
            source='https://sketchfab.com/3d-models/whole-roasted-chicken-45f6d12dc97d41a0a8f405171579d5b7',
            license='CC-BY-4.0',reason='Official download API requires user authentication; not bundled')])
    (DEST/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    main()
