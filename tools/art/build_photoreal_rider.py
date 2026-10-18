"""Fit a CC0 Vitruvian head and strand groom to the unchanged courier rig.

Blender 4.5: blender -b --python-exit-code 1 --python tools/art/build_photoreal_rider.py
The shipped clips, skeleton reference and body/clothing weights stay intact.
"""
import collections,hashlib,json,math,random,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

ROOT=Path(__file__).resolve().parents[2];SOURCE=ROOT/'AssetsSource/CityCourier'
OUT=SOURCE/'Exports/Portrait';TEX=OUT/'Textures';TEX.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'tools/art'))
from build_rider import read_body
CATALOG={}
TEXTURE_RECIPE={'version':2,'light_mix':.82,'dark_mix':.18,'age_height':.035,
                'head_normal_strength':.006,'eye_normal_strength':.003,
                'roughness_min':.25,'roughness_max':.72}

def smooth(value):
    v=max(0.,min(1.,value));return v*v*(3-2*v)

def read_map(name,size):
    image=bpy.data.images.load(str(SOURCE/'External/CharMorphProbe/textures/4K'/name),check_existing=False)
    image.colorspace_settings.name='Non-Color'
    if tuple(image.size)!=(size,size):image.scale(size,size)
    data=np.empty(size*size*4,dtype=np.float32);image.pixels.foreach_get(data)
    bpy.data.images.remove(image);return data.reshape(size,size,4)

def save_map(name,data,srgb=False):
    path=TEX/(name+'.png');height,width=data.shape[:2]
    image=bpy.data.images.new(name,width=width,height=height,alpha=True,float_buffer=True)
    image.colorspace_settings.name='Linear Rec.709' if srgb else 'Non-Color'
    image.pixels.foreach_set(np.ascontiguousarray(data,dtype=np.float32).ravel())
    scene=bpy.context.scene
    scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
    scene.render.image_settings.color_depth='8';scene.render.image_settings.compression=35
    scene.view_settings.view_transform='Standard' if srgb else 'Raw'
    scene.view_settings.look='None';scene.view_settings.exposure=0;scene.view_settings.gamma=1
    image.save_render(str(path),scene=scene);bpy.data.images.remove(image)
    return str(path.relative_to(SOURCE)).replace('\\','/')

def normal_from_height(height,strength):
    dy,dx=np.gradient(height)
    normal=np.stack((-dx*strength,-dy*strength,np.ones_like(height)),axis=-1)
    normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-8)
    data=np.ones((*height.shape,4),np.float32);data[:,:,:3]=normal*.5+.5
    return data

