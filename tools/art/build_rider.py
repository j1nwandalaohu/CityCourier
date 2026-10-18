"""CC0 MakeHuman body/wardrobe with original headgear, rig and animation."""
import bpy
import math
import re
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from collections import defaultdict
from mathutils import Vector,Matrix
from art_common import *
from rider_details import fitted_garment,headgear,facial_details,finger_bones,curl_fingers,orient_grip
from rider_cloth import tag_garment_panels,refine_workwear

def read_body():
    verts=[];tex=[];faces=[];groups=defaultdict(set);group=''
    for line in (SOURCE/'External/MakeHuman/base.obj').read_text().splitlines():
        p=line.split()
        if not p:continue
        if p[0]=='v':verts.append(Vector(tuple(map(float,p[1:4]))))
        elif p[0]=='vt':tex.append(tuple(map(float,p[1:3])))
        elif p[0]=='g':group=p[1]
        elif p[0]=='f':
            vi=[int(t.split('/')[0])-1 for t in p[1:]]
            groups[group].update(vi)
            if group=='body':faces.append((vi,[int(t.split('/')[1])-1 for t in p[1:]]))
    lo=min(verts[i].y for i in groups['body']);hi=max(verts[i].y for i in groups['body']);scale=1.76/(hi-lo)
    def convert(v):return Vector((v.x*scale,-v.z*scale,(v.y-lo)*scale))
    joints={g:convert(sum((verts[i] for i in ids),Vector())/len(ids)) for g,ids in groups.items() if g.startswith('joint-')}
    return [convert(v) for v in verts],tex,faces,joints

