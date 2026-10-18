"""Measure exported-source pose compatibility and foot motion before UE import."""
import json,math,statistics,sys
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'AssetsSource/CityCourier'

def validate():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/Mesh2MotionProbe.blend'))
    rig=bpy.data.objects['CourierRig'];mesh=bpy.data.objects['SK_Courier'];scene=bpy.context.scene
    baseline=bpy.data.objects['BaselineRig']
    assert [(b.name,b.parent.name if b.parent else None,list(map(list,b.matrix_local))) for b in rig.data.bones]==[(b.name,b.parent.name if b.parent else None,list(map(list,b.matrix_local))) for b in baseline.data.bones]
    sole={}
    for side,sign in [('l',1),('r',-1)]:
        ids={i for p in mesh.data.polygons if mesh.data.materials[p.material_index].name=='M_WorkShoes' for i in p.vertices if mesh.data.vertices[i].co.x*sign>0}
        low=min(mesh.data.vertices[i].co.z for i in ids)
        ids=[i for i in ids if mesh.data.vertices[i].co.z<low+.012]
        sole[side]=ids
    result={}
    for name in ['A_Idle','A_Walk','A_Run','A_M2M_Idle','A_M2M_Walk','A_M2M_Jog','A_M2M_Run']:
        action=bpy.data.actions[name];rig.animation_data.action=action
        first,last=map(int,action.frame_range);positions={s:[] for s in sole};roots=[];maxstep=0
        previous=None;pose_samples=[]
        for frame in range(first,last+1):
            scene.frame_set(frame);deps=bpy.context.evaluated_depsgraph_get();evaluated=mesh.evaluated_get(deps)
            data=evaluated.to_mesh()
            for side,ids in sole.items():
                verts=[data.vertices[i].co.copy() for i in ids]
                assert all(math.isfinite(v) for p in verts for v in p)
                positions[side].append({'z':min(p.z for p in verts),'y':statistics.mean(p.y for p in verts),'x':statistics.mean(p.x for p in verts)})
            evaluated.to_mesh_clear()
            roots.append(list(rig.pose.bones['root'].matrix.translation))
            matrices={b.name:b.matrix.copy() for b in rig.pose.bones}
            pose_samples.append(matrices)
            if previous:
                maxstep=max(maxstep,max((matrices[n].translation-previous[n].translation).length for n in matrices))
            previous=matrices
        velocities=[];lateral=[]
        minimum=min(p['z'] for a in positions.values() for p in a)
        for samples in positions.values():
            for a,b in zip(samples,samples[1:]):
                if max(a['z'],b['z'])<minimum+.025:
                    velocities.append((b['y']-a['y'])*30)
                    lateral.append(abs(b['x']-a['x'])*30)
        speed=statistics.median(velocities) if velocities else 0
        residual=statistics.median(abs(v-speed) for v in velocities) if velocities else 0
        game_speed=420 if name in ('A_Walk','A_M2M_Jog') else 700 if name in ('A_Run','A_M2M_Run') else max(0,speed*100)
        rate=game_speed/(speed*100) if name.startswith('A_M2M_') and speed>.1 else 1
        world_slip=statistics.median(abs(v*rate-game_speed/100) for v in velocities)*100 if velocities else 0
        closing_step=max((pose_samples[-1][n].translation-pose_samples[-2][n].translation).length for n in pose_samples[0])
        seam_velocity=max(((pose_samples[1][n].translation-pose_samples[0][n].translation)-(pose_samples[-1][n].translation-pose_samples[-2][n].translation)).length*30 for n in ('foot_l','foot_r'))
        roottravel=max((Vector(p)-Vector(roots[0])).length for p in roots)
        if name.startswith('A_M2M_'):
            assert roottravel<1e-6,roottravel
            assert minimum>=-.0001,('shoe penetration',name,minimum)
        result[name]={'duration_s':(last-first)/30,'lowest_shoe_z_cm':minimum*100,
            'root_motion_cm':roottravel*100,'max_joint_step_cm':maxstep*100,
            'estimated_natural_speed_cm_s':speed*100,'stance_velocity_residual_cm_s':residual*100,
            'stance_lateral_velocity_cm_s':statistics.median(lateral)*100 if lateral else 0,
            'comparison_world_speed_cm_s':game_speed,'matched_play_rate':rate,
            'estimated_world_stance_slip_cm_s':world_slip,'closing_joint_step_cm':closing_step*100,
            'foot_seam_velocity_difference_cm_s':seam_velocity*100}
    (SOURCE/'motion_probe_validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('VALIDATION',json.dumps(result),flush=True)

if __name__=='__main__':validate()
