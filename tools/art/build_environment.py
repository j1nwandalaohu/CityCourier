"""Author reusable south-China shop houses and street fixtures with real dimensions."""
import math
import random
import bpy
from art_common import *
from environment_detail import window, cable, tree, stall, dressing, tapered, box, cylinder

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
    # The map stretches building depth 2x; shallow authoring gives a 1.4 m canopy.
    awning=box('Awning',(0,front-.40,2.86),(7.8,.72,.055),colors[variant],.008)
    awning.rotation_euler.x=.055
    for x in [-3.4,3.4]:cylinder('Awning_brace',(x,front-.1,2.45),(x,front-.75,2.84),.018,m['metal'])
    for i in range(27):
        box('Awning_seam',(-3.78+i*.29,front-.4,2.9),(.014,.70,.012),m['metal'],.002)
    # Tile joints, roller-shutter tracks and a worn entrance establish human scale.
    for x in [-3.88,0,3.88]:
        for z in range(1,11):box('Pier_tile_joint',(x,front-.339,z*.27),(.215,.007,.012),m['concrete'],0)
    box('Entrance_step',(0,front-.24,.055),(7.58,.40,.11),m['concrete'],.013)
    for side in [-1,1]:
        x=side*1.96
        for zz in [2.72,2.66,2.60]:box('Shutter_slats',(x,front-.24,zz),(3.48,.055,.045),m['aluminum'],.003)
        box('Shop_door_sticker',(x+.38,front-.245,1.48),(.14,.008,.21),m['cream'],.001)
    box('Electric_meter_box',(-3.54,front-.19,1.75),(.28,.18,.43),m['metal'],.012)
    box('Meter_face',(-3.54,front-.289,1.8),(.15,.015,.16),m['dark'],.002)
    cable('Meter_conduit',[(-3.54,front-.19,1.98),(-3.54,front-.19,3.86),(-3.15,front-.19,3.86)],.014,m['cream'])
    for j in range(3):
        cable('Facade_service_cable',[(x,front-.14,3.91+j*.055-.10*math.sin((x+3.9)/7.8*math.pi)) for x in [-3.9+i*.39 for i in range(21)]],.010,m['rubber'])
    floors=round((height-4.1)/3.3)
    for floor in range(floors):
        z=4.25+floor*3.3
        if floor==0 or (floor+variant)%2==0:
            box('Floor_band',(0,front-.04,z-.22),(8.06,.12,.085),m['concrete'],.006)
        for col,x in enumerate([-2.65,0,2.65]):
            window(m,x,front-.04,z+1.05,variant*101+floor*11+col*3,
                   width=[1.73,1.82,1.63][(col+variant)%3],height=1.92 if variant!=1 else 1.82)
            if (floor+col+variant)%3==0:
                box('Balcony_slab',(x,front-.36,z-.02),(2.17,.72,.115),m['concrete'],.014)
                for dx in [-1.02,1.02]:
                    cylinder('Balcony_post',(x+dx,front-.68,z),(x+dx,front-.68,z+.95),.019,m['metal'])
                    cylinder('Balcony_return',(x+dx,front-.1,z+.95),(x+dx,front-.68,z+.95),.021,m['metal'])
                for j in range(10):cylinder('Balcony_rail',(x-1+j*.22,front-.68,z),(x-1+j*.22,front-.68,z+.9),.010,m['metal'],8)
                cylinder('Balcony_top',(x-1.06,front-.68,z+.95),(x+1.06,front-.68,z+.95),.024,m['metal'])
                if (floor+variant)%2==0:
                    box('Balcony_planter',(x-.62,front-.43,z+.18),(.48,.23,.33),m['brick'],.018)
                    for j in range(5):
                        tapered('Planter_leaf',(x-.62,front-.43,z+.32),(x-.79+j*.08,front-.48,z+.57+(j%2)*.10),.045,.002,m['leaf'],5)
                else:
                    cable('Clothes_line',[(x-.93,front-.46,z+1.68),(x,front-.46,z+1.62),(x+.93,front-.46,z+1.68)],.005,m['rubber'])
                    for j in range(2):
                        box('Drying_laundry',(x-.45+j*.55,front-.46,z+1.31),(.32,.018,.60-j*.15),m['cream'] if j else m['metal'],.008)
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
    torus('Tank_top_band',(2,2,height+1.42),.63,.018,m['aluminum'],rotation=(0,0,0))
    tapered('Tank_lid',(2,2,height+1.5),(2,2,height+1.66),.62,.10,m['aluminum'],32)
    for x in [1.65,2.35]:box('Tank_foot',(x,2,height+.15),(.09,.75,.3),m['metal'],.008)
    cable('Tank_supply',[(2.62,2,height+.44),(3.45,2,height+.44),(3.45,4.5,height+.44),(3.45,4.5,height-1.5)],.035,m['cream'])
    box('Roof_stairwell',(-1.75,2.7,height+.85),(2.35,3.1,1.7),wall,.015)
    box('Stairwell_roof',(-1.75,2.7,height+1.74),(2.55,3.3,.14),m['concrete'],.015)
    box('Roof_access_door',(-1.75,1.135,height+.72),(.72,.04,1.43),m['metal'],.005)
    cylinder('Aerial_mast',(-2.6,3.3,height+1.75),(-2.6,3.3,height+3.10),.018,m['metal'],10)
    for i in range(5):
        yy=2.95+i*.18
        cylinder('Aerial_elements',(-3.05+i*.045,yy,height+2.86),(-2.15-i*.045,yy,height+2.86),.009,m['aluminum'],8)
    # Complete side and rear elevations, including lintels and utility doors.
    for side in [-1,1]:
        box('Side_tile_skirt',(side*4.025,0,.38),(.055,11.4,.66),m['concrete'],.008)
        for floor in range(floors):
            z=5.3+floor*3.3
            for col,y in enumerate([-3.1,.4,3.9]):
                window(m,side*4.03,y,z,200+variant*41+floor*7+col,width=1.4,height=1.70,
                       angle=side*math.pi/2)
        box('Service_door_frame',(side*4.05,3.45,1.14),(.09,1.11,2.28),m['concrete'],.012)
        box('Service_door',(side*4.105,3.45,1.11),(.045,.93,2.16),m['metal'],.006)
        for i in range(6):box('Door_vent',(side*4.135,3.45,.45+i*.07),(.012,.66,.025),m['dark'],0)
        cylinder('Side_rainpipe',(side*4.09,5.15,.25),(side*4.09,5.15,height),.045,m['cream'],10)
    for floor in range(floors):
        for col,x in enumerate([-2.5,.1,2.55]):
            window(m,x,5.765,5.3+floor*3.3,300+variant*31+floor*5+col,width=1.55,height=1.76,angle=math.pi)
    box('Rear_skirt',(0,5.79,.35),(8,.07,.6),m['concrete'],.008)
    box('Rear_door_surround',(-1.9,5.82,1.18),(1.2,.10,2.36),m['concrete'],.01)
    box('Rear_service_door',(-1.9,5.88,1.12),(1.02,.04,2.21),m['metal'],.006)
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
    for seed, name in [(31, 'SM_PlanterTree'), (67, 'SM_PlanterTreeB')]:
        reset();start=set(bpy.data.objects)
        tree(m,seed)
        o=join(list(set(bpy.data.objects)-start),name);export(o.name,[o],'StreetProps')
    for variant, name in enumerate(['SM_MarketStall','SM_MarketStallProduce','SM_MarketStallNoodles']):
        reset();start=set(bpy.data.objects)
        stall(m,variant)
        o=join(list(set(bpy.data.objects)-start),name);export(o.name,[o],'StreetProps')
    for kind in ['DrainGrate','Manhole','WheelieBin','CrateStack','TrafficCone']:
        reset();start=set(bpy.data.objects)
        dressing(m,kind)
        o=join(list(set(bpy.data.objects)-start),'SM_'+kind);export(o.name,[o],'StreetProps')
    save('StreetProps')

def build():
    m=palette()
    for variant in range(3):
        reset();o=building(m,variant);export(o.name,[o],'Architecture');save(o.name)
    props(m)

if __name__=='__main__':build()
