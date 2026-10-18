"""Original, deterministic street detail meshes. All dimensions are metres."""
import math
import random
import bpy
import bmesh
from mathutils import Vector, Matrix
from art_common import torus, sphere, join, text_mesh, uv_box


def mesh(name, vertices, faces, mat):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    data.materials.append(mat)
    return obj


def box(name, loc, size, mat, bevel=.015):
    """Direct mesh primitives avoid thousands of depsgraph/operator updates."""
    sx,sy,sz=[s*.5 for s in size]
    vertices=[(-sx,-sy,-sz),(sx,-sy,-sz),(sx,sy,-sz),(-sx,sy,-sz),
              (-sx,-sy,sz),(sx,-sy,sz),(sx,sy,sz),(-sx,sy,sz)]
    faces=[(3,2,1,0),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)]
    obj=mesh(name,vertices,faces,mat)
    obj.location=loc
    if bevel:
        bm=bmesh.new();bm.from_mesh(obj.data)
        bmesh.ops.bevel(bm,geom=list(bm.edges),offset=min(bevel,min(size)*.45),segments=1,affect='EDGES')
        bm.to_mesh(obj.data);bm.free();obj.data.update()
    uv_box(obj)
    return obj


def cylinder(name, a, b, radius, mat, vertices=20):
    return tapered(name,a,b,radius,radius,mat,vertices)


def tapered(name, a, b, radius_a, radius_b, mat, sides=9):
    a, b = Vector(a), Vector(b)
    axis = (b-a).normalized()
    u = axis.cross(Vector((0, 1, 0))).normalized()
    if u.length < .1:
        u = axis.cross(Vector((1, 0, 0))).normalized()
    v = axis.cross(u)
    vertices = [p + radius * (math.cos(i*math.tau/sides)*u + math.sin(i*math.tau/sides)*v)
                for p, radius in [(a, radius_a), (b, radius_b)] for i in range(sides)]
    faces = [(i, (i+1)%sides, (i+1)%sides+sides, i+sides) for i in range(sides)]
    faces.extend([tuple(reversed(range(sides))), tuple(range(sides, 2*sides))])
    obj = mesh(name, vertices, faces, mat)
    for face in obj.data.polygons:
        face.use_smooth = len(face.vertices)==4
    uv_box(obj)
    return obj


def cable(name, points, radius, mat):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 1
    curve.bevel_depth = radius
    curve.bevel_resolution = 1
    spline = curve.splines.new('POLY')
    spline.points.add(len(points)-1)
    for p, co in zip(spline.points, points):
        p.co = (*co, 1)
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.convert(target='MESH')
    return bpy.context.object


def curtain(x, y, z, width, height, mat, seed):
    r = random.Random(seed)
    vertices, faces = [], []
    folds = 18
    for i in range(folds+1):
        u = i/folds
        yy = y + .025*math.sin(u*math.pi*10)
        hem = .012*math.sin(u*math.pi*6)
        vertices.extend([(x-width/2+u*width, yy, z-height/2+hem),
                         (x-width/2+u*width, yy, z+height/2-r.uniform(0, .009))])
    for i in range(folds):
        faces.append((i*2, i*2+1, i*2+3, i*2+2))
        faces.append((i*2+2, i*2+3, i*2+1, i*2))
    return mesh('Folded_curtain', vertices, faces, mat)


