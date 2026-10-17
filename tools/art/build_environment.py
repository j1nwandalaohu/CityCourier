"""Author reusable south-China shop houses and street fixtures with real dimensions."""
import math
import random
import bpy
from art_common import *

def palette():
    return dict(
        plaster=material('M_Plaster',(.63,.60,.51),scan='painted_plaster_wall'),
        concrete=material('M_Concrete',(.49,.48,.44),scan='concrete_wall_008'),
        brick=material('M_Brick',(.45,.21,.12),scan='red_brick_03'),
        asphalt=material('M_Asphalt',(.09,.09,.085),scan='asphalt_02'),
        metal=material('M_PaintedMetal',(.075,.11,.11),.38,.65),
        aluminum=material('M_Aluminum',(.45,.48,.46),.3,.82),
        glass=material('M_WindowGlass',(.06,.105,.12),.18,.45),
        warm=material('M_WindowWarm',(.64,.38,.16),.6,emission=.65),
        dark=material('M_Recess',(.025,.03,.029),.92),
        cream=material('M_CreamPaint',(.72,.69,.58),.75),
        red=material('M_SignRed',(.24,.035,.025),.55),
        green=material('M_SignGreen',(.035,.16,.12),.6),
        yellow=material('M_SignGold',(.83,.65,.32),.45,emission=.2),
        white=material('M_LetterWhite',(.85,.81,.68),.5,emission=.4),
        leaf=material('M_Leaf',(.055,.12,.035),.85),
        wood=material('M_Wood',(.18,.10,.05),.8),
        rubber=material('M_Rubber',(.025,.027,.025),.95))

def aircon(m):
    start=set(bpy.data.objects)
    box('AC_case',(0,0,0),(.83,.32,.55),m['cream'],.035)
    cylinder('AC_fan_shadow',(.12,-.165,0),(.12,-.185,0),.215,m['dark'],32)
    for i in range(10):
        x=-.1+i*.045
        cylinder('Fan_grille',(x,-.195,-.19),(x,-.195,.19),.003,m['aluminum'],8)
    for i in range(9):
        z=-.18+i*.045
        cylinder('Fan_grille',(-.1,-.195,z),(.34,-.195,z),.003,m['aluminum'],8)
    for i in range(8):box('Side_vent',(-.3,-.17,-.18+i*.05),(.13,.01,.012),m['dark'],0)
    for side in [-1,1]:box('AC_bracket',(side*.28,.04,-.3),(.035,.55,.035),m['metal'],.005)
    return join(list(set(bpy.data.objects)-start),'SM_AirConditioner')

