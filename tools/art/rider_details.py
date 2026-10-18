"""Authored rider fittings. Geometry uses meters and the MakeHuman rest body."""
import math
import random
import bpy
from mathutils import Vector, Quaternion
from art_common import SOURCE, material, sphere, box


def fitted_garment(verts, folder, name, mat):
    """Evaluate the author's hm08 barycentric attachment, including axis scales."""
    base=SOURCE/'External/MakeHumanPack/clothes'/folder
    factors=[1,1,1]; positions=[]; reading=False
    for line in (base/(folder+'.mhclo')).read_text().splitlines():
        p=line.split()
        if not p: continue
        if p[0] in ['x_scale','y_scale','z_scale']:
            axis=['x_scale','z_scale','y_scale'].index(p[0])
            factors[axis]=abs(verts[int(p[1])][axis]-verts[int(p[2])][axis])/float(p[3])
        elif p[0]=='verts': reading=True
        elif p[0]=='delete_verts': break
        elif reading and p[0].isdigit():
            if len(p)==1: positions.append(verts[int(p[0])].copy())
            else:
                v=sum((verts[int(p[i])]*float(p[i+3]) for i in range(3)),Vector())
                v+=Vector((float(p[6])*factors[0],-float(p[8])*factors[1],float(p[7])*factors[2]))
                positions.append(v)
    uv=[]; faces=[]
    for line in (base/(folder+'.obj')).read_text().splitlines():
        p=line.split()
        if not p: continue
        if p[0]=='vt':uv.append(tuple(map(float,p[1:3])))
        elif p[0]=='f':faces.append(([int(t.split('/')[0])-1 for t in p[1:]],[int(t.split('/')[1])-1 for t in p[1:]]))
    data=bpy.data.meshes.new(name);data.from_pydata(positions,[],[f[0] for f in faces]);data.update()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);data.materials.append(mat)
    layer=data.uv_layers.new(name='UVMap')
    for poly,(_,indices) in zip(data.polygons,faces):
        poly.use_smooth=True
        for li,ti in zip(poly.loop_indices,indices):layer.data[li].uv=uv[ti]
    return obj


def mesh_object(name, vertices, faces, mat):
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);data.materials.append(mat)
    for face in data.polygons:face.use_smooth=True
    return obj


def tube(name,points,radius,mat):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=2
    curve.bevel_depth=radius;curve.bevel_resolution=2
    spline=curve.splines.new('POLY');spline.points.add(len(points)-1)
    for p,v in zip(spline.points,points):p.co=(*v,1)
    obj=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.object.convert(target='MESH');obj=bpy.context.object;obj.data.materials.append(mat)
    return obj


def headgear(attach, helmet, rubber, reflective, joints):
    """Compact open-face commuter shell, front brow opening, edge seal, vents."""
    center=Vector((0,-.032,1.674))
    rx,ry,rz=.107,.145,.116
    def edge(a):
        front=max(0,-math.sin(a))
        return 1.99-.65*front**3
    def surface(a,t,offset=0):
        return center+Vector(((rx+offset)*math.sin(t)*math.cos(a),
                             (ry+offset)*math.sin(t)*math.sin(a),(rz+offset)*math.cos(t)))
    segments=72;rings=18
    vertices=[surface(k*math.tau/segments,edge(k*math.tau/segments)*r/rings)
              for r in range(rings+1) for k in range(segments)]
    faces=[]
    for r in range(rings):
        for k in range(segments):faces.append((r*segments+k,(r+1)*segments+k,(r+1)*segments+(k+1)%segments,r*segments+(k+1)%segments))
    shell=mesh_object('Commuter_helmet_shell',vertices,faces,helmet)
    bpy.context.view_layer.objects.active=shell
    solid=shell.modifiers.new('Shell wall 3mm','SOLIDIFY');solid.thickness=.003
    bpy.ops.object.modifier_apply(modifier=solid.name)
    attach(shell,'head')
    rim=[surface(k*math.tau/segments,edge(k*math.tau/segments)) for k in range(segments+1)]
    attach(tube('Continuous_helmet_edge_seal',rim,.0035,rubber),'head')
    # Two small sculpted intake scoops follow the crown, with black recessed mouths.
    for sign in [-1,1]:
        for yy,zz in [(-.114,1.752),(.074,1.716)]:
            obj=sphere('Helmet_vent_recess',(sign*.047,yy,zz),(.020,.009,.004),rubber)
            obj.rotation_euler.x=math.radians(-40 if yy<0 else 65)
            attach(obj,'head')
        # Flat webbing branches around each ear and meets below the jaw.
        for pts in [[(sign*.098,-.080,1.641),(sign*.080,-.054,1.566),(sign*.045,-.090,1.525)],
                    [(sign*.091,.035,1.633),(sign*.080,-.054,1.566)]]:
            vv=[]
            for i,p in enumerate(pts):
                tangent=Vector(pts[min(i+1,len(pts)-1)])-Vector(pts[max(0,i-1)])
                width=tangent.cross(Vector((sign,0,0))).normalized()*.004
                vv.extend([Vector(p)-width,Vector(p)+width])
            ff=[(i*2,i*2+1,i*2+3,i*2+2) for i in range(len(pts)-1)]
            strap=mesh_object('Helmet_woven_Y_strap',vv,ff,rubber)
            bpy.context.view_layer.objects.active=strap
            wall=strap.modifiers.new('Woven_webbing_thickness','SOLIDIFY');wall.thickness=.001
            bpy.ops.object.modifier_apply(modifier=wall.name)
            attach(strap,'head')
        attach(box('Strap_adjuster',(sign*.080,-.054,1.566),(.005,.012,.012),rubber,.001),'head')
    attach(tube('Chin_retention_strap',[(-.045,-.09,1.525),(0,-.102,1.511),(.045,-.09,1.525)],.003,rubber),'head')
    # Thin rear reflective safety bars lie on the curved shell instead of floating.
    for a in [.95,2.19]:
        pts=[surface(a,1.47+i*.014,.0016) for i in range(17)]
        attach(tube('Helmet_rear_reflector',pts,.003,reflective),'head')


