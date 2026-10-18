"""Retain immutable, CC0 Mesh2Motion animation inputs for the courier probe."""
import hashlib,json,struct,urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]/'AssetsSource/CityCourier/External/Mesh2MotionProbe'
REVISION='653a9698f5f315523072f1c3ff3496100401317b'
FILES=['README.md','LICENSE-CC0.MD','static/animations/human-base-animations.glb',
       'static/animations/human-addon-animations.glb']
EXPECTED_SHA256={
    'README.md':'ce4a2c0cbb3227fadd43de2c4d63226507da0a6241646fb2ef966309300867b5',
    'LICENSE-CC0.MD':'52fec1c484df708c16b4c27b2bf0d7f0d3994105c57adc4b9e6f95bcfd9de65f',
    'static/animations/human-base-animations.glb':'406eb0a8dc4ab366e623b79b6e3005a4951392e1bda78ae39c1099d31147733c',
    'static/animations/human-addon-animations.glb':'a0d64d555e0d492026b72d58bf8e16c5e86779295f9093e376dcc001915c2c95',
}

def fetch():
    ROOT.mkdir(parents=True,exist_ok=True);records=[]
    for filename in FILES:
        url=f'https://raw.githubusercontent.com/Mesh2Motion/mesh2motion-app/{REVISION}/{filename}'
        path=ROOT/filename;path.parent.mkdir(parents=True,exist_ok=True)
        if not path.exists():
            path.write_bytes(urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'CityCourierArt/1.0'}),timeout=120).read())
        data=path.read_bytes()
        if hashlib.sha256(data).hexdigest()!=EXPECTED_SHA256[filename]:
            raise RuntimeError('Pinned source checksum mismatch: '+filename)
        records.append(dict(path=filename,source='https://github.com/Mesh2Motion/mesh2motion-app',
            revision=REVISION,download=url,license='CC0-1.0' if filename.endswith('.glb') else 'License evidence',sha256=hashlib.sha256(data).hexdigest()))
        if filename.endswith('.glb'):
            size,kind=struct.unpack_from('<II',data,12);gltf=json.loads(data[20:20+size])
            print(Path(filename).name,[(a.get('name'),len(a['channels'])) for a in gltf.get('animations',[])],flush=True)
    (ROOT/'sources.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8',newline='\n')

if __name__=='__main__':fetch()