def window(m, x, y, z, seed, width=1.82, height=1.92, angle=0):
    """Framed outward-facing -Y window; optional rotation about its centre."""
    start = set(bpy.data.objects)
    r = random.Random(seed)
    w, h = width, height
    box('Window_shadow',(0,.015,0),(w+.12,.07,h+.13),m['dark'],.005)
    lit = r.random() < .19
    box('Window_glass',(0,-.035,0),(w-.09,.025,h-.09),m['glass'],.001)
    # Visible curtains sit just inside the external sash. This also gives opaque
    # runtime glass dimensional detail without requiring translucent materials.
    mode = seed % 7
    cloth = m['warm'] if lit else m['cream']
    if mode in (0, 1, 4):
        curtain(-w*.29,-.057,0,w*.32,h-.12,cloth,seed)
        curtain(w*.31,-.057,0,w*(.24 if mode==1 else .30),h-.12,cloth,seed+1)
        box('Curtain_dark_gap',(0,-.054,0),(w*.18,.008,h-.15),m['dark'],0)
    elif mode == 2:
        # Partially lowered metal blind: real slats cast fine alternating shadows.
        for i in range(9):
            slat=box('Venetian_blind',(0,-.065,h*.43-i*.087),(w-.12,.035,.065),m['cream'],.002)
            slat.rotation_euler.x=.16
    elif lit:
        box('Room_warm_panel',(w*.26,-.053,0),(w*.32,.008,h-.13),m['warm'],0)
        box('Room_shelf',(w*.22,-.075,-h*.23),(w*.43,.08,.035),m['wood'],.002)
    for dx in [-w/2, .07 if mode==4 else 0, w/2]:
        box('Window_vertical',(dx,-.105,0),(.045,.075,h+.07),m['aluminum'],.003)
    for dz in [-h/2,h*.22,h/2]:
        box('Window_horizontal',(0,-.105,dz),(w+.045,.075,.045),m['aluminum'],.003)
    box('Window_sill',(0,-.13,-h/2-.065),(w+.24,.29,.09),m['concrete'],.009)
    if mode in (3, 5):
        for i in range(8):
            xx = -w*.47+i*w*.94/7
            cylinder('Security_vertical',(xx,-.23,-h*.47),(xx,-.23,h*.47),.010,m['metal'],6)
        for zz in [-h*.47,0,h*.47]:
            cylinder('Security_crossbar',(-w*.49,-.23,zz),(w*.49,-.23,zz),.014,m['metal'],8)
        for side in [-1,1]:
            cylinder('Security_return',(side*w*.49,-.1,0),(side*w*.49,-.23,0),.014,m['metal'],8)
    if mode == 6:
        for side in [-1,1]:
            sx = side*(w*.5+.22)
            box('Shutter_back',(sx,-.085,0),(.37,.06,h),m['green'],.006)
            for i in range(12):
                slat=box('Shutter_louver',(sx,-.13,-h*.43+i*h*.078),(.34,.045,.095),m['metal'],.002)
                slat.rotation_euler.x=.23
    # Local window components are transformed together for side/back elevations.
    rot = Matrix.Rotation(angle,4,'Z')
    trans = Matrix.Translation((x,y,z))
    for obj in set(bpy.data.objects)-start:
        obj.matrix_world = trans @ rot @ obj.matrix_basis


def crate(m, loc=(0,0,0), name='Crate', color='green'):
    x,y,z=loc
    box(name+'_bottom',(x,y,z+.025),(.55,.38,.05),m[color],.008)
    for zz in [.075,.16,.245]:
        for yy in [-.185,.185]:box(name+'_slat',(x,y+yy,z+zz),(.55,.025,.048),m[color],.003)
        for xx in [-.26,.26]:box(name+'_end',(x+xx,y,z+zz),(.028,.36,.048),m[color],.003)
    for xx in [-.25,.25]:
        for yy in [-.17,.17]:box(name+'_corner',(x+xx,y+yy,z+.15),(.03,.03,.28),m[color],.002)


