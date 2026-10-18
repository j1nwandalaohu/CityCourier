"""Retarget retained CC0 punches to the production rig and gameplay hit windows."""
import json,math,shutil,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import bpy
from mathutils import Matrix
import build_motion_probe as retarget
from art_common import SOURCE

PROFILES={
    'A_LightPunch_L':{'source':'Punch_Jab','hand':'l','duration':.55,'contact':.18},
    'A_LightPunch_R':{'source':'Punch_Cross','hand':'r','duration':.55,'contact':.18},
    'A_HeavyPunch_R':{'source':'Melee_Hook','hand':'r','duration':.95,'contact':.40},
}
checks={}
def align_contact(name,samples,rest):
    profile=PROFILES[name]
    # Detect the striking side from its forward excursion, not an upstream label.
    excursion={s:max(-m['hand_'+s].translation.y for m in samples)-min(-m['hand_'+s].translation.y for m in samples) for s in ('l','r')}
    source_hand=max(excursion,key=excursion.get)
    if source_hand!=profile['hand']:
        mirror=Matrix.Diagonal((-1,1,1,1));mirrored=[]
        for pose in samples:
            target={}
            for bone in rest:
                opposite=bone[:-1]+('r' if bone[-1]=='l' else 'l') if bone.endswith(('_l','_r')) else bone
                target[bone]=mirror@(pose[opposite]@rest[opposite].inverted())@mirror@rest[bone]
            mirrored.append(target)
        samples=mirrored
    hand='hand_'+profile['hand']
    peak=max(range(len(samples)),key=lambda i:-samples[i][hand].translation.y)
    assert 0<peak<len(samples)-1,'Strike must extend and recover'
    source_contact=peak/(len(samples)-1)
    bpy.context.scene.render.fps=60
    frames=round(profile['duration']*60);result=[]
    for index in range(frames+1):
        t=index/60;contact=profile['contact']
        u=source_contact*t/contact if t<=contact else source_contact+(1-source_contact)*(t-contact)/(profile['duration']-contact)
        position=max(0,min(len(samples)-1,u*(len(samples)-1)));lo=math.floor(position);hi=min(lo+1,len(samples)-1);alpha=position-lo
        pose={}
        for bone in rest:
            a,b=samples[lo][bone],samples[hi][bone]
            m=a.to_quaternion().slerp(b.to_quaternion(),alpha).to_matrix().to_4x4()
            m.translation=a.translation.lerp(b.translation,alpha);pose[bone]=m
        pose['root']=rest['root'].copy()
        result.append(pose)
    checks[name]={**profile,'source_hand':source_hand,'mirrored':source_hand!=profile['hand'],'source_contact_normalized':source_contact,'samples':frames+1}
    return result

retarget.CLIPS={p['source']:name for name,p in PROFILES.items()}
retarget.EXPORT_FOLDER='ProductionActions'
retarget.REPORT_NAME='production_actions.json'
retarget.BLEND_NAME='CourierCombat'
retarget.CYCLIC=False
retarget.POSTPROCESS=align_contact
retarget.build()
folder=SOURCE/'Exports/ProductionActions'
for original,promoted in [('A_M2M_Jog','A_Jog'),('A_M2M_Run','A_Sprint')]:
    shutil.copyfile(SOURCE/'Exports/MotionProbe'/f'{original}.fbx',folder/f'{promoted}.fbx')
report=json.loads((SOURCE/'production_actions.json').read_text())
report.update(experimental=False,fps=60,contact_alignment=checks,locomotion={'A_Jog':{'source':'A_M2M_Jog','natural_speed_cm_s':473.2},'A_Sprint':{'source':'A_M2M_Run','natural_speed_cm_s':715.9}})
(SOURCE/'production_actions.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('PRODUCTION ACTIONS EXPORTED',json.dumps(checks),flush=True)