def build():
    reset();verts,tex,faces,j=read_body()
    skin=material('M_Skin',(.48,.285,.18),.65)
    def textured(mat,folder,diffuse,normal=None):
        p=mat.node_tree.nodes.get('Principled BSDF')
        n=mat.node_tree.nodes.new('ShaderNodeTexImage');n.image=bpy.data.images.load(str(SOURCE/'External/MakeHumanPack'/folder/diffuse),check_existing=True)
        mat.node_tree.links.new(n.outputs['Color'],p.inputs['Base Color'])
        if normal:
            n=mat.node_tree.nodes.new('ShaderNodeTexImage');n.image=bpy.data.images.load(str(SOURCE/'External/MakeHumanPack'/folder/normal),check_existing=True);n.image.colorspace_settings.name='Non-Color'
            bump=mat.node_tree.nodes.new('ShaderNodeNormalMap');bump.inputs['Strength'].default_value=.5
            mat.node_tree.links.new(n.outputs['Color'],bump.inputs['Color']);mat.node_tree.links.new(bump.outputs['Normal'],p.inputs['Normal'])
    textured(skin,Path('skins/young_asian_male'),'young_lightskinned_male_diffuse3.png')
    rubber=material('M_Rubber',(.025,.027,.025),.92)
    reflective=material('M_ReflectiveStrip',(.70,.73,.65),.33,.4)
    helmet=material('M_Helmet',(.83,.48,.045),.36)
    eye=material('M_EyeWhite',(.43,.40,.35),.23)
    iris=material('M_Iris',(.058,.029,.013),.28)
    # Retain the skin under the garment to avoid gaps at animated sleeve openings.
    objects=[]
    def body_part(name,predicate,mat,offset):
        selected=[(vi,uv) for vi,uv in faces if predicate(sum((verts[i] for i in vi),Vector())/len(vi))]
        ids=sorted({i for vi,_ in selected for i in vi});mapping={old:new for new,old in enumerate(ids)}
        data=bpy.data.meshes.new(name);data.from_pydata([verts[i] for i in ids],[],[[mapping[i] for i in vi] for vi,_ in selected]);data.update()
        obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);data.materials.append(mat)
        uv=data.uv_layers.new(name='UVMap')
        for face,(_,uv_ids) in zip(data.polygons,selected):
            face.use_smooth=True
            for li,ti in zip(face.loop_indices,uv_ids):uv.data[li].uv=tex[ti]
        if offset:
            for v in data.vertices:
                v.co+=v.normal*offset
                # Small nonuniform cloth folds, not perfectly smooth plastic skin.
                v.co+=v.normal*(.0018*math.sin(v.co.z*145+v.co.x*24))
        objects.append(obj);return obj
    wardrobe=SOURCE/'External/MakeHumanPack/clothes/male_casualsuit04'
    hidden=set()
    shoe_folder=SOURCE/'External/MakeHumanPack/clothes/shoes01'
    hidden_text='\n'.join(p.read_text().split('delete_verts',1)[1] for p in
                         [wardrobe/'male_casualsuit04.mhclo',shoe_folder/'shoes01.mhclo'])
    for match in re.finditer(r'(\d+)(?:\s*-\s*(\d+))?',hidden_text):
        a=int(match[1]);b=int(match[2]) if match[2] else a;hidden.update(range(a,b+1))
    faces=[(vi,uv) for vi,uv in faces if not all(i in hidden for i in vi)]
    body_part('Body',lambda c:True,skin,0)
    cloth=material('M_WorkClothes',(.15,.25,.35),.9)
    textured(cloth,Path('clothes/male_casualsuit04'),'male_casualsuit04_diffuse.png','male_casualsuit04_normal.png')
    workwear=fitted_garment(verts,'male_casualsuit04','Tailored_workwear',cloth)
    tag_garment_panels(workwear);objects.append(workwear)
    shoes=material('M_WorkShoes',(.11,.095,.08),.79)
    textured(shoes,Path('clothes/shoes01'),'shoes01_diffuse.png','shoes01_normal.png')
    objects.append(fitted_garment(verts,'shoes01','Fitted_work_shoes',shoes))
    # Clothing details have explicit attachment bones after joining.
    attachments=[]
    def attach(o,bone):objects.append(o);attachments.append((o,bone));return o
    headgear(attach,helmet,rubber,reflective,j)
    facial_details(attach,j,eye,iris,verts,faces)
    # Build the rig from MakeHuman joint landmarks, preserving measured limb lengths.
    rig_data=bpy.data.armatures.new('CourierSkeleton');rig=bpy.data.objects.new('CourierRig',rig_data);bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig;rig.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
    definitions=[]
    def bone(name,head,tail,parent=None):
        b=rig_data.edit_bones.new(name);b.head=head;b.tail=tail
        if parent:b.parent=rig_data.edit_bones[parent]
        definitions.append((name,Vector(head),Vector(tail),parent))
    bone('root',(0,0,0),(0,0,.15))
    bone('pelvis',j['joint-pelvis'],j['joint-spine-4'],'root')
    bone('spine',j['joint-spine-4'],j['joint-spine-2'],'pelvis')
    bone('chest',j['joint-spine-2'],j['joint-neck'],'spine')
    bone('neck',j['joint-neck'],j['joint-head'],'chest')
    bone('head',j['joint-head'],j['joint-head-2'],'neck')
    for s in ['l','r']:
        bone('clavicle_'+s,j['joint-neck'],j['joint-'+s+'-shoulder'],'chest')
        bone('upperarm_'+s,j['joint-'+s+'-shoulder'],j['joint-'+s+'-elbow'],'clavicle_'+s)
        bone('forearm_'+s,j['joint-'+s+'-elbow'],j['joint-'+s+'-hand'],'upperarm_'+s)
        # Keep the shipped skeleton's hand basis. Grip pronation belongs in the
        # animation, so reimporting into an existing UE skeleton stays compatible.
        bone('hand_'+s,j['joint-'+s+'-hand'],j['joint-'+s+'-hand']+Vector((0,-.065,-.03)),'forearm_'+s)
        finger_bones(bone,j,s)
        bone('thigh_'+s,j['joint-'+s+'-upper-leg'],j['joint-'+s+'-knee'],'pelvis')
        bone('shin_'+s,j['joint-'+s+'-knee'],j['joint-'+s+'-ankle'],'thigh_'+s)
        bone('foot_'+s,j['joint-'+s+'-ankle'],j['joint-'+s+'-foot-2'],'shin_'+s)
    bpy.ops.object.mode_set(mode='OBJECT')
    explicit={o.name:b for o,b in attachments}
    for o in objects:
        for name,_,_,_ in definitions:o.vertex_groups.new(name=name)
        if o.name in explicit:o.vertex_groups[explicit[o.name]].add(list(range(len(o.data.vertices))),1,'REPLACE')
        else:
            for v in o.data.vertices:
                p=v.co;side='l' if p.x>=0 else 'r'
                if o.name=='Fitted_work_shoes':
                    # The CC0 shoe OBJ also contains sock shafts. Rigidly
                    # weighting those to the foot made them protrude behind the
                    # calf in the riding pose. Keep the shoe rigid and let the
                    # upper sock follow the shin through a soft ankle transition.
                    ankle=j['joint-'+side+'-ankle'].z
                    blend=max(0.,min(1.,(p.z-ankle-.035)/.055))
                    blend=blend*blend*(3-2*blend)
                    if blend<1:o.vertex_groups['foot_'+side].add([v.index],1-blend,'REPLACE')
                    if blend>0:o.vertex_groups['shin_'+side].add([v.index],blend,'REPLACE')
                    continue
                elif o.name=='Tailored_workwear' and o.data.attributes['cloth_shirt'].data[v.index].value>.5:
                    candidates=['pelvis','spine','chest']
                    if p.z>1.24 and abs(p.x)>.17:candidates+=['upperarm_'+side,'clavicle_'+side]
                elif o.name=='Tailored_workwear' and p.z<.25:candidates=['shin_'+side]
                elif (p-j['joint-'+side+'-hand']).length<.21 and abs(p.x)>.39:
                    candidates=['hand_'+side,'forearm_'+side]
                    candidates+=[f'finger_{d}_{s}_{side}' for d in range(1,6) for s in range(1,4)]
                elif p.z>1.54:candidates=['head']
                elif p.z>1.49:candidates=['head','neck']
                elif p.z<.94:candidates=['thigh_'+side,'shin_'+side,'foot_'+side,'pelvis']
                else:candidates=['pelvis','spine','chest','neck','upperarm_'+side,'forearm_'+side,'hand_'+side,'clavicle_'+side]
                distances=[]
                for name,a,b,_ in definitions:
                    if name not in candidates:continue
                    ab=b-a;t=max(0,min(1,(p-a).dot(ab)/ab.length_squared))
                    distances.append(((p-(a+t*ab)).length,name))
                weights=sorted(distances)[:3];total=sum(1/max(d,.005)**4 for d,_ in weights)
                for d,name in weights:o.vertex_groups[name].add([v.index],1/max(d,.005)**4/total,'REPLACE')
        bpy.context.view_layer.objects.active=o;o.select_set(True)
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        if o.name=='Tailored_workwear':refine_workwear(o,j)
    mesh=join(objects,'SK_Courier');mesh.parent=rig
    mod=mesh.modifiers.new('Courier skin','ARMATURE');mod.object=rig
    export('SK_Courier',[mesh,rig],'Characters')
    scene=bpy.context.scene;scene.render.fps=30
    def direction(name,target):
        pb=rig.pose.bones[name];rest=pb.bone
        head=pb.parent.matrix @ pb.parent.bone.matrix_local.inverted() @ rest.head_local if pb.parent else rest.head_local
        rotation=(rest.tail_local-rest.head_local).rotation_difference(Vector(target))
        matrix=rotation.to_matrix().to_4x4() @ rest.matrix_local.to_quaternion().to_matrix().to_4x4();matrix.translation=head
        pb.matrix=matrix;bpy.context.view_layer.update()
    def solve_limb(upper,lower,target,pole):
        a=rig.pose.bones[upper];b=rig.pose.bones[lower]
        root=a.matrix.translation;delta=Vector(target)-root
        length_a=a.bone.length;length_b=b.bone.length
        distance=min(delta.length,length_a+length_b-.001);axis=delta.normalized()
        bend=Vector(pole);bend=(bend-axis*bend.dot(axis)).normalized()
        along=(length_a**2-length_b**2+distance**2)/(2*distance)
        elbow=root+axis*along+bend*math.sqrt(max(0,length_a**2-along**2))
        direction(upper,elbow-root);direction(lower,Vector(target)-elbow)
    for name,frames in [('A_Idle',60),('A_Walk',30),('A_Run',22),('A_Ride',60)]:
        action=bpy.data.actions.new(name);rig.animation_data_create();rig.animation_data.action=action
        scene.frame_start=1;scene.frame_end=frames+1
        for frame in range(1,frames+2):
            scene.frame_set(frame);phase=(frame-1)/frames*math.tau
            for pb in rig.pose.bones:pb.matrix_basis=Matrix.Identity(4);pb.rotation_mode='QUATERNION'
            ride=name=='A_Ride';moving=name in ['A_Walk','A_Run'];run=name=='A_Run'
            height=.69-j['joint-pelvis'].z if ride else (.012*(1-math.cos(phase*2)) if moving else .002*math.sin(phase))
            root=rig.pose.bones['root']
            root.matrix=Matrix.Translation(Vector((0,0,height))) @ root.bone.matrix_local
            bpy.context.view_layer.update()
            if ride:direction('spine',(0,-.22,1))
            direction('chest',(0,-.35 if ride else -.035 if run else 0,1))
            for s,sign in [('l',1),('r',-1)]:
                swing=math.sin(phase)*sign*(.65 if run else .43) if moving else 0
                direction('thigh_'+s,(sign*.025,-.95 if ride else swing,-.12 if ride else -1))
                direction('shin_'+s,(0,.12 if ride else max(0,-swing)*1.3,-1))
                direction('foot_'+s,(0,-1,-.03))
                direction('upperarm_'+s,(sign*.14,-.65 if ride else -swing*.7,-.72 if ride else -1))
                direction('forearm_'+s,(0,-1 if ride else -.65 if run else -.16,-.02 if ride else -.75))
                direction('hand_'+s,(sign*.08,-.2,-1))
                if ride:
                    solve_limb('thigh_'+s,'shin_'+s,(sign*.20,-.13,.17),(0,-1,0))
                    direction('foot_'+s,(0,-1,-.03))
                    solve_limb('upperarm_'+s,'forearm_'+s,(sign*.34,-.45,.88),(sign*.5,.2,-1))
                    direction('hand_'+s,(0,-1,-.2))
                    orient_grip(rig,j,s)
                curl_fingers(rig,j,s,ride,moving)
            for pb in rig.pose.bones:
                pb.keyframe_insert('location',frame=frame);pb.keyframe_insert('rotation_quaternion',frame=frame)
        scene.frame_set(1);export(name,[rig],'Characters',True)
    rig.animation_data.action=bpy.data.actions['A_Idle'];scene.frame_set(1)
    save('CourierRider')

if __name__=='__main__':build()
