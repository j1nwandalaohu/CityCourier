"""Original CityCourier meshes and rig. Run with Blender --background --python this.py.

Centimeters, X forward, Z up. Retains editable .blend sources and FBX exports.
No external geometry/textures; only the project's licensed Noto font is reused.
"""
import bpy
import math
import pathlib
import json
import csv
from mathutils import Vector

ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'AssetsSource/CityCourier'
EXPORT=OUT/'Exports'
EXPORT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=.01
bpy.context.scene.render.fps=30
materials={}
assets=[]

def material(name,color,metal=0,rough=.6,emission=0):
    m=bpy.data.materials.new(name)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Metallic'].default_value=metal
    p.inputs['Roughness'].default_value=rough
    if emission:
        p.inputs['Emission Color'].default_value=(*color,1)
        p.inputs['Emission Strength'].default_value=emission
    materials[name]=m
    return name

for args in [
    ('Amber',(.92,.47,.045)),('Teal',(.035,.20,.21)),('Fabric',(.055,.075,.10)),
    ('Skin',(.52,.30,.19)),('Hair',(.025,.018,.015)),('White',(.76,.77,.70)),
    ('Rubber',(.018,.021,.025)),('Steel',(.32,.36,.38),.8,.26),
    ('Paint',(.055,.18,.19),.38,.24),('Glass',(.055,.10,.13),.35,.2),
    ('Lamp',(.95,.68,.30),0,.3,3),('RedLamp',(.6,.025,.012),0,.25,2),
    ('Plaster',(.48,.40,.30)),('Concrete',(.24,.27,.26)),('Brick',(.33,.12,.065)),
    ('Tile',(.26,.32,.29)),('Wood',(.25,.115,.042)),('Canvas',(.57,.12,.065)),
    ('Leaf',(.07,.19,.065)),('WarmWindow',(.78,.48,.19),0,.4,.6),
    ('SignTeal',(.025,.12,.13)),('SignRed',(.30,.035,.025)),('SignCream',(.84,.68,.40))]: material(*args)

def finish_object(obj,name,mat,bone=None):
    obj.name=name
    if mat:obj.data.materials.append(materials[mat])
    if bone:
        group=obj.vertex_groups.new(name=bone)
        group.add(list(range(len(obj.data.vertices))),1,'REPLACE')
    return obj

def cube(name,p,size,mat,bevel=0,bone=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p)
    o=bpy.context.object;o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=o.modifiers.new('Soft manufactured edges','BEVEL');mod.width=bevel;mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return finish_object(o,name,mat,bone)

def ellipsoid(name,p,size,mat,bone=None,segments=20):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=12,radius=1,location=p)
    o=bpy.context.object;o.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    for face in o.data.polygons:face.use_smooth=True
    return finish_object(o,name,mat,bone)

def cylinder(name,a,b,r,mat,r2=None,bone=None,vertices=16):
    a,b=Vector(a),Vector(b)
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r,radius2=r if r2 is None else r2,depth=(b-a).length,location=(a+b)/2)
    o=bpy.context.object;o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    for face in o.data.polygons:face.use_smooth=len(face.vertices)==4
    return finish_object(o,name,mat,bone)