def building(m,variant):
    start=set(bpy.data.objects);r=random.Random(107+variant)
    width=8;depth=11.5;height=[17.7,14.4,20.9][variant]
    wall=m['plaster'] if variant!=1 else m['brick']
    # Structural volume is inset behind the individually framed front openings.
    box('Building_core',(0,.24,height/2),(width,depth-.48,height),wall,.025)
    box('Ground_plinth',(0,0,.2),(8.1,11.6,.4),m['concrete'],.025)
    front=-5.76
    # Ground floor is a shallow occupied shop, with a dark recessed interior.
    box('Shop_recess',(0,front-.05,1.6),(7.6,.12,3.05),m['dark'],.01)
    for x in [-3.88,0,3.88]:box('Tiled_pier',(x,front-.16,1.65),(.22,.35,3.3),m['cream'],.015)
    for side in [-1,1]:
        cx=side*1.96
        for x in [cx-1.73,cx,cx+1.73]:box('Shop_frame',(x,front-.19,1.4),(.06,.07,2.8),m['aluminum'],.005)
        box('Shop_glazing',(cx,front-.13,1.42),(3.42,.04,2.62),m['glass'],.002)
        box('Shop_lower_panel',(cx,front-.21,.28),(3.42,.08,.5),m['metal'],.008)
        box('Door_header',(cx,front-.2,2.78),(3.5,.08,.08),m['aluminum'],.005)
        cylinder('Door_handle',(cx+.13,front-.28,1.1),(cx+.13,front-.28,1.55),.014,m['aluminum'])
        # Interior hints visible as warmer strips; no fake navigable interiors.
        box('Interior_light',(cx,front-.175,2.5),(2.6,.018,.07),m['warm'],.002)
    colors=[m['red'],m['green'],m['metal']]
    box('Lightbox',(0,front-.26,3.3),(7.78,.35,.85),colors[variant],.035)
    title=['阿荣面馆','南城便利','巷口烧腊'][variant]
    text_mesh('Shop_name',title,(0,front-.445,3.08),.60,m['white'])
    box('Awning',(0,front-.78,2.86),(7.8,1.45,.09),colors[variant],.015)
    for x in [-3.4,3.4]:cylinder('Awning_brace',(x,front-.1,2.35),(x,front-1.45,2.86),.025,m['metal'])
    floors=round((height-4.1)/3.3)
    for floor in range(floors):
        z=4.25+floor*3.3
        box('Floor_band',(0,front-.04,z-.22),(8.1,.16,.12),m['concrete'],.008)
        for col,x in enumerate([-2.65,0,2.65]):
            box('Window_recess',(x,front-.04,z+1.05),(1.85,.1,1.95),m['dark'],.008)
            glass=m['warm'] if r.random()<.18 else m['glass']
            box('Window_pane',(x,front-.11,z+1.05),(1.68,.035,1.78),glass,.002)
            for dx in [-.88,0,.88]:box('Window_mullion',(x+dx,front-.16,z+1.05),(.045,.06,1.94),m['aluminum'],.003)
            for dz in [.12,1.04,1.98]:box('Window_transom',(x,front-.16,z+dz),(1.82,.06,.045),m['aluminum'],.003)
            box('Window_sill',(x,front-.28,z+.05),(2.0,.5,.10),m['concrete'],.015)
            if (floor+col+variant)%3==0:
                box('Balcony_slab',(x,front-.65,z-.02),(2.2,1.3,.14),m['concrete'],.02)
                for dx in [-1.02,1.02]:cylinder('Balcony_post',(x+dx,front-1.22,z),(x+dx,front-1.22,z+.95),.022,m['metal'])
                for j in range(10):cylinder('Balcony_rail',(x-1+j*.22,front-1.22,z),(x-1+j*.22,front-1.22,z+.9),.012,m['metal'],8)
                cylinder('Balcony_top',(x-1.08,front-1.22,z+.95),(x+1.08,front-1.22,z+.95),.028,m['metal'])
            if (floor+col)%2==0:
                ac=aircon(m);ac.location=(x+.72,front-.30,z-.62)
                cylinder('AC_drain',(x+1.07,front-.32,z-.9),(x+1.07,front-.32,z-1.45),.018,m['cream'],10)
    for x in [-3.68,3.68]:
        cylinder('Rain_pipe',(x,front-.18,.25),(x,front-.18,height+.15),.055,m['cream'],12)
        for z in range(1,int(height),2):torus('Pipe_clamp',(x,front-.18,z),.061,.008,m['metal'],rotation=(0,0,0))
    box('Roof_coping',(0,0,height),(8.25,11.7,.22),m['concrete'],.025)
    for x in [-3.85,3.85]:box('Roof_parapet',(x,0,height+.35),(.18,11.5,.65),wall,.02)
    for y in [-5.6,5.6]:box('Roof_parapet',(0,y,height+.35),(7.8,.18,.65),wall,.02)
    cylinder('Water_tank',(2,2,height+.2),(2,2,height+1.5),.62,m['aluminum'],32)
    torus('Tank_band',(2,2,height+.65),.63,.025,m['metal'],rotation=(0,0,0))
    # Side windows and a service door prevent blank side faces at intersections.
    for side in [-1,1]:
        for z in [5.4,8.7,12]:
            if z+1>height:continue
            for y in [-2.8,1.1,4]:
                box('Side_window',(side*4.015,y,z),(.035,1.5,1.65),m['glass'],.002)
                for dy in [-.8,.8]:box('Side_frame',(side*4.055,y+dy,z),(.07,.06,1.8),m['aluminum'],.003)
    return join(list(set(bpy.data.objects)-start),'SM_ShopHouse'+chr(65+variant))

