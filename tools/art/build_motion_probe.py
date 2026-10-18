"""Experimental Mesh2Motion -> existing courier retarget; never edits shipped clips."""
import json,math,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import bpy
from mathutils import Vector,Matrix
from art_common import SOURCE,export

ROOT=SOURCE.parents[1]
OUT=ROOT/'Saved/CharacterProbe';OUT.mkdir(parents=True,exist_ok=True)
CLIPS={'Idle_A':'A_M2M_Idle','Walk':'A_M2M_Walk','Jog':'A_M2M_Jog','Sprint':'A_M2M_Run'}
EXPORT_FOLDER='MotionProbe'
REPORT_NAME='motion_probe.json'
BLEND_NAME='Mesh2MotionProbe'
POSTPROCESS=None
CYCLIC=True
MAPPING={'pelvis':'pelvis','spine':'spine_01','chest':'spine_03','neck':'neck_01','head':'head'}
for side in ('l','r'):
    MAPPING.update({f'{t}_{side}':f'{s}_{side}' for t,s in (
        ('clavicle','clavicle'),('upperarm','upperarm'),('forearm','lowerarm'),
        ('hand','hand'),('thigh','thigh'),('shin','calf'),('foot','foot'))})
    for digit,label in enumerate(('thumb','index','middle','ring','pinky'),1):
        for segment in range(1,4):MAPPING[f'finger_{digit}_{segment}_{side}']=f'{label}_{segment:02}_{side}'

def filtered_source():
    data=(SOURCE/'External/Mesh2MotionProbe/static/animations/human-base-animations.glb').read_bytes()
    size,kind=struct.unpack_from('<II',data,12);doc=json.loads(data[20:20+size])
    doc['animations']=[a for a in doc['animations'] if a['name'] in CLIPS]
    payload=json.dumps(doc,separators=(',',':')).encode();payload+=b' '*((-len(payload))%4)
    tail=data[20+size:]
    path=OUT/'selected-source.glb'
    path.write_bytes(struct.pack('<III',0x46546c67,2,20+len(payload)+len(tail))+struct.pack('<II',len(payload),kind)+payload+tail)
    return path