def texture_set():
    cache=SOURCE/'portrait_texture_cache.json'
    inputs=['Light_Skin_Color.1001.exr','Dark_Skin_Color.1001.exr',
            'Skin_Roughness.1001.exr','Skin_Height.1001.exr','Skin_Age_Height.1001.exr']
    inputs += [f'{prefix}_{channel}.{tile}.exr'
               for prefix,tile in [('Mouth',1008),('Iris',1007),('Sclera',1005)]
               for channel in (('Color','Height') if prefix=='Sclera' else ('Color','Roughness','Height'))]
    provenance={'recipe':TEXTURE_RECIPE,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'inputs':{name:hashlib.sha256((SOURCE/'External/CharMorphProbe/textures/4K'/name).read_bytes()).hexdigest()
                          for name in inputs}}
    if cache.exists():
        d=json.loads(cache.read_text())
        if d.get('provenance')==provenance and d.get('maps') and d.get('outputs'):
            if all((SOURCE/path).is_file() and hashlib.sha256((SOURCE/path).read_bytes()).hexdigest()==digest
                   for path,digest in d['outputs'].items()):return d['maps']
    result={}
    light=read_map('Light_Skin_Color.1001.exr',4096)
    dark=read_map('Dark_Skin_Color.1001.exr',4096)
    light[:,:,:3]=light[:,:,:3]*TEXTURE_RECIPE['light_mix']+dark[:,:,:3]*TEXTURE_RECIPE['dark_mix'];light[:,:,3]=1
    head={'base_color':save_map('PortraitHead_BaseColor',light,True)};del light,dark
    rough=read_map('Skin_Roughness.1001.exr',4096)
    # Preserve authored regional roughness; retain a small oily lobe in UE.
    rough[:,:,:3]=np.clip(rough[:,:,:3],TEXTURE_RECIPE['roughness_min'],TEXTURE_RECIPE['roughness_max']);rough[:,:,3]=1
    head['roughness']=save_map('PortraitHead_Roughness',rough);del rough
    h=read_map('Skin_Height.1001.exr',4096)[:,:,0].copy()
    age=read_map('Skin_Age_Height.1001.exr',4096)[:,:,0]
    h+=(age-.5)*TEXTURE_RECIPE['age_height']
    head['normal']=save_map('PortraitHead_NormalGL',normal_from_height(h,4096*TEXTURE_RECIPE['head_normal_strength']))
    # A cheek-height patch, made periodic by removing its low Fourier modes,
    # supplies fine body pores without projecting facial features onto arms.
    patch=h[1840:2352,800:1312].copy();freq=np.fft.fft2(patch)
    yy,xx=np.ogrid[:512,:512];radius=np.minimum(yy,512-yy)**2+np.minimum(xx,512-xx)**2
    freq[radius<100]=0;patch=np.fft.ifft2(freq).real.astype(np.float32)
    micro={'normal':save_map('PortraitMicro_NormalGL',normal_from_height(patch,4096*TEXTURE_RECIPE['head_normal_strength']))}
    del h,age,patch,freq
    head['neck_base_color']='External/MakeHumanPack/skins/young_asian_male/young_lightskinned_male_diffuse3.png'
    result['head']=head;result['micro']=micro
    for key,prefix,tile in [('mouth','Mouth',1008),('iris','Iris',1007),('sclera','Sclera',1005)]:
        rgba=read_map(f'{prefix}_Color.{tile}.exr',2048)
        if key!='sclera':rgba[:,:,3]=1
        maps={'base_color':save_map('Portrait'+prefix+'_BaseColor',rgba,True)};del rgba
        if key!='sclera':
            rgba=read_map(f'{prefix}_Roughness.{tile}.exr',2048)
            maps['roughness']=save_map('Portrait'+prefix+'_Roughness',rgba);del rgba
        height=read_map(f'{prefix}_Height.{tile}.exr',2048)[:,:,0]
        maps['normal']=save_map('Portrait'+prefix+'_NormalGL',normal_from_height(height,2048*TEXTURE_RECIPE['eye_normal_strength']))
        result[key]=maps
    outputs={path:hashlib.sha256((SOURCE/path).read_bytes()).hexdigest() for maps in result.values() for path in maps.values()}
    cache.write_text(json.dumps({'provenance':provenance,'maps':result,'outputs':outputs},indent=2)+'\n',encoding='utf-8')
    return result

def material(name,kind,textures=None,**options):
    m=bpy.data.materials.new(name);m.use_nodes=True
    bsdf=m.node_tree.nodes.get('Principled BSDF')
    color=(.025,.018,.012,1) if kind=='hair' else (.35,.19,.12,1)
    if kind=='pupil':color=(.002,.001,.0005,1)
    bsdf.inputs['Base Color'].default_value=color
    bsdf.inputs['Roughness'].default_value=.4
    for channel,file in (textures or {}).items():
        if channel=='neck_base_color':continue
        node=m.node_tree.nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(str(SOURCE/file),check_existing=True)
        node.image.colorspace_settings.name='sRGB' if channel=='base_color' else 'Non-Color'
        if channel=='normal':
            normal=m.node_tree.nodes.new('ShaderNodeNormalMap');m.node_tree.links.new(node.outputs['Color'],normal.inputs['Color'])
            m.node_tree.links.new(normal.outputs['Normal'],bsdf.inputs['Normal'])
        else:m.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color' if channel=='base_color' else 'Roughness'])
    CATALOG[name]={'kind':kind,'textures':textures or {},**options}
    return m

