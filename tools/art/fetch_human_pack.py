"""Read only selected CC0 pack members using public HTTP byte ranges."""
import hashlib,io,json,urllib.request,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
URL='https://files.makehumancommunity.org/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip'
def request(a,b):
    with urllib.request.urlopen(urllib.request.Request(URL,headers={'Range':f'bytes={a}-{b}','User-Agent':'CityCourierArt/1.0'}),timeout=45) as r:
        if r.status!=206:raise RuntimeError('Server did not honor range request')
        return r.read(),r.headers['Content-Range']
first,header=request(0,0);length=int(header.split('/')[-1])
class Remote(io.RawIOBase):
    def __init__(self):self.pos=0;self.cache={}
    def seekable(self):return True
    def readable(self):return True
    def seek(self,p,w=0):self.pos=p if w==0 else self.pos+p if w==1 else length+p;return self.pos
    def tell(self):return self.pos
    def read(self,n=-1):
        if n<0:n=length-self.pos
        start=self.pos;self.pos+=n
        if not n:return b''
        for a,data in self.cache.items():
            if a<=start and start+n<=a+len(data):return data[start-a:start-a+n]
        data,_=request(start,min(length-1,start+max(n,4096)-1));self.cache[start]=data
        return data[:n]
with zipfile.ZipFile(Remote()) as z:
    names=z.namelist()
    wanted=[n for n in names if any(s in n for s in ['male_casualsuit04/','young_asian_male/','shoes01/'])]
    pack=ROOT/'AssetsSource/CityCourier/External/MakeHumanPack'
    manifest=pack/'sources.json'
    # Preserve provenance for previously fetched assets even when the selection grows.
    records={r['path']:r for r in json.loads(manifest.read_text())} if manifest.exists() else {}
    for name in wanted:
        if name.endswith('/'):continue
        if Path(name).is_absolute() or '..' in Path(name).parts:raise RuntimeError('Unsafe archive path')
        path=ROOT/'AssetsSource/CityCourier/External/MakeHumanPack'/name
        path.parent.mkdir(parents=True,exist_ok=True)
        if not path.exists():path.write_bytes(z.read(name))
        records[name]=dict(path=name,source=URL,license='CC0-1.0',
                           license_source='https://creativecommons.org/publicdomain/zero/1.0/',
                           license_file='LICENSE.txt',sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        print(name,path.stat().st_size,flush=True)
    manifest.write_text(json.dumps(list(records.values()),indent=2)+'\n',encoding='utf-8',newline='\n')
