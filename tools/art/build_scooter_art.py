"""Original delivery scooter: tubular frame, shaped fairings, wheels and insulated box."""
import bpy
import math
from art_common import *

def box(name,loc,size,mat,bevel=.015):
    """Small manufactured parts need rounded highlights, including their silhouette."""
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object;o.scale=size
    finish(o,name,mat,smooth=True)
    if bevel:
        mod=o.modifiers.new('Rounded manufactured edges','BEVEL');mod.width=bevel;mod.segments=5
        bpy.ops.object.modifier_apply(modifier=mod.name)
    mod=o.modifiers.new('Weighted panel normals','WEIGHTED_NORMAL');mod.keep_sharp=True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return o

def build():
    reset()
    paint=material('M_ScooterPaint',(.018,.11,.13),.32,0)
    gold=material('M_CourierGold',(.95,.48,.045),.5)
    steel=material('M_Steel',(.20,.22,.23),.24,.85)
    black=material('M_Rubber',(.025,.027,.025),.9)
    seat=material('M_Seat',(.045,.048,.046),.72)
    light=material('M_Headlight',(.88,.89,.75),.17,emission=2)
    red=material('M_Taillight',(.55,.008,.004),.22,emission=.8)
    silver=material('M_ReflectiveStrip',(.70,.73,.65),.33,.4)
    # Wheels are separate origin-centered assets so runtime motion is visible.
    torus('Tire',(0,0,0),.205,.062,black,(0,math.pi/2,0))
    cylinder('Rim',(-.045,0,0),(.045,0,0),.16,steel,40)
    cylinder('Hub',(-.08,0,0),(.08,0,0),.065,black,24)
    for side in [-1,1]:
        for i in range(10):
            a=i*math.tau/10
            cylinder('Spoke',(side*.052,.06*math.sin(a),.06*math.cos(a)),
                     (side*.052,.15*math.sin(a+.1),.15*math.cos(a+.1)),.009,steel,8)
    for i in range(48):
        a=i*math.tau/48
        o=box('Tire_tread',(0,.265*math.sin(a),.265*math.cos(a)),(.09,.014,.004),black,.001)
        o.rotation_euler.x=-a
    wheel=join(list(bpy.data.objects),'SM_ScooterWheel');export(wheel.name,[wheel],'Scooter');save('ScooterWheel')
    reset()
    # Chassis and low step-through deck.
    for x in [-.18,.18]:
        cylinder('Chassis',(x,-.5,.24),(x,.58,.35),.035,steel)
        cylinder('Rear_support',(x,.15,.3),(x,.4,.72),.035,steel)
        cylinder('Fork',(x,-.63,.26),(x,-.44,.93),.028,steel)
    box('Battery_enclosure',(0,.1,.36),(.39,.65,.28),paint,.075)
    for side in [-1,1]:
        box('Battery_access_panel',(side*.197,.10,.39),(.008,.37,.14),black,.018)
        for y in [-.05,.25]:
            cylinder('Panel_screw',(side*.20,y,.39),(side*.207,y,.39),.005,steel,12)
    box('Foot_deck',(0,-.17,.27),(.52,.62,.07),black,.035)
    for i in range(8):box('Deck_grip',(0,-.43+i*.07,.31),(.43,.018,.008),steel,.003)
    sphere('Rear_fairing',(0,.38,.57),(.27,.45,.19),paint)
    box('Saddle',(0,.18,.79),(.49,.69,.12),seat,.055)
    box('Saddle_seam',(0,.18,.784),(.505,.69,.018),black,.008)
    sphere('Front_shield',(0,-.49,.64),(.26,.11,.35),paint)
    cylinder('Steering_column',(0,-.43,.7),(0,-.47,1.05),.035,steel)
    cylinder('Handlebar',(-.38,-.49,1.08),(.38,-.49,1.08),.021,steel)
    for side in [-1,1]:
        cylinder('Rubber_grip',(side*.25,-.49,1.08),(side*.4,-.49,1.08),.029,black)
        cylinder('Brake_lever',(side*.26,-.55,1.08),(side*.39,-.57,1.08),.009,steel,12)
        cylinder('Mirror_stem',(side*.25,-.48,1.08),(side*.36,-.52,1.34),.009,steel,12)
        sphere('Mirror_housing',(side*.37,-.52,1.35),(.085,.028,.06),black)
        sphere('Mirror_glass',(side*.37,-.487,1.35),(.073,.006,.048),silver)
        cylinder('Suspension',(side*.2,.26,.64),(side*.2,.61,.28),.025,steel)
        for j in range(9):
            t=j/10;o=torus('Rear_spring',(side*.2,.28+t*.28,.61-t*.29),.035,.008,black,(.77,0,0))
    sphere('Headlamp_housing',(0,-.52,1.0),(.17,.09,.10),paint)
    sphere('Headlamp_lens',(0,-.602,1.0),(.135,.018,.065),light)
    box('Instrument_panel',(0,-.46,1.09),(.15,.13,.035),black,.012)
    box('Instrument_glass',(0,-.46,1.111),(.11,.09,.008),silver,.006)
    for side in [-1,1]:
        cylinder('Brake_cable',(side*.24,-.49,1.06),(side*.12,-.45,.81),.004,black,8)
        sphere('Amber_indicator',(side*.205,-.57,.92),(.04,.018,.022),gold)
    # Fender arcs follow the wheel instead of primitive slabs.
    for cy in [-.63,.62]:
        verts=[];faces=[]
        for i in range(25):
            a=-1.0+i*2.0/24
            for x in [-.09,.09]:verts.append((x,cy+.30*math.sin(a),.265+.30*math.cos(a)))
        for i in range(24):faces.append((i*2,i*2+1,i*2+3,i*2+2))
        mesh=bpy.data.meshes.new('Fender');mesh.from_pydata(verts,[],faces);mesh.update()
        o=bpy.data.objects.new('Fender',mesh);bpy.context.collection.objects.link(o);o.data.materials.append(paint)
        mod=o.modifiers.new('Fender thickness','SOLIDIFY');mod.thickness=.012
        bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
    # Rear rack and insulated delivery box with hinges, lid seam and reflectors.
    for x in [-.24,.24]:cylinder('Rack',(x,.35,.79),(x,.88,.79),.017,steel)
    box('Insulated_box',(0,.64,1.035),(.56,.49,.49),gold,.04)
    box('Box_lid',(0,.64,1.30),(.59,.52,.065),gold,.025)
    box('Lid_gasket',(0,.64,1.266),(.57,.5,.012),black,.003)
    for x in [-.2,.2]:box('Lid_latch',(x,.387,1.23),(.045,.018,.08),steel,.008)
    for side in [-1,1]:
        box('Box_reflector',(side*.284,.64,.97),(.008,.4,.055),silver,.002)
        box('Carry_handle',(side*.298,.64,1.17),(.025,.18,.045),black,.01)
    text_mesh('Delivery_brand','南城速达',(0,.894,1.04),.10,black,(math.pi/2,0,math.pi))
    box('Rear_light',(0,.88,.62),(.22,.045,.055),red,.015)
    box('Number_plate',(0,.88,.5),(.21,.018,.12),silver,.005)
    text_mesh('Plate_registration','南城 · 0628',(0,.891,.48),.025,black,(math.pi/2,0,math.pi))
    for x in [-.08,.08]:cylinder('Plate_screw',(x,.89,.54),(x,.897,.54),.004,steel,12)
    cylinder('Kickstand',(-.2,.08,.32),(-.3,.14,.04),.015,steel)
    body=join(list(bpy.data.objects),'SM_ScooterBody');export(body.name,[body],'Scooter');save('DeliveryScooter')

if __name__=='__main__':build()
