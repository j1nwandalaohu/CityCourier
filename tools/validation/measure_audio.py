import wave,struct,math,json,pathlib
out=pathlib.Path(__file__).resolve().parents[2]/'Saved/Validation/Task11-Audio'
with wave.open(str(out/'runtime-audio.wav'),'rb') as f:
    rate=f.getframerate();channels=f.getnchannels();frames=f.getnframes();width=f.getsampwidth()
    assert width==2
    samples=struct.unpack('<'+'h'*(frames*channels),f.readframes(frames))
values={}
for name,start in [('idle',1),('moving',4),('pause',7),('resume',10),('stopped',13),('restart',16)]:
    data=samples[int(start*rate*channels):int((start+1)*rate*channels)]
    assert data
    values[name]={'rms':math.sqrt(sum(x*x for x in data)/len(data))/32768,'peak':max(abs(x) for x in data)/32768}
(out/'levels.json').write_text(json.dumps(values,indent=2),encoding='utf-8');print(json.dumps(values,indent=2))
assert values['moving']['rms']>values['idle']['rms']*2,'Motor inaudible after idle'
assert values['pause']['rms']<values['idle']['rms']*.05,'World audio did not pause'
assert values['restart']['rms']>values['moving']['rms']*.7,'Motor did not restart after stopping'
assert max(v['peak'] for v in values.values())<.95,'Audio clipping'
print('Actual runtime mixer output: motor, pause, resume and restart PASS')