def build():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierRider.blend'))
    original_rest={b.name:[list(row) for row in b.matrix_local] for b in bpy.data.objects['CourierRig'].data.bones}
    # The production .blend retained only its active action. Recreate the
    # unchanged baseline in memory so all four source actions survive the probe.
    import build_rider
    build_rider.export=lambda *args:None;build_rider.save=lambda *args:None
    build_rider.build()
    target=bpy.data.objects['CourierRig'];mesh=bpy.data.objects['SK_Courier']
    assert original_rest=={b.name:[list(row) for row in b.matrix_local] for b in target.data.bones},'Source skeleton drift'
    for action in bpy.data.actions:action.use_fake_user=True
    rest={b.name:b.matrix_local.copy() for b in target.data.bones}
    skeleton_before={b.name:(b.parent.name if b.parent else None,[list(row) for row in b.matrix_local]) for b in target.data.bones}
    old_actions={a.name:([float(x) for x in a.frame_range],len(a.fcurves)) for a in bpy.data.actions}
    baseline_rig=target.copy();baseline_rig.data=target.data.copy();baseline_rig.name='BaselineRig'
    bpy.context.collection.objects.link(baseline_rig)
    baseline_mesh=mesh.copy();baseline_mesh.data=mesh.data.copy();baseline_mesh.name='BaselineRider'
    bpy.context.collection.objects.link(baseline_mesh);baseline_mesh.parent=baseline_rig
    for mod in baseline_mesh.modifiers:
        if mod.type=='ARMATURE':mod.object=baseline_rig
    baseline_rig.hide_render=True;baseline_mesh.hide_render=True;baseline_mesh.hide_set(True)
    previous=set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(filtered_source()))
    imported=set(bpy.data.objects)-previous
    source=next(o for o in imported if o.type=='ARMATURE')
    for o in imported:
        if o!=source:bpy.data.objects.remove(o,do_unlink=True)
    source.hide_render=True
    scene=bpy.context.scene
    # glTF importer uses the current scene fps. The courier source is 30 fps.
    fps=scene.render.fps/scene.render.fps_base
    target.animation_data.action=None
    source_rest={b.name:b.matrix_local.copy() for b in source.data.bones}
    align={}
    for dst,src in MAPPING.items():
        if dst in ('pelvis','spine','chest','neck','head') or dst.startswith('clavicle_'):
            # Core bones use different anatomical endpoints, especially the
            # clavicles. Transfer their motion delta, not their edit-bone axis.
            reference=rest[dst].to_quaternion()
        else:
            a=(rest[dst].to_quaternion()@Vector((0,1,0))).normalized()
            b=(source_rest[src].to_quaternion()@Vector((0,1,0))).normalized()
            if dst.startswith('hand_'):
                side=dst[-1]
                a=(rest['finger_3_1_'+side].translation-rest[dst].translation).normalized()
                b=(source_rest['middle_01_'+side].translation-source_rest[src].translation).normalized()
            reference=a.rotation_difference(b)@rest[dst].to_quaternion()
        align[dst]=source_rest[src].to_quaternion().inverted()@reference
    leg_scale=sum(target.data.bones[n+'_l'].length for n in ('thigh','shin'))/sum(source.data.bones[n+'_l'].length for n in ('thigh','calf'))
    ordered=list(target.data.bones)
    shoe_ids={i for p in mesh.data.polygons if mesh.data.materials[p.material_index].name=='M_WorkShoes' for i in p.vertices}
    sole_ids=[i for i in shoe_ids if mesh.data.vertices[i].co.z<min(mesh.data.vertices[j].co.z for j in shoe_ids)+.012]
    report={'source_revision':'653a9698f5f315523072f1c3ff3496100401317b','fps':fps,'leg_scale':leg_scale,'clips':{},'experimental':True}
    for source_name,name in CLIPS.items():
        action=bpy.data.actions[source_name];source.animation_data.action=action
        if action.slots:source.animation_data.action_slot=action.slots[0]
        first,last=map(float,action.frame_range);frames=round(last-first)
        samples=[]
        for frame in range(frames+1):
            source_frame=first+(last-first)*frame/frames
            scene.frame_set(math.floor(source_frame),subframe=source_frame-math.floor(source_frame))
            source_pose={b.name:b.matrix.copy() for b in source.pose.bones}
            matrices={'root':rest['root'].copy()}
            for bone in ordered:
                if bone.name=='root':continue
                parent=matrices[bone.parent.name]
                head=parent@rest[bone.parent.name].inverted()@bone.head_local
                q=source_pose[MAPPING[bone.name]].to_quaternion()@align[bone.name]
                if bone.name=='pelvis':
                    head=bone.head_local+(source_pose['pelvis'].translation-source_rest['pelvis'].translation)*leg_scale
                mat=q.to_matrix().to_4x4();mat.translation=head;matrices[bone.name]=mat
            # Match measured ankle trajectories with a two-bone solve; bone
            # lengths and reference matrices remain untouched.
            for side in ('l','r'):
                thigh,shin,foot=(n+'_'+side for n in ('thigh','shin','foot'))
                hip=matrices[thigh].translation
                ankle=rest[foot].translation+(source_pose[foot].translation-source_rest[foot].translation)*leg_scale
                a=target.data.bones[thigh].length;b=target.data.bones[shin].length
                delta=ankle-hip;distance=max(.001,min(delta.length,a+b-.0001));axis=delta.normalized()
                pole=source_pose['calf_'+side].translation-source_pose['thigh_'+side].translation
                pole=(pole-axis*pole.dot(axis)).normalized()
                along=(a*a-b*b+distance*distance)/(2*distance)
                knee=hip+axis*along+pole*math.sqrt(max(0,a*a-along*along))
                for joint,end in ((thigh,knee),(shin,ankle)):
                    head=hip if joint==thigh else knee
                    old=matrices[joint].to_quaternion();direction=old@Vector((0,1,0))
                    q=direction.rotation_difference((end-head).normalized())@old
                    mat=q.to_matrix().to_4x4();mat.translation=head;matrices[joint]=mat
                matrices[foot].translation=knee+matrices[shin].to_quaternion()@Vector((0,b,0))
            samples.append(matrices)
        lowest=math.inf
        for matrices in samples:
            deform={n:matrices[n]@rest[n].inverted() for n in rest}
            for i in sole_ids:
                vertex=mesh.data.vertices[i];point=Vector()
                for group in vertex.groups:point+=(deform[mesh.vertex_groups[group.group].name]@vertex.co)*group.weight
                lowest=min(lowest,point.z)
        lift=.002-lowest
        for matrices in samples:
            for joint,mat in matrices.items():
                if joint!='root':mat.translation.z+=lift
        if POSTPROCESS:
            samples=POSTPROCESS(name,samples,rest)
            frames=len(samples)-1
            fps=scene.render.fps/scene.render.fps_base
        # These four sources are cyclic locomotion/idle. Close the terminal
        # key explicitly after fractional-frame resampling to avoid a seam.
        if CYCLIC:samples[-1]={n:m.copy() for n,m in samples[0].items()}
        generated=bpy.data.actions.new(name);generated.use_fake_user=True;target.animation_data.action=generated
        scene.frame_start=1;scene.frame_end=frames+1
        previous_quaternions={}
        for frame,matrices in enumerate(samples,1):
            scene.frame_set(frame)
            for bone in ordered:
                basis=matrices[bone.parent.name]@rest[bone.parent.name].inverted()@rest[bone.name] if bone.parent else rest[bone.name]
                pb=target.pose.bones[bone.name];pb.rotation_mode='QUATERNION'
                pb.matrix_basis=basis.inverted()@matrices[bone.name]
                if bone.name in previous_quaternions:pb.rotation_quaternion.make_compatible(previous_quaternions[bone.name])
                previous_quaternions[bone.name]=pb.rotation_quaternion.copy()
                pb.keyframe_insert('location',frame=frame);pb.keyframe_insert('rotation_quaternion',frame=frame)
        for curve in generated.fcurves:
            for key in curve.keyframe_points:key.interpolation='LINEAR'
        scene.frame_set(1)
        export(name,[target],EXPORT_FOLDER,True)
        delta=max((samples[0][n].translation-samples[-1][n].translation).length for n in rest)
        angle=max(min(a,math.tau-a) for n in rest for a in [samples[0][n].to_quaternion().rotation_difference(samples[-1][n].to_quaternion()).angle])
        report['clips'][name]={'source_action':source_name,'duration_s':frames/fps,'samples':frames+1,'sole_height_correction_cm':lift*100,'loop_translation_error_cm':delta*100,'loop_rotation_error_deg':math.degrees(angle)}
    assert skeleton_before=={b.name:(b.parent.name if b.parent else None,[list(row) for row in b.matrix_local]) for b in target.data.bones}
    assert all(old_actions[name]==([float(x) for x in bpy.data.actions[name].frame_range],len(bpy.data.actions[name].fcurves)) for name in old_actions)
    report['skeleton_reference_unchanged']=True;report['baseline_actions_unchanged']=True
    report['mapping']=MAPPING
    (SOURCE/REPORT_NAME).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    target.animation_data.action=bpy.data.actions[next(iter(CLIPS.values()))];scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender'/f'{BLEND_NAME}.blend'))
    print('MOTION PROBE',json.dumps(report),flush=True)

if __name__=='__main__':build()