def props(m):
    reset();ac=aircon(m);export(ac.name,[ac],'StreetProps')
    reset();start=set(bpy.data.objects)
    cylinder('Pole',(0,0,.1),(0,0,6.3),.065,m['metal'])
    cylinder('Lamp_arm',(0,0,6.3),(0,-1.1,6.5),.045,m['metal'])
    box('Lamp_head',(0,-1.15,6.45),(.32,.75,.13),m['metal'],.05)
    box('Lamp_emitter',(0,-1.15,6.375),(.23,.6,.015),m['warm'],.005)
    box('Lamp_base',(0,0,.14),(.23,.23,.28),m['metal'],.04)
    o=join(list(set(bpy.data.objects)-start),'SM_StreetLamp');export(o.name,[o],'StreetProps')
    reset();start=set(bpy.data.objects)
    box('Planter',(0,0,.3),(.9,.9,.6),m['concrete'],.08)
    box('Soil',(0,0,.61),(.77,.77,.03),m['dark'],.01)
    cylinder('Trunk',(0,0,.6),(.06,0,2.4),.07,m['wood'])
    r=random.Random(31)
    leaves=[];leaf_faces=[]
    for i in range(22):
        x,y=r.uniform(-.75,.75),r.uniform(-.75,.75);z=2.1+r.uniform(-.3,.8)
        cylinder('Branch',(0,0,1.7),(x,y,z),.018,m['wood'],8)
        for k in range(65):
            center=Vector((x+r.gauss(0,.26),y+r.gauss(0,.26),z+r.gauss(0,.24)))
            a=r.uniform(0,math.tau);u=Vector((math.cos(a),math.sin(a),r.uniform(-.8,.8))).normalized()*r.uniform(.065,.12)
            v=u.cross(Vector((0,0,1))).normalized()*.032
            n=len(leaves)
            leaves.extend([center-u,center+v,center+Vector((0,0,.012)),center-v,center+u])
            leaf_faces.extend([(n,n+1,n+2),(n,n+2,n+3),(n+1,n+4,n+2),(n+2,n+4,n+3)])
    data=bpy.data.meshes.new('Camphor_leaves');data.from_pydata(leaves,[],leaf_faces);data.update()
    foliage=bpy.data.objects.new('Camphor_leaves',data);bpy.context.collection.objects.link(foliage);data.materials.append(m['leaf'])
    o=join(list(set(bpy.data.objects)-start),'SM_PlanterTree');export(o.name,[o],'StreetProps')
    reset();start=set(bpy.data.objects)
    for x in [-1,1]:
        for y in [-.55,.55]:cylinder('Stall_post',(x,y,0),(x,y,2.3),.025,m['metal'])
    box('Market_counter',(0,0,.9),(2.15,1.2,.1),m['aluminum'],.025)
    box('Counter_front',(0,-.56,.48),(2.05,.06,.85),m['green'],.01)
    box('Market_canopy',(0,0,2.3),(2.5,1.7,.12),m['red'],.04)
    text_mesh('Stall_name','邻里小厨',(0,-.86,2.08),.25,m['white'])
    for x in [-.55,.1,.65]:
        cylinder('Cooking_pot',(x,0,.96),(x,0,1.2),.23,m['aluminum'],24)
        torus('Pot_rim',(x,0,1.21),.22,.015,m['metal'],rotation=(0,0,0))
    o=join(list(set(bpy.data.objects)-start),'SM_MarketStall');export(o.name,[o],'StreetProps')
    save('StreetProps')

def build():
    m=palette()
    for variant in range(3):
        reset();o=building(m,variant);export(o.name,[o],'Architecture');save(o.name)
    props(m)

if __name__=='__main__':build()