def torus(name,p,major,minor,mat,rotation=(0,0,0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=major,minor_radius=minor,major_segments=32,minor_segments=10,location=p,rotation=rotation)
    o=bpy.context.object
    for face in o.data.polygons:face.use_smooth=True
    return finish_object(o,name,mat)

def objects_since(before):return [o for o in bpy.context.scene.objects if o.name not in before]
def join(objects,name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join()
    o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    return o

def export(name,objects,animation=False):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.export_scene.fbx(filepath=str(EXPORT/(name+'.fbx')),use_selection=True,
        object_types={'MESH','ARMATURE'},axis_forward='-Y',axis_up='Z',global_scale=1,
        apply_unit_scale=True,add_leaf_bones=False,use_armature_deform_only=True,
        bake_anim=animation,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0,mesh_smooth_type='FACE')

def static(name,category,builder,identifier=None):
    before=set(o.name for o in bpy.context.scene.objects)
    builder()
    o=join(objects_since(before),name)
    export(name,[o])
    assets.append(dict(name=name,category=category,kind='static',asset_id=identifier or name.lower()))
    o.hide_render=True;o.hide_set(True)
    return o

# Rider: fitted work jacket, trousers, gloves, helmet and reflective detailing.
before=set(o.name for o in bpy.context.scene.objects)
ellipsoid('Jacket torso',(0,0,124),(13,21,26),'Amber','spine')
ellipsoid('Jacket waist',(0,0,104),(12,18,13),'Amber','pelvis')
ellipsoid('Trouser hips',(0,0,94),(12,20,12),'Fabric','pelvis')
cube('Jacket zipper',(12.3,0,126),(1,1,38),'Steel',.3,'spine')
for side in [-1,1]:
    cube('Reflective chest band',(12.2,side*12,132),(1.5,13,3),'White',.4,'spine')
    cube('Chest pocket',(12.5,side*12,119),(2,11,10),'Teal',1,'spine')
    cube('Reflective back band',(-12.2,side*10,132),(1.5,17,3),'White',.4,'spine')
cylinder('Neck',(0,0,142),(0,0,153),6,'Skin',bone='neck')
ellipsoid('Face',(1,0,160),(10.5,9.5,13),'Skin','head')
ellipsoid('Hair rear',(-4,0,164),(8,10,10),'Hair','head')
ellipsoid('Nose',(11,0,158),(3,2,3),'Skin','head')
for side in [-1,1]:
    ellipsoid('Ear',(0,side*10,159),(2,2.5,4),'Skin','head',12)
    ellipsoid('Eye white',(10,side*4.1,162),(.8,1.4,.8),'White','head',12)
    ellipsoid('Eye',(10.75,side*4.1,162),(.3,.5,.6),'Hair','head',12)
    cylinder('Eyebrow',(10.1,side*4.1-1.5,164),(10.1,side*4.1+1.5,164),.45,'Hair',bone='head',vertices=8)
    cylinder('Helmet strap',(0,side*10,168),(5,side*7,148),.8,'Fabric',bone='head',vertices=8)
cylinder('Mouth',(10,-2.5,153),(10,2.5,153),.4,'Hair',bone='head',vertices=8)
ellipsoid('Helmet shell',(0,0,171),(12.5,12,7.5),'Amber','head')
cube('Helmet peak',(11,0,169),(13,24,2),'Teal',1,'head')
cube('Helmet stripe',(0,0,178),(18,3,1),'White',.4,'head')

bones=[('root',(0,0,0),(0,0,10),None),('pelvis',(0,0,94),(0,0,110),'root'),
       ('spine',(0,0,110),(0,0,142),'pelvis'),('neck',(0,0,142),(0,0,151),'spine'),
       ('head',(0,0,151),(0,0,175),'neck')]
for s,suffix in [(-1,'l'),(1,'r')]:
    shoulder=(0,s*21,140);elbow=(0,s*28,113);wrist=(3,s*30,90)
    hip=(0,s*10,95);knee=(1,s*11,52);ankle=(0,s*11,11)
    bones += [('upperarm_'+suffix,shoulder,elbow,'spine'),('forearm_'+suffix,elbow,wrist,'upperarm_'+suffix),
              ('hand_'+suffix,wrist,(4,s*30,80),'forearm_'+suffix),('thigh_'+suffix,hip,knee,'pelvis'),
              ('calf_'+suffix,knee,ankle,'thigh_'+suffix),('foot_'+suffix,ankle,(17,s*11,6),'calf_'+suffix)]
    cylinder('Jacket upper sleeve',elbow,shoulder,7,'Amber',10,'upperarm_'+suffix)
    ellipsoid('Shoulder',shoulder,(9,9,9),'Amber','upperarm_'+suffix)
    cylinder('Jacket lower sleeve',wrist,elbow,4.5,'Amber',7,'forearm_'+suffix)
    ellipsoid('Elbow cloth',elbow,(7,7,7),'Amber','forearm_'+suffix)
    cylinder('Sleeve cuff',wrist,Vector(wrist)+Vector((0,0,4)),4.8,'Teal',bone='forearm_'+suffix)
    ellipsoid('Gloved hand',(4,s*30,85),(4,3.3,6),'Fabric','hand_'+suffix)
    ellipsoid('Thumb',(7,s*28,86),(2,2,3.5),'Fabric','hand_'+suffix)
    cylinder('Trouser thigh',knee,hip,8,'Fabric',11,'thigh_'+suffix)
    ellipsoid('Knee',knee,(8,8,8),'Fabric','calf_'+suffix)
    cylinder('Trouser calf',ankle,knee,5.8,'Fabric',8,'calf_'+suffix)
    cube('Shoe sole',(7,s*11,2.5),(27,13,5),'Rubber',2,'foot_'+suffix)
    ellipsoid('Work shoe',(6,s*11,7),(13,6.5,6),'Fabric','foot_'+suffix)
    cube('Shoe reflective stripe',(11,s*11,10),(9,11,1),'White',.4,'foot_'+suffix)
mesh=join(objects_since(before),'SK_Courier')
arm_data=bpy.data.armatures.new('CourierSkeleton')
rig=bpy.data.objects.new('CourierRig',arm_data)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode='EDIT')
for name,head,tail,parent in bones:
    b=arm_data.edit_bones.new(name);b.head=head;b.tail=tail
    if parent:b.parent=arm_data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
mesh.parent=rig
modifier=mesh.modifiers.new('Courier skin','ARMATURE');modifier.object=rig
rig.animation_data_create()
bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=31
export('SK_Courier',[rig,mesh])
assets.append(dict(name='SK_Courier',category='Characters',kind='skeletal',asset_id='courier_rider'))

for name,frames,amplitude in [('Idle',61,.025),('Walk',31,.53),('Run',25,.85),('Ride',61,.015)]:
    action=bpy.data.actions.new('A_'+name);rig.animation_data.action=action
    bpy.context.scene.frame_end=frames
    for frame in range(1,frames+1):
        phase=(frame-1)/(frames-1)*math.tau
        for pb in rig.pose.bones:
            pb.rotation_mode='XYZ';pb.rotation_euler=(0,0,0);pb.location=(0,0,0)
        pelvis=rig.pose.bones['pelvis']
        if name=='Ride':
            # Seated pelvis at 82cm, knees forward, elbows bent to grips.
            pelvis.location=(0,-12,0)
            rig.pose.bones['spine'].rotation_euler.z=-.08
            for s,suffix in [(-1,'l'),(1,'r')]:
                rig.pose.bones['thigh_'+suffix].rotation_euler.z=-1.8
                rig.pose.bones['calf_'+suffix].rotation_euler.z=1.8
                rig.pose.bones['upperarm_'+suffix].rotation_euler.z=-.72
                rig.pose.bones['forearm_'+suffix].rotation_euler.z=-.9
                rig.pose.bones['foot_'+suffix].rotation_euler.x=0
            rig.pose.bones['head'].rotation_euler.y=math.sin(phase)*.025
        elif name=='Idle':
            rig.pose.bones['spine'].rotation_euler.x=math.sin(phase)*.015
            rig.pose.bones['head'].rotation_euler.y=math.sin(phase)*.03
        else:
            pelvis.location.y=abs(math.sin(phase))* (2 if name=='Walk' else 5)
            for s,suffix in [(-1,'l'),(1,'r')]:
                wave=math.sin(phase)*s
                rig.pose.bones['thigh_'+suffix].rotation_euler.z=wave*amplitude
                rig.pose.bones['calf_'+suffix].rotation_euler.z=max(0,-wave)*(amplitude*1.3)
                rig.pose.bones['upperarm_'+suffix].rotation_euler.z=-wave*amplitude*.7
                rig.pose.bones['forearm_'+suffix].rotation_euler.z=-.3 if name=='Walk' else -.95
            rig.pose.bones['spine'].rotation_euler.x=.05 if name=='Walk' else .15
        for pb in rig.pose.bones:
            pb.keyframe_insert('rotation_euler',frame=frame,group=pb.name)
            pb.keyframe_insert('location',frame=frame,group=pb.name)
    for curve in action.fcurves:
        for key in curve.keyframe_points:key.interpolation='LINEAR'
    bpy.context.scene.frame_set(1)
    export('A_'+name,[rig],True)
    assets.append(dict(name='A_'+name,category='Characters',kind='animation',asset_id='courier_'+name.lower()))
rig.animation_data.action=None
for pb in rig.pose.bones:pb.rotation_euler=(0,0,0);pb.location=(0,0,0)
rig.hide_render=True;rig.hide_set(True);mesh.hide_render=True;mesh.hide_set(True)

def scooter():
    for x in [-63,63]:
        torus('Pneumatic tyre',(x,0,25),20,5,'Rubber',(math.pi/2,0,0))
        cylinder('Wheel hub',(x,-6,25),(x,6,25),13,'Steel',vertices=24)
        for i in range(8):
            angle=i*math.tau/8
            cylinder('Alloy spoke',(x,0,25),(x+17*math.cos(angle),0,25+17*math.sin(angle)),1.5,'Steel',vertices=8)
    cube('Battery housing',(-6,0,32),(86,32,24),'Paint',8)
    cube('Non slip deck',(10,0,45),(72,40,6),'Rubber',3)
    cylinder('Main frame',(-62,0,28),(-30,0,68),4,'Steel')
    cylinder('Rear suspension',(-65,-12,28),(-38,-12,65),3,'Amber')
    cylinder('Rear suspension',(-65,12,28),(-38,12,65),3,'Amber')
    cylinder('Seat support',(-30,0,44),(-30,0,75),5,'Steel')
    cube('Sculpted saddle',(-27,0,79),(58,36,13),'Rubber',6)
    cube('Tail fairing',(-64,0,55),(24,34,22),'Paint',6)
    cube('Tail light',(-77,0,63),(3,22,7),'RedLamp',2)
    cube('Rear mudguard',(-77,0,34),(19,15,8),'Rubber',3)
    cube('Front leg shield',(47,0,71),(17,35,59),'Paint',8)
    for side in [-1,1]:
        cylinder('Front fork',(63,side*10,25),(49,side*10,85),2.5,'Steel')
        cylinder('Handle grip',(35,side*20,111),(35,side*35,111),2.5,'Rubber')
        cylinder('Mirror stalk',(35,side*27,112),(40,side*35,133),1.1,'Steel')
        ellipsoid('Rear mirror',(40,side*37,134),(2,7,4),'Glass')
        cylinder('Delivery rack strut',(-67,side*17,57),(-67,side*17,91),1.8,'Steel')
    cylinder('Steering column',(49,0,78),(35,0,110),3,'Steel')
    cylinder('Handlebar',(35,-28,111),(35,28,111),2,'Steel')
    cube('Headlamp housing',(51,0,99),(14,24,17),'Paint',5)
    cube('Headlamp lens',(59,0,99),(2,19,12),'Lamp',3)
    cube('Instrument console',(42,0,113),(12,14,4),'Rubber',2)
    cube('Instrument display',(42,0,115.5),(8,9,1),'Glass',1)
    cube('Insulated delivery box',(-67,0,111),(43,47,43),'Amber',4)
    cube('Delivery lid',(-67,0,134),(45,49,4),'Teal',2)
    for side in [-1,1]:
        cube('Box reflective stripe',(-67,side*24,113),(39,1,5),'White',.5)
    cube('Box badge',(-89,0,116),(1,25,16),'Teal',1)
static('SM_Scooter','Scooter',scooter,'courier_scooter')

def window(x,y,z,width=130,height=160,warm=False):
    cube('Window reveal',(x,y,z),(width+20,18,height+20),'Concrete',2)
    cube('Recessed glazing',(x,y-11,z),(width,4,height),'WarmWindow' if warm else 'Glass')
    cube('Window mullion',(x,y-16,z),(6,6,height),'White')
    cube('Window sill',(x,y-22,z-height/2),(width+30,35,10),'Concrete',2)
    cube('Lintel',(x,y-15,z+height/2),(width+24,12,10),'White',1)

def building(shop=False):
    width,depth,height=(800,1200,1800)
    cube('Rendered structure',(0,0,height/2),(width,depth,height),'Plaster',8)
    cube('Tile skirting',(0,-606,150),(width,20,300),'Tile',2)
    for level in range(6):
        z=150+level*285
        cube('Floor belt',(0,-612,z+140),(width+16,25,12),'Concrete',2)
        for col in range(3):
            x=-265+col*265
            if level==0 and shop:
                cube('Shopfront frame',(x,-620,140),(242,22,260),'Teal',3)
                cube('Store glazing',(x,-635,140),(224,5,235),'WarmWindow')
                cube('Store door bar',(x,-641,140),(6,8,230),'Steel')
            else:
                window(x,-610,z,135,160,(col+level)%4==0)
                if (level+col)%3==0:
                    cube('Balcony slab',(x,-675,z-90),(205,150,15),'Concrete',2)
                    for rail in [-80,-40,0,40,80]:cylinder('Balcony bar',(x+rail,-742,z-80),(x+rail,-742,z+8),2,'Steel',vertices=8)
                    cylinder('Balcony handrail',(x-100,-742,z+10),(x+100,-742,z+10),3,'Steel',vertices=8)
                cube('Air conditioner',(x+83,-632,z-45),(50,30,36),'White',3)
                for vent in range(4):cube('AC grille',(x+83,-649,z-56+vent*7),(42,2,2),'Steel')
    # Back and sides carry windows as well; distant blocks are not blank boxes.
    for level in range(1,6):
        for x in [-265,0,265]:
            cube('Rear window',(x,603,150+level*285),(135,8,160),'Glass')
        for side in [-1,1]:
            for y in [-320,100,400]:cube('Side window',(side*403,y,150+level*285),(8,130,160),'Glass')
    cube('Roof coping',(0,0,1810),(830,1230,24),'Concrete',3)
    for side in [-1,1]:cube('Roof parapet',(side*397,0,1850),(18,1200,80),'Plaster',2)
    cube('Roof tank plinth',(150,200,1850),(200,220,60),'Concrete',3)
    cylinder('Water tank',(150,200,1880),(150,200,2040),75,'Steel',vertices=20)
    cube('Stair head',(-150,100,1910),(230,260,180),'Plaster',5)
static('SM_Residential','Architecture',lambda:building(False),'courier_residential')
static('SM_ShopHouse','Architecture',lambda:building(True),'courier_shophouse')

def shop():
    cube('Shop shell',(0,0,200),(650,500,400),'Plaster',4)
    cube('Store interior dark',(0,-253,150),(600,5,275),'Glass')
    for x in [-285,0,285]:cube('Shop uprights',(x,-265,150),(12,20,300),'Teal',2)
    cube('Transom',(0,-267,300),(600,20,15),'Teal',2)
    cube('Door rail',(0,-270,155),(8,12,285),'Steel')
    cube('Door handle',(25,-283,150),(4,10,60),'Steel',1)
    cube('Entrance step',(0,-290,7),(650,95,14),'Concrete',3)
    cube('Sign fascia',(0,-270,355),(650,30,95),'Teal',3)
    cube('Awning',(0,-340,305),(680,180,15),'Canvas',2)
    for x in range(-300,301,100):cube('Awning stripes',(x,-340,314),(32,180,2),'White')
    cube('Roof lip',(0,0,415),(690,540,30),'Concrete',4)
    for x in [-220,220]:cube('Warm storefront panel',(x,-257,170),(120,3,180),'WarmWindow')
static('SM_Shop','Architecture',shop,'courier_shop')

def gate():
    for x in [-250,250]:cube('Masonry pillar',(x,0,190),(80,100,380),'Brick',5)
    cube('Gate canopy',(0,0,395),(650,160,40),'Concrete',4)
    for side in [-1,1]:
        for i in range(8):cylinder('Security gate bar',(side*(70+i*21),0,20),(side*(70+i*21),0,280),2.5,'Steel',vertices=8)
        cube('Gate cross rail',(side*145,0,150),(190,6,8),'Steel')
static('SM_Gate','Architecture',gate,'courier_gate')

def wall():
    cube('Brick boundary',(0,0,110),(600,45,220),'Brick',3)
    for z in range(20,220,40):cube('Mortar courses',(0,-24,z),(600,2,2),'Concrete')
    cube('Wall cap',(0,0,226),(620,60,12),'Concrete',2)
static('SM_Wall','Architecture',wall)

def stall():
    cube('Stall counter',(0,0,80),(190,95,90),'Steel',3)
    cube('Wood worktop',(0,0,128),(205,105,10),'Wood',2)
    for x in [-95,95]:
        for y in [-45,45]:cylinder('Canopy post',(x,y,0),(x,y,250),3,'Steel',vertices=10)
    cube('Striped stall roof',(0,0,250),(225,140,14),'Canvas',3)
    for x in range(-100,101,40):cube('Canopy cream stripe',(x,0,258),(18,140,2),'White')
    for x in [-60,0,60]:
        cylinder('Cooking pot',(x,0,133),(x,0,157),22,'Steel',vertices=24)
        ellipsoid('Food basket',(x,-25,136),(20,13,6),'Amber')
    cube('Stall sign plate',(0,-55,88),(160,4,45),'Teal',2)
static('SM_Stall','StreetProps',stall)

def lamp():
    cylinder('Streetlight foot',(0,0,0),(0,0,35),14,'Concrete')
    cylinder('Streetlight pole',(0,0,25),(0,0,640),5,'Steel',3)
    cylinder('Lamp arm',(0,0,635),(110,0,665),4,'Steel',3)
    cube('Luminaire',(110,0,660),(80,30,18),'Steel',5)
    cube('Luminous diffuser',(110,0,649),(65,23,2),'Lamp',1)
static('SM_StreetLamp','StreetProps',lamp)

def planter():
    cube('Concrete planter',(0,0,30),(160,100,60),'Concrete',5)
    cube('Soil',(0,0,60),(140,80,5),'Wood')
    for x in [-50,0,50]:ellipsoid('Shrub',(x,0,105),(45,38,55),'Leaf',segments=12)
static('SM_Planter','StreetProps',planter)
def bench():
    for x in [-60,60]:cube('Bench leg',(x,0,23),(10,40,46),'Steel',2)
    for y in [-20,-7,7,20]:cube('Seat board',(0,y,48),(170,10,6),'Wood',1)
    for z in [65,80]:cube('Back board',(0,23,z),(170,7,11),'Wood',1)
static('SM_Bench','StreetProps',bench)
def ac():
    cube('Air conditioner case',(0,0,30),(90,36,60),'White',3)
    torus('Fan grille',(18,-20,30),22,1.5,'Steel',(math.pi/2,0,0))
    for x in range(-35,-5,6):cube('Vent slat',(x,-20,30),(2,3,48),'Steel')
static('SM_AirConditioner','StreetProps',ac)
def binmesh():
    cube('Litter bin',(0,0,42),(45,40,84),'Teal',5)
    cube('Lid',(0,0,88),(50,45,8),'Rubber',2)
    cube('Recycling opening',(0,-22,68),(28,3,12),'Rubber',2)
static('SM_Bin','StreetProps',binmesh)
def bollard():
    cylinder('Bollard',(0,0,0),(0,0,80),7,'Steel')
    cylinder('Reflector',(0,0,60),(0,0,70),7.5,'Amber')
static('SM_Bollard','StreetProps',bollard)

# Geometry signs avoid reliance on an unlicensed CJK texture atlas or editor-only text rendering.
font=bpy.data.fonts.load(str(OUT/'Fonts/NotoSansCJKsc-Regular.otf'))
signs=['阿荣面馆','巷口烧腊饭','青禾茶铺','邻里小厨','骑手驿站','西苑小区',
       '东里公寓','南街写字楼','北岸工作室','桂花里','梧桐苑','中心便利店','南风设计社',
       '老街理发','南城五金','兴记修车','巷口夜市','南城 · 慢行']
for i,label in enumerate(signs):
    def make_sign(label=label,i=i):
        cube('Enamel sign backing',(0,0,0),(600,16,90),'SignRed' if i%3==0 else 'SignTeal',3)
        cube('Sign border top',(0,-10,42),(590,3,3),'SignCream',.5)
        cube('Sign border bottom',(0,-10,-42),(590,3,3),'SignCream',.5)
        curve=bpy.data.curves.new('Chinese lettering','FONT');curve.body=label;curve.font=font
        curve.align_x='CENTER';curve.align_y='CENTER';curve.size=min(66,540/max(1,len(label)));curve.extrude=.6;curve.bevel_depth=.15;curve.resolution_u=4
        obj=bpy.data.objects.new('Original shop name',curve);bpy.context.collection.objects.link(obj)
        obj.location=(0,-10,0);obj.rotation_euler=(math.pi/2,0,0)
        obj.data.materials.append(materials['SignCream'])
        bpy.context.view_layer.objects.active=obj;obj.select_set(True)
        bpy.ops.object.convert(target='MESH')
    static('SM_Sign_%02d'%i,'StreetProps',make_sign)

assets_meta={'assets':assets,'signs':signs,'units':'centimeters','author':'CityCourier project','license':'Original project artwork'}
(OUT/'art-catalog.json').write_text(json.dumps(assets_meta,ensure_ascii=False,indent=2),encoding='utf-8')
# Show the rigged rider on opening the editable source; other assets remain individually
# named and can be revealed from the outliner without affecting their export origins.
rig.hide_render=False;rig.hide_set(False);mesh.hide_render=False;mesh.hide_set(False)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CityCourier_Original.blend'))
manifest=ROOT/'docs/art/asset-manifest.csv'
with manifest.open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
ids={a['asset_id'] for a in assets}
rows=[r for r in rows if r['asset_id'] not in ids]
for a in assets:
    rows.append(dict(asset_id=a['asset_id'],source_path='AssetsSource/CityCourier/Exports/'+a['name']+'.fbx',
        ue_path='/Game/CityCourier/Art/'+a['category']+'/'+a['name'],author='CityCourier project',source_url='',license='Original project artwork',is_placeholder='false'))
with manifest.open('w',encoding='utf-8',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=['asset_id','source_path','ue_path','author','source_url','license','is_placeholder'])
    writer.writeheader();writer.writerows(rows)
print('CityCourier original art exported:',len(assets),'assets')
