"""Blender 4.0 original traffic meshes and deterministic synthesized mono WAV sources."""
import pathlib,math,json,csv,wave,struct,random
source=pathlib.Path(__file__).with_name('build_courier_assets.py').read_text(encoding='utf-8')
# Reuse the retained original geometry helpers and exact courier bone proportions.
exec(compile(source.split('# Rider:')[0],str(__file__),'exec'))
OUT=ROOT/'AssetsSource/CityCourier/Ambient';EXPORT=OUT/'Exports';EXPORT.mkdir(parents=True,exist_ok=True)
material('CitizenCloth',(.12,.20,.31));material('CarPaint',(.24,.32,.38),.45,.28)
rider=source.split('# Rider:')[1].split("export('SK_Courier'")[0]
rider=rider[rider.index('before='):]
rider='\n'.join(line for line in rider.splitlines() if not any(part in line for part in ['Helmet','Reflective','Chest pocket']))
rider=rider.replace("for side in [-1,1]:\ncylinder('Neck'", "cylinder('Neck'")
rider=rider.replace("'Amber'","'CitizenCloth'").replace("'SK_Courier'","'SK_Citizen'")
rider=rider.replace("mesh=join", "ellipsoid('Hair crown',(0,0,169),(10.5,10,6),'Hair','head')\nmesh=join")
exec(compile(rider,str(__file__),'exec'))
export('SK_Citizen',[rig,mesh]);mesh.hide_set(True);rig.hide_set(True)
before=set(o.name for o in bpy.context.scene.objects)
cube('Sedan lower body',(0,0,65),(425,176,70),'CarPaint',12)
cube('Sedan hood',(130,0,100),(125,172,30),'CarPaint',6)
cube('Cabin',(-20,0,127),(225,158,80),'CarPaint',20)
cube('Front windshield',(79,0,141),(5,137,53),'Glass',5)
cube('Rear windshield',(-121,0,138),(5,137,45),'Glass',4)
for side in [-1,1]:
    for x in [-75,29]:
        cube('Side window',(x,side*79,140),(91,3,47),'Glass',6)
        cube('Door handle',(x-24,side*89,100),(18,3,4),'Steel',1)
    cube('Side mirror',(77,side*99,121),(24,16,13),'CarPaint',4)
    for x in [-135,138]:
        cylinder('Tire',(x,side*78,38),(x,side*98,38),36,'Rubber',vertices=24)
        cylinder('Alloy hub',(x,side*98,38),(x,side*100,38),23,'Steel',vertices=16)
    cube('Front lamp',(213,side*57,78),(3,43,21),'Lamp',4)
    cube('Rear lamp',(-213,side*61,78),(3,36,19),'RedLamp',3)
cube('Front bumper',(214,0,45),(8,168,18),'Rubber',3)
cube('Grille',(219,0,72),(3,53,21),'Rubber',2)
cube('Plate',(223,0,52),(2,36,12),'Teal',1)
car=join(objects_since(before),'SM_CityCar');export('SM_CityCar',[car])
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CityCourier_Ambient.blend'))

audio=OUT/'Audio';audio.mkdir(exist_ok=True);rate=44100;rng=random.Random(12011)
def write(name,duration,fn):
    samples=[]
    for i in range(round(rate*duration)):
        t=i/rate;fade=min(1,t/.04,(duration-t)/.08)
        samples.append(max(-32767,min(32767,round(fn(t)*fade*32767))))
    with wave.open(str(audio/(name+'.wav')),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(struct.pack('<'+'h'*len(samples),*samples))
def tone(t,f):return math.sin(2*math.pi*t*f)
for name,notes in [('S_Accepted',[523,659]),('S_PickedUp',[659,784]),('S_Delivered',[523,659,784,1046]),('S_Failed',[392,330,262])]:
    def cue(t,notes=notes):
        step=int(t/.16)
        if step>=len(notes):return 0
        local=t-step*.16
        return .25*(tone(t,notes[step])+.18*tone(t,notes[step]*2))*math.exp(-local*15)
    write(name,len(notes)*.16+.2,cue)
write('S_Motor',2,lambda t:.15*tone(t,120)+.04*tone(t,360)+.015*tone(t,720))
write('S_Brake',.35,lambda t:(rng.uniform(-1,1)*.14+.04*tone(t,1600))*math.exp(-t*8))
noise=0
def city(t):
    global noise
    noise=.985*noise+.015*rng.uniform(-1,1)
    return noise*.6+.025*tone(t,70)+.012*tone(t,110)*(1+math.sin(t*.7))
write('S_City',12,city)
entries=[dict(asset_id='ambient_car',name='SM_CityCar',kind='static',source_path=str((EXPORT/'SM_CityCar.fbx').relative_to(ROOT)).replace('\\','/')),
         dict(asset_id='ambient_citizen',name='SK_Citizen',kind='skeletal',source_path=str((EXPORT/'SK_Citizen.fbx').relative_to(ROOT)).replace('\\','/'))]
for path in sorted(audio.glob('*.wav')):entries.append(dict(asset_id=path.stem.lower(),name=path.stem,kind='audio',source_path=str(path.relative_to(ROOT)).replace('\\','/')))
for entry in entries:entry['ue_path']='/Game/CityCourier/'+('Audio/' if entry['kind']=='audio' else 'Art/Ambient/')+entry['name']
(OUT/'catalog.json').write_text(json.dumps(entries,indent=2),encoding='utf-8')
manifest=ROOT/'docs/art/asset-manifest.csv'
with manifest.open(encoding='utf-8-sig',newline='') as f:reader=csv.DictReader(f);fields=reader.fieldnames;rows=list(reader)
ids={entry['asset_id'] for entry in entries};rows=[row for row in rows if row['asset_id'] not in ids]
for entry in entries:rows.append(dict(asset_id=entry['asset_id'],source_path=entry['source_path'],ue_path=entry['ue_path'],author='CityCourier project',source_url='',license='Original project asset',is_placeholder='false'))
with manifest.open('w',encoding='utf-8',newline='') as f:writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(rows)
print('Original ambient assets:',len(entries))