def subset(source,name,selected,mats=None):
    src=source.data;faces=[src.polygons[i] for i in selected]
    ids=sorted({i for face in faces for i in face.vertices});remap={old:new for new,old in enumerate(ids)}
    mesh=bpy.data.meshes.new(name);mesh.from_pydata([src.vertices[i].co for i in ids],[],[[remap[i] for i in f.vertices] for f in faces]);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    for m in (mats if mats is not None else src.materials):mesh.materials.append(m)
    for out,original in zip(mesh.polygons,faces):out.material_index=original.material_index;out.use_smooth=True
    uv=mesh.uv_layers.new(name='UVMap');original=src.uv_layers[0]
    for a,b in zip(mesh.polygons,faces):
        for x,y in zip(a.loop_indices,b.loop_indices):uv.data[x].uv=original.data[y].uv
    for group in source.vertex_groups:obj.vertex_groups.new(name=group.name)
    for old,new in remap.items():
        for group in src.vertices[old].groups:obj.vertex_groups[group.group].add([new],group.weight,'REPLACE')
    return obj

def bind_head(obj,rig):
    for name in (b.name for b in rig.data.bones):
        if name not in obj.vertex_groups:obj.vertex_groups.new(name=name)
    for vertex in obj.data.vertices:
        h=smooth((vertex.co.z-1.477)/.080)
        if h:obj.vertex_groups['head'].add([vertex.index],h,'REPLACE')
        if h<1:obj.vertex_groups['neck'].add([vertex.index],1-h,'REPLACE')

def add_surface_data(obj,values=None):
    if len(obj.data.uv_layers)<2:
        uv=obj.data.uv_layers.new(name='BodyTransfer')
        for a,b in zip(uv.data,obj.data.uv_layers[0].data):a.uv=b.uv
    colors=obj.data.color_attributes.new(name='RiderSurface',type='FLOAT_COLOR',domain='POINT')
    for i,color in enumerate(colors.data):color.color=values[i] if values else (0,0,0,1)

def scalp_mask(p,n):
    front=max(0,-n.y);line=1.586+.102*front+.018*abs(n.x)
    ear=1.
    if abs(p.x)>.070:
        # Feather an anatomical ear ellipse. An axis-aligned exclusion left a
        # conspicuous rectangular bald patch above each ear in profile.
        distance=((p.y+.055)/.031)**2+((p.z-1.616)/.038)**2
        ear=smooth((distance-1.)/.35)
    return smooth((p.z-line)/.012)*ear

def groom_object(name,strands,mat,rig):
    vertices=[];faces=[];uvs=[];colors=[]
    for points,radius,seed in strands:
        start=len(vertices);count=len(points)
        for index,p in enumerate(points):
            t=index/(count-1);tangent=(points[min(index+1,count-1)]-points[max(index-1,0)]).normalized()
            ref=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((1,0,0))
            a=tangent.cross(ref).normalized();b=tangent.cross(a).normalized()
            r=radius*((1-t)**.65*.94+.06)
            for side in range(3):
                angle=side*math.tau/3
                vertices.append(p+(a*math.cos(angle)+b*math.sin(angle))*r)
                colors.append((tangent.x*.5+.5,tangent.y*.5+.5,tangent.z*.5+.5,1))
                uvs.append((side/3,t))
        for index in range(count-1):
            for side in range(3):faces.append((start+index*3+side,start+index*3+(side+1)%3,start+(index+1)*3+(side+1)%3,start+(index+1)*3+side))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.materials.append(mat);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    uv=mesh.uv_layers.new(name='UVMap')
    for face in mesh.polygons:
        face.use_smooth=True
        for loop in face.loop_indices:uv.data[loop].uv=uvs[mesh.loops[loop].vertex_index]
    bind_head(obj,rig);add_surface_data(obj,colors);return obj