def tree(m, seed=31):
    r=random.Random(seed)
    box('Planter',(0,0,.29),(.91,.91,.58),m['concrete'],.055)
    box('Planter_rim',(0,0,.57),(.96,.96,.08),m['concrete'],.018)
    box('Planter_soil',(0,0,.615),(.79,.79,.025),m['dark'],.004)
    tapered('Trunk_root',(0,0,.6),(.04,.025,1.65),.105,.065,m['wood'],11)
    tapered('Trunk_upper',(.04,.025,1.65),(-.09,.03,2.85),.065,.026,m['wood'],9)
    clusters=[]
    for i in range(15):
        a=i*2.39996+r.uniform(-.2,.2)
        radius=r.uniform(.55,1.05)
        z=r.uniform(2.35,3.2)
        base=Vector((.02,.02,1.55+i*.055))
        end=Vector((math.cos(a)*radius, math.sin(a)*radius,z))
        elbow=base.lerp(end,.58)+Vector((0,0,.10))
        tapered('Main_branch',base,elbow,.037,.019,m['wood'])
        tapered('Branch_tip',elbow,end,.019,.005,m['wood'],7)
        for j in range(3):
            tip=end+Vector((r.uniform(-.27,.27),r.uniform(-.27,.27),r.uniform(-.02,.35)))
            tapered('Twig',end.lerp(elbow,.35),tip,.009,.002,m['wood'],5)
            clusters.append(tip)
    clusters.extend([Vector((0,0,3.2)),Vector((.2,.1,3.35))])
    vertices,faces=[],[]
    for cluster in clusters:
        for _ in range(76):
            # Elliptical, ridged leaves in overlapping irregular clusters.
            center=cluster+Vector((r.gauss(0,.24),r.gauss(0,.24),r.gauss(0,.18)))
            a=r.uniform(0,math.tau)
            length=r.uniform(.10,.175)
            u=Vector((math.cos(a),math.sin(a),r.uniform(-.75,.75))).normalized()*length
            v=u.cross(Vector((0,0,1))).normalized()*length*r.uniform(.34,.48)
            ridge=Vector((0,0,.013))
            n=len(vertices)
            vertices.extend([center-u,center-u*.5+v*.75,center+u*.25+v,
                             center+u,center+u*.25-v,center-u*.5-v*.75,center+ridge])
            for i in range(6):
                faces.append((n+i,n+(i+1)%6,n+6))
                faces.append((n+6,n+(i+1)%6,n+i))
    foliage=mesh('Camphor_broad_leaves',vertices,faces,m['leaf'])
    # Flat normals preserve the centre ridge; coincident reverse faces must not
    # share averaged smooth normals or their normals would cancel.
    for face in foliage.data.polygons:face.use_smooth=False


def canopy(m, variant):
    # Pitched fabric panels sag slightly between bows instead of a solid slab.
    vertices,faces=[],[]
    for i in range(17):
        x=-1.26+i*2.52/16
        for j in range(5):
            y=-.85+j*.425
            z=2.29+.23*(1-abs(y)/.85)-.025*math.sin(i*math.pi/4)**2
            vertices.append((x,y,z))
    for i in range(16):
        for j in range(4):
            n=i*5+j
            faces.extend([(n,n+5,n+6,n+1),(n+1,n+6,n+5,n)])
    mesh('Fabric_canopy',vertices,faces,m[['red','green','cream'][variant]])
    for x in [-1.25,0,1.25]:
        cable('Canopy_bow',[(x,-.84,2.28),(x,0,2.52),(x,.84,2.28)],.018,m['metal'])
    for i in range(12):
        x=-1.15+i*.21
        box('Canvas_valance',(x,-.846,2.21),(.212,.016,.16),m[['red','green','cream'][variant]],.002)


