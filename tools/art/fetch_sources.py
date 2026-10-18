"""Download a small, explicit set of CC0 art sources; retain licenses and hashes."""
import hashlib
import json
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "AssetsSource/CityCourier/External"
OUT.mkdir(parents=True, exist_ok=True)


def get(url, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.stat().st_size:
        return path.read_bytes()
    for attempt in range(3):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "CityCourierArt/1.0"}), timeout=60) as response:
                data = response.read()
            path.write_bytes(data)
            print(path.relative_to(ROOT), len(data), flush=True)
            return data
        except Exception:
            if attempt == 2:
                raise
            time.sleep(2)


records = []
mh = "https://raw.githubusercontent.com/makehumancommunity/makehuman/master/"
for relative, dest in [("makehuman/data/3dobjs/base.obj", "MakeHuman/base.obj"),
                       ("LICENSE.md", "MakeHuman/LICENSE.md"),
                       ("LICENSE.ASSETS.md", "MakeHuman/LICENSE.ASSETS.md")]:
    data = get(mh + relative, OUT / dest)
    records.append(dict(path=dest, source=mh+relative, license="CC0-1.0 (asset)", sha256=hashlib.sha256(data).hexdigest()))

get("https://polyhaven.com/license", OUT / "PolyHaven/license.html")
for name in ["concrete_wall_008", "asphalt_02", "red_brick_03", "painted_plaster_wall", "pavement_05"]:
    meta = json.loads(get("https://api.polyhaven.com/files/"+name, OUT/"PolyHaven"/(name+".json")))
    for channel in ["Diffuse", "nor_gl", "Rough"]:
        entry = meta[channel]["2k"]["jpg"]
        dest = "PolyHaven/"+entry["url"].split("/")[-1]
        data = get(entry["url"], OUT/dest)
        if hashlib.md5(data).hexdigest() != entry["md5"]:
            raise RuntimeError("Source checksum mismatch: "+dest)
        records.append(dict(path=dest, source="https://polyhaven.com/a/"+name, download=entry["url"], license="CC0-1.0", sha256=hashlib.sha256(data).hexdigest()))
meta=json.loads(get('https://api.polyhaven.com/files/sunset_jhbcentral',OUT/'PolyHaven/sunset_jhbcentral.json'))
entry=meta['hdri']['1k']['hdr'];dest='PolyHaven/sunset_jhbcentral_1k.hdr'
data=get(entry['url'],OUT/dest)
if hashlib.md5(data).hexdigest()!=entry['md5']:raise RuntimeError('HDRI source checksum mismatch')
records.append(dict(path=dest,source='https://polyhaven.com/a/sunset_jhbcentral',download=entry['url'],license='CC0-1.0',sha256=hashlib.sha256(data).hexdigest()))
entry=meta['hdri']['2k']['hdr'];dest='PolyHaven/sunset_jhbcentral_2k.hdr'
data=get(entry['url'],OUT/dest)
if hashlib.md5(data).hexdigest()!=entry['md5']:raise RuntimeError('Sky source checksum mismatch')
records.append(dict(path=dest,source='https://polyhaven.com/a/sunset_jhbcentral',download=entry['url'],license='CC0-1.0',sha256=hashlib.sha256(data).hexdigest()))
name='industrial_sunset_02_puresky'
meta=json.loads(get('https://api.polyhaven.com/files/'+name,OUT/'PolyHaven'/f'{name}.json'))
entry=meta['hdri']['2k']['hdr'];dest=f'PolyHaven/{name}_2k.hdr'
data=get(entry['url'],OUT/dest)
if hashlib.md5(data).hexdigest()!=entry['md5']:raise RuntimeError('Pure sky source checksum mismatch')
records.append(dict(path=dest,source='https://polyhaven.com/a/'+name,download=entry['url'],license='CC0-1.0',sha256=hashlib.sha256(data).hexdigest()))
(OUT/"sources.json").write_text(json.dumps(records, ensure_ascii=False, indent=2)+"\n", encoding="utf-8",newline='\n')
print("CC0 source download complete", flush=True)
