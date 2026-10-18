"""Fetch complete, checksum-verified CC0 glTF sets from Poly Haven."""
import concurrent.futures
import hashlib
import json
import urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]/'AssetsSource/CityCourier/External/PolyHavenModels'
ASSETS=['tree_small_02','plastic_crate_02','exterior_aircon_unit']

def fetch(url,path,md5=None):
    path.parent.mkdir(parents=True,exist_ok=True)
    data=path.read_bytes() if path.exists() else urllib.request.urlopen(
        urllib.request.Request(url,headers={'User-Agent':'CityCourierArt/1.0'}),timeout=120).read()
    if md5 and hashlib.md5(data).hexdigest()!=md5:raise RuntimeError('Checksum mismatch: '+str(path))
    if not path.exists():path.write_bytes(data)
    return data

def asset(name):
    folder=ROOT/name
    meta=fetch('https://api.polyhaven.com/files/'+name,folder/'metadata.json')
    metadata=json.loads(meta);entry=metadata['gltf']['2k']['gltf']
    files={name+'_2k.gltf':entry,**entry['include']}
    for channel,formats in metadata.items():
        if 'alpha' in channel.lower() or 'opacity' in channel.lower():
            info=formats.get('2k',{}).get('jpg')
            if info:files['textures/'+info['url'].rsplit('/',1)[-1]]=info
    records=[]
    for relative,info in files.items():
        path=(folder/relative).resolve()
        if not path.is_relative_to(folder.resolve()):raise RuntimeError('Unsafe source path')
        data=fetch(info['url'],path,info['md5'])
        records.append(dict(path=path.relative_to(ROOT).as_posix(),source='https://polyhaven.com/a/'+name,
            download=info['url'],license='CC0-1.0',sha256=hashlib.sha256(data).hexdigest()))
    print(name,sum((ROOT/r['path']).stat().st_size for r in records),'bytes verified',flush=True)
    return records

if __name__=='__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        records=[record for group in pool.map(asset,ASSETS) for record in group]
    (ROOT/'sources.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8',newline='\n')