def build():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierRider.blend'))
    rig=bpy.data.objects['CourierRig'];original=bpy.data.objects['SK_Courier']
    rig.animation_data.action=None
    for bone in rig.pose.bones:bone.matrix_basis=Matrix.Identity(4)
    reference=[(b.name,b.parent.name if b.parent else None,[list(r) for r in b.matrix_local]) for b in rig.data.bones]
    _,_,_,joints=read_body();maps=texture_set()
    skin_body=material('M_PortraitSkinBody','skin_body',{'base_color':maps['head']['neck_base_color'],**maps['micro']},normal_uv_scale=6.)
    skin_head=material('M_PortraitSkinHead','skin_head',maps['head'],neck_vertex_color='R',scalp_vertex_color='B')
    mouth=material('M_PortraitMouth','mouth',maps['mouth'])
    iris=material('M_PortraitIris','iris',maps['iris'])
    sclera=material('M_PortraitSclera','sclera',maps['sclera'],alpha_mask=True)
    pupil=material('M_PortraitPupil','pupil')
    cornea=material('M_PortraitCornea','cornea',ior=1.376,roughness=.025)
    hair=material('M_PortraitHair','hair',vertex_tangent=True)
    wet=material('M_PortraitTearline','tearline',roughness=.08)
    # Preserve the original skin UV triangle map for a genuine color transition
    # at the neck, rather than tinting a seam to an arbitrary constant.
    triangles=[];triangle_uv=[];source_uv=original.data.uv_layers[0]
    for p in original.data.polygons:
        if original.data.materials[p.material_index].name!='M_Skin':continue
        for i in range(1,len(p.vertices)-1):
            ids=(0,i,i+1);triangles.append(tuple(p.vertices[k] for k in ids))
            triangle_uv.append([Vector((*source_uv.data[p.loop_indices[k]].uv,0)) for k in ids])
    body_bvh=BVHTree.FromPolygons([v.co for v in original.data.vertices],triangles,all_triangles=True)
    body_faces=[p.index for p in original.data.polygons if original.data.materials[p.material_index].name not in ('M_Hair','M_EyeWhite','M_Iris','M_Pupil')
                and not (original.data.materials[p.material_index].name=='M_Skin' and min(original.data.vertices[i].co.z for i in p.vertices)>1.487)]
    body=subset(original,'PortraitBody',body_faces)
    for i,m in enumerate(body.data.materials):
        if m.name=='M_Skin':body.data.materials[i]=skin_body
    add_surface_data(body)
    with bpy.data.libraries.load(str(SOURCE/'Blender/CharMorphProbe.blend'),link=False) as (src,dst):dst.objects=['SM_CharMorphBust']
    bust=dst.objects[0];bpy.context.collection.objects.link(bust)
    sclera_ids={i for p in bust.data.polygons if bust.data.materials[p.material_index].name=='M_CM_Sclera' for i in p.vertices}
    sclera_center=Vector((0,(min(bust.data.vertices[i].co.y for i in sclera_ids)+max(bust.data.vertices[i].co.y for i in sclera_ids))/2,
                         (min(bust.data.vertices[i].co.z for i in sclera_ids)+max(bust.data.vertices[i].co.z for i in sclera_ids))/2))
    offset=Vector((0,joints['joint-l-eye'].y,joints['joint-l-eye'].z))-sclera_center
    eyes={}
    for side,sign in [('l',1),('r',-1)]:
        ids_side=[i for i in sclera_ids if bust.data.vertices[i].co.x*sign>0]
        eyes[side]=Vector([(min(bust.data.vertices[i].co[a] for i in ids_side)+max(bust.data.vertices[i].co[a] for i in ids_side))/2 for a in range(3)])+offset
    keep={'M_CM_Skin_1001':skin_head,'M_CM_Mouth':mouth,'M_CM_Iris':iris,'M_CM_Sclera':sclera,'M_CM_Pupil':pupil}
    ids=[p.index for p in bust.data.polygons if bust.data.materials[p.material_index].name in keep]
    head=subset(bust,'PortraitHead',ids)
    for i,m in enumerate(head.data.materials):
        if m.name in keep:head.data.materials[i]=keep[m.name]
    for v in head.data.vertices:v.co+=offset
    add_surface_data(head)
    uv1=head.data.uv_layers[1]
    colors=head.data.color_attributes['RiderSurface'];skin_index=list(head.data.materials).index(skin_head)
    skin_ids={i for p in head.data.polygons if p.material_index==skin_index for i in p.vertices}
    neck_weights={}
    for i in skin_ids:
        v=head.data.vertices[i];p=v.co.copy();neck=smooth((1.545-p.z)/.055)
        hit,normal,tri,distance=body_bvh.find_nearest(p)
        if hit is not None and p.z<1.540:
            amount=smooth((1.540-p.z)/.030)
            v.co=v.co.lerp(hit-normal*.0005,amount)
            abc=[original.data.vertices[k] for k in triangles[tri]]
            bary=barycentric_transform(hit,*(v.co for v in abc),Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
            weights=collections.defaultdict(float)
            for vertex,factor in zip(abc,bary):
                for group in vertex.groups:weights[original.vertex_groups[group.group].name]+=group.weight*max(0.,factor)
            total=sum(weights.values())
            if total:neck_weights[i]=(amount,{name:weight/total for name,weight in weights.items()})
        colors.data[i].color=(neck,0,0,1)
    for p in head.data.polygons:
        if p.material_index!=skin_index:continue
        center=sum((head.data.vertices[i].co for i in p.vertices),Vector())/len(p.vertices)
        hit,normal,tri,distance=body_bvh.find_nearest(center)
        if hit is None:continue
        a,b,c=(original.data.vertices[k].co for k in triangles[tri])
        # Select a chart per face and store UVs per corner. Sharing one nearest
        # UV per vertex bridges unrelated body islands at the back-neck seam.
        for li in p.loop_indices:
            i=head.data.loops[li].vertex_index
            uv1.data[li].uv=barycentric_transform(head.data.vertices[i].co,a,b,c,*triangle_uv[tri]).xy
    bind_head(head,rig)
    # The overlap follows the existing body exactly, including its deformed
    # neck weights, instead of separating when the rider looks down.
    for index,(amount,weights) in neck_weights.items():
        existing={head.vertex_groups[g.group].name:g.weight for g in head.data.vertices[index].groups}
        for name in set(existing)|set(weights):
            value=existing.get(name,0)*(1-amount)+weights.get(name,0)*amount
            group=head.vertex_groups.get(name) or head.vertex_groups.new(name=name)
            if value>1e-8:group.add([index],value,'REPLACE')
            elif name in existing:group.remove([index])
    # Subdivision smooths the face silhouette and interpolates UVs and neck
    # weights; skin microstructure remains authored CC0 tangent normal detail.
    bpy.context.view_layer.objects.active=head;head.select_set(True)
    mod=head.modifiers.new('Portrait silhouette refinement','SUBSURF');mod.levels=1;mod.render_levels=1
    bpy.ops.object.modifier_apply(modifier=mod.name)
    head.data.update()
    skin_ids={i for p in head.data.polygons if head.data.materials[p.material_index]==skin_head for i in p.vertices}
    head_surface=BVHTree.FromPolygons([v.co for v in head.data.vertices],[list(p.vertices) for p in head.data.polygons if head.data.materials[p.material_index]==skin_head])
    colors=head.data.color_attributes['RiderSurface']
    for i in skin_ids:
        v=head.data.vertices[i];c=colors.data[i].color[:];colors.data[i].color=(c[0],c[1],scalp_mask(v.co,v.normal),1)
    # Separate physical corneal bulges; the CC0 sclera mask exposes the iris.
    cornea_faces=[]
    for p in head.data.polygons:
        if head.data.materials[p.material_index]!=sclera:continue
        center=p.center;eye=eyes['l' if center.x>0 else 'r']
        radius=math.hypot(center.x-eye.x,center.z-eye.z)
        if center.y<eye.y-.003 and radius<.0105:cornea_faces.append(p.index)
    lens=subset(head,'PortraitCornealBulges',cornea_faces)
    for i in range(len(lens.data.materials)):lens.data.materials[i]=cornea
    for v in lens.data.vertices:v.co+=v.normal*.00008
    add_surface_data(lens)
    rng=random.Random(20261018);strands=[];scalp_roots=[]
    center=Vector((0,-.035,1.660))
    for k in range(48000):
        z=1-2*(k+.5)/48000;angle=k*math.pi*(3-math.sqrt(5))
        direction=Vector((math.sqrt(max(0,1-z*z))*math.cos(angle),math.sqrt(max(0,1-z*z))*math.sin(angle),z))
        origin=center+direction*.4;root,normal,_,_=head_surface.ray_cast(origin,-direction,.8)
        if root is None or scalp_mask(root,normal)<.95:continue
        root+=normal*.00018
        scalp_roots.append((root,normal,k))
    # Sample the entire scalp, including temples/nape; stopping an ordered
    # Fibonacci traversal early would leave the sides and nape bare.
    selected_roots=rng.sample(scalp_roots,min(16000,len(scalp_roots)))
    for root,normal,k in selected_roots:
        flow=Vector((.18 if root.x>0 else -.05,1,-.25));flow=(flow-normal*flow.dot(normal)).normalized()
        top=root.z>1.685
        length=rng.uniform(.022,.034) if top else rng.uniform(.007,.014)
        lift=rng.uniform(.004,.007) if top else rng.uniform(.0015,.003)
        points=[]
        # Four rings preserve the short swept silhouette with fewer triangles
        # than the first twelve-thousand-strand groom's six-ring tubes.
        for step in range(4):
            t=step/3;p=root+flow*(length*t)+normal*(math.sin(t*math.pi)*lift+.0004*t)
            p+=normal*(math.sin(t*9+k)*.0003*t)
            points.append(p)
        strands.append((points,rng.uniform(.000042,.000060),k))
    scalp_count=len(strands)
    # Individual tapered brow hairs follow the face surface, with sparse edges.
    for side in ('l','r'):
        eye=eyes[side];sign=1 if side=='l' else -1
        for k in range(760):
            u=rng.random();x=sign*(.012+.040*u);z=eye.z+.019+.002*math.sin(u*math.pi)+rng.gauss(0,.0012)
            root,normal,_,_=head_surface.ray_cast(Vector((x,-.5,z)),Vector((0,1,0)),.8)
            if root is None:continue
            root+=normal*.00016;direction=Vector((sign*.75,0,.55-.75*u)).normalized()*rng.uniform(.002,.0038)
            strands.append(([root,root+direction*.45+normal*.0002,root+direction],rng.uniform(.000027,.000042),k))
    # Vitruvian lids fold back into the eye socket, so they have no open
    # topology edge. Project the anatomical lid curve onto the real surface.
    wet_strands=[];lashes=0
    for side in ('l','r'):
        eye=eyes[side];rim=[]
        for k in range(96):
            angle=k*math.tau/96;upper=math.sin(angle)>=0
            sample=eye+Vector((.0122*math.cos(angle),-.0128,(.0057 if upper else .0048)*math.sin(angle)))
            root,normal,_,distance=head_surface.find_nearest(sample)
            if root is None or distance>.008 or root.y>eye.y-.005:continue
            root+=normal*.00012;rim.append(root)
            if k%2 and not upper:continue
            length=rng.uniform(.0027,.0040) if upper else rng.uniform(.0012,.0021)
            direction=Vector((.10*(1 if root.x>0 else -1),-.85,.30 if upper else -.35))*length
            strands.append(([root,root+direction*.45,root+direction],.000033 if upper else .000025,k));lashes+=1
        if len(rim)>12:
            for a,b in zip(rim,rim[1:]+rim[:1]):
                if 1e-5<(a-b).length<.004:wet_strands.append(([a,b],.00012,0))
    assert lashes>=100,('Eyelid fitting did not produce complete lashes',lashes)
    for k in range(1400):
        x=rng.uniform(-.057,.057);z=rng.uniform(1.524,1.585)
        if abs(x)<.022 and z>1.548:continue
        root,normal,_,_=head_surface.ray_cast(Vector((x,-.5,z)),Vector((0,1,0)),.8)
        if root is None or normal.y>-.3:continue
        root+=normal*.0001
        strands.append(([root,root+normal*rng.uniform(.0002,.00055)+Vector((0,0,-.00015))],.000030,k))
    groom=groom_object('PortraitIndividualHair',strands,hair,rig)
    parts=[body,head,lens,groom]
    if wet_strands:parts.append(groom_object('PortraitWetLidRim',wet_strands,wet,rig))
    # Remove old head objects from this new source project only.
    for obj in list(bpy.data.objects):
        if obj not in parts and obj!=rig:bpy.data.objects.remove(obj,do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:obj.select_set(True)
    bpy.context.view_layer.objects.active=body;bpy.ops.object.join();mesh=bpy.context.object;mesh.name='SK_Courier'
    mesh.parent=rig
    for modifier in list(mesh.modifiers):mesh.modifiers.remove(modifier)
    modifier=mesh.modifiers.new('Unchanged courier skeleton','ARMATURE');modifier.object=rig
    # Imported library material slots not used by polygons need not become game assets.
    bpy.ops.object.material_slot_remove_unused()
    for i,m in enumerate(mesh.data.materials):
        if m.name not in CATALOG and m.name.endswith('.001'):
            original_mat=bpy.data.materials.get(m.name[:-4])
            if original_mat:mesh.data.materials[i]=original_mat
    assert reference==[(b.name,b.parent.name if b.parent else None,[list(r) for r in b.matrix_local]) for b in rig.data.bones]
    scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1;scene.render.fps=30
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
    bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_Courier.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},global_scale=1,apply_unit_scale=True,
                            axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False,use_mesh_modifiers=True,use_triangles=True,use_tspace=True,mesh_smooth_type='FACE',path_mode='STRIP')
    for name,definition in CATALOG.items():definition['texture_sha256']={key:hashlib.sha256((SOURCE/file).read_bytes()).hexdigest() for key,file in definition['textures'].items()}
    (SOURCE/'portrait_materials.json').write_text(json.dumps({'materials':CATALOG,'normal_convention':'OpenGL +Y','albedo_color_space':'sRGB','linear_maps':['normal','roughness']},indent=2)+'\n',encoding='utf-8')
    report={'source_head':'CharMorph-Vitruvian c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44 CC0','body':'Existing CC0 MakeHuman courier',
            'fitting_translation_m':list(offset),'skeleton_reference_unchanged':True,'bones':len(reference),'scalp_strands':scalp_count,'total_hair_strands':len(strands),
            'lashes':lashes,'scalp_root_bounds_m':[[min(p[0][axis] for p in selected_roots),max(p[0][axis] for p in selected_roots)] for axis in range(3)],
            'vertices':len(mesh.data.vertices),'triangles':sum(len(p.vertices)-2 for p in mesh.data.polygons),
            'skin_source_mix':{'light':.82,'dark':.18,'age_height':.035},'units':'meters; existing FBX centimeter convention',
            'limitations':['Rigid head/neck skinning; no new facial expression rig','Hero strand geometry requires LOD validation at gameplay distance']}
    (SOURCE/'portrait_rider.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierPhotoreal.blend'))
    print('PHOTOREAL RIDER',json.dumps(report),flush=True)

if __name__=='__main__':build()