def stall(m, variant=0):
    for x in [-1,1]:
        for y in [-.55,.55]:
            cylinder('Stall_post',(x,y,.16),(x,y,2.31),.022,m['metal'],12)
            cylinder('Caster',(x-.035,y,.10),(x+.035,y,.10),.075,m['rubber'],16)
    box('Market_counter',(0,0,.92),(2.15,1.2,.075),m['aluminum'],.018)
    box('Counter_front',(0,-.56,.54),(2.05,.06,.69),m[['green','metal','wood'][variant]],.008)
    box('Lower_shelf',(0,.08,.22),(1.96,.92,.045),m['aluminum'],.008)
    for x in [-.98,.98]:box('Counter_edge',(x,-.603,.54),(.035,.022,.70),m['aluminum'],.003)
    canopy(m,variant)
    titles=['邻里小厨','时令鲜果','老街汤粉']
    text_mesh('Stall_name',titles[variant],(0,-.87,2.16),.18,m['white'] if variant!=2 else m['red'])
    box('Menu_board',(-.78,-.64,1.40),(.34,.035,.64),m['dark'],.008)
    for i in range(5):
        box('Menu_chalk',(-.79,-.664,1.61-i*.095),(.22-.02*(i%2),.004,.012),m['cream'],0)
    if variant==1:
        r=random.Random(43)
        for i,x in enumerate([-.57,.05,.66]):
            crate(m,(x,0,.97),color='wood')
            for j in range(13):
                px=x+r.uniform(-.20,.20);py=r.uniform(-.12,.12)
                sphere('Fresh_produce',(px,py,1.25+r.uniform(-.01,.07)),(.065,.064,.061),m['yellow'] if i!=1 else m['red'])
        crate(m,(.48,.1,.25),color='green')
        crate(m,(-.2,.1,.25),color='wood')
    else:
        for i,x in enumerate([-.28,.35,.81] if variant==0 else [-.31,.36]):
            radius=.205 if variant==0 else .25
            cylinder('Cooking_pot',(x,.15,.97),(x,.15,1.19),radius,m['aluminum'],24)
            cylinder('Pot_dark_opening',(x,.15,1.192),(x,.15,1.196),radius*.91,m['dark'],24)
            torus('Pot_rim',(x,.15,1.20),radius,.012,m['aluminum'],rotation=(0,0,0))
            for side in [-1,1]:
                box('Pot_handle',(x+side*(radius+.035),.15,1.14),(.075,.06,.027),m['metal'],.006)
            if i==0:
                cylinder('Ladle',(x+.03,.12,1.14),(x+.13,.18,1.44),.009,m['wood'],8)
        for x in [.31,.5,.69]:
            cylinder('Condiment_bottle',(x,-.36,.97),(x,-.36,1.12),.034,m['red'] if x==.5 else m['cream'],12)
            cylinder('Bottle_cap',(x,-.36,1.12),(x,-.36,1.15),.024,m['metal'],12)
        crate(m,(.40,.12,.25),color='green')
        cylinder('Water_bucket',(-.55,.12,.24),(-.55,.12,.63),.17,m['cream'],20)
    box('Counter_towel',(.03,-.47,.969),(.35,.20,.012),m['cream'],.003)
    cylinder('Task_lamp',(-.75,.45,2.10),(.75,.45,2.10),.022,m['warm'],12)


def dressing(m, kind):
    if kind=='DrainGrate':
        box('Drain_recess',(0,0,.015),(.72,.39,.03),m['dark'],.003)
        for x in [-.35,.35]:box('Drain_edge',(x,0,.035),(.035,.4,.035),m['metal'],.003)
        for y in [-.18,.18]:box('Drain_edge',(0,y,.035),(.7,.035,.035),m['metal'],.003)
        for i in range(13):box('Drain_slot',(-.30+i*.05,0,.035),(.018,.34,.03),m['metal'],.002)
    elif kind=='Manhole':
        cylinder('Manhole_base',(0,0,0),(0,0,.018),.33,m['dark'],48)
        cylinder('Manhole_lid',(0,0,.018),(0,0,.028),.305,m['metal'],48)
        torus('Manhole_ring',(0,0,.03),.32,.012,m['metal'],rotation=(0,0,0))
        for i in range(-4,5):
            y=i*.055;span=math.sqrt(.27**2-y*y)
            box('Cast_tread',(0,y,.035),(span*2,.014,.009),m['aluminum'],.001)
        for x in [-.22,.22]:box('Lid_lift_hole',(x,0,.037),(.055,.035,.006),m['dark'],.002)
    elif kind=='WheelieBin':
        tapered('Bin_body',(0,0,.13),(0,0,.83),.25,.31,m['green'],4).rotation_euler.z=math.pi/4
        box('Bin_lid',(0,0,.85),(.57,.57,.065),m['metal'],.026)
        box('Bin_handle',(0,.30,.80),(.36,.045,.045),m['metal'],.008)
        for x in [-.23,.23]:cylinder('Bin_wheel',(x-.03,.17,.11),(x+.03,.17,.11),.105,m['rubber'],20)
        box('Bin_label',(0,-.236,.55),(.16,.012,.18),m['cream'],.002)
    elif kind=='CrateStack':
        for i in range(3):crate(m,((i%2)*.035,0,i*.285),color='wood' if i==1 else 'green')
    elif kind=='TrafficCone':
        box('Cone_base',(0,0,.035),(.39,.39,.07),m['rubber'],.018)
        tapered('Cone_lower',(0,0,.07),(0,0,.26),.145,.095,m['red'],24)
        tapered('Reflective_band',(0,0,.26),(0,0,.39),.095,.060,m['cream'],24)
        tapered('Cone_tip',(0,0,.39),(0,0,.59),.060,.015,m['red'],24)