def facial_details(attach,joints,eye,iris,vertices,faces):
    from mathutils.bvhtree import BVHTree
    surface=BVHTree.FromPolygons(vertices,[f[0] for f in faces])
    pupil=material('M_Pupil',(.004,.003,.002),.12)
    hair=material('M_Hair',(.022,.016,.013),.88)
    rng=random.Random(23017)
    for side in ['l','r']:
        sign=1 if side=='l' else -1
        p=joints['joint-'+side+'-eye']
        attach(sphere('Sclera',p,(.0133,.014,.0122),eye),'head')
        front=p+Vector((0,-.0131,0))
        attach(sphere('Brown_iris',front,(.0058,.0017,.0058),iris),'head')
        attach(sphere('Pupil',front+Vector((0,-.0015,0)),(.0026,.0007,.0026),pupil),'head')
        # Short individual eyebrow strands vary in length and follow the brow arch.
        for k in range(100):
            u=rng.random();x=sign*(.013+.040*u)
            # Place the brow 14-17 mm above the eye center, following the actual
            # face landmark rather than a fixed forehead height.
            z=p.z+.014+.003*math.sin(u*math.pi)-.0015*u+rng.uniform(-.0013,.0013)*(1-u*.65)
            hit,normal,_,_=surface.ray_cast(Vector((x,-1,z)),Vector((0,1,0)))
            if hit:
                root=hit+normal*.00045
                attach(tube('Eyebrow_hair',[root,root+Vector((sign*.0013,0,.0013+.0006*math.sin(k*2.4)))],.00016,hair),'head')
    # Sparse, deterministic short stubble. Each shaft is actual fine geometry on
    # the face surface, so it follows engine lights and the existing skin UVs.
    vv=[];ff=[]
    for _ in range(950):
        x=rng.uniform(-.068,.068);z=rng.uniform(1.502,1.594)
        if abs(x)<.034 and z>1.540:continue
        if z>1.568 and abs(x)<.048:continue
        hit,normal,_,_=surface.ray_cast(Vector((x,-1,z)),Vector((0,1,0)))
        if hit is None or hit.y>-.084:continue
        root=hit+normal*.00013
        tangent=normal.cross(Vector((0,0,1))).normalized()*.00013
        bitangent=normal.cross(tangent).normalized()*.00013
        n=len(vv)
        vv.extend([root+tangent,root-tangent*.5+bitangent*.866,root-tangent*.5-bitangent*.866,
                   root+normal*rng.uniform(.00035,.00075)+Vector((0,0,-.00035))])
        ff.extend([(n,n+1,n+3),(n+1,n+2,n+3),(n+2,n,n+3)])
    attach(mesh_object('Short_jaw_stubble',vv,ff,hair),'head')


def palm_normal(joints,side):
    wrist=joints['joint-'+side+'-hand']
    middle=joints[f'joint-{side}-finger-3-1']
    across=joints[f'joint-{side}-finger-5-1']-joints[f'joint-{side}-finger-2-1']
    return (middle-wrist).normalized().cross(across).normalized()*(1 if side=='l' else -1)


def orient_grip(rig,joints,side):
    """Pronate the hand so the palm wraps down over the horizontal handlebar."""
    pb=rig.pose.bones['hand_'+side]
    axis=(pb.tail-pb.head).normalized()
    palm=(pb.matrix@pb.bone.matrix_local.inverted()).to_3x3()@palm_normal(joints,side)
    palm=(palm-axis*palm.dot(axis)).normalized()
    desired=Vector((0,0,-1));desired=(desired-axis*desired.dot(axis)).normalized()
    rotation=palm.rotation_difference(desired).to_matrix().to_4x4()
    matrix=rotation@pb.matrix;matrix.translation=pb.matrix.translation
    pb.matrix=matrix;bpy.context.view_layer.update()


def finger_bones(bone,joints,side):
    for digit in range(1,6):
        for segment in range(1,4):
            name=f'finger_{digit}_{segment}_{side}'
            parent=f'finger_{digit}_{segment-1}_{side}' if segment>1 else 'hand_'+side
            bone(name,joints[f'joint-{side}-finger-{digit}-{segment}'],
                 joints[f'joint-{side}-finger-{digit}-{segment+1}'],parent)


def curl_fingers(rig,joints,side,ride=False,moving=False):
    """Curl toward the anatomical palm normal; no wrist offsets or IK targets change."""
    palm=palm_normal(joints,side)
    for digit in range(1,6):
        for segment in range(1,4):
            pb=rig.pose.bones[f'finger_{digit}_{segment}_{side}']
            direction=(pb.bone.tail_local-pb.bone.head_local).normalized()
            axis=direction.cross(palm).normalized()
            local_axis=pb.bone.matrix_local.to_3x3().inverted()@axis
            if digit==1:
                angle=([.36,.58,.56] if ride else [.12,.22,.18])[segment-1]
            else:
                angle=([.52,1.12,.82] if ride else [.12,.28,.18])[segment-1]
                if moving:angle+=.10
                angle+=(digit-2)*.015
            pb.rotation_quaternion=Quaternion(local_axis,angle)
