"""Open, shallow shop sets. Y depths compensate for the map's 2x building scale.

The sets use existing materials and only direct mesh primitives. Openings are
actually empty; opaque simulated glass must never cover the shop interiors.
"""
import math
import random
from environment_detail import box, cylinder, tapered, mesh
from art_common import text_mesh


def stool(m, x, y, color='red'):
    box('Stackable_stool_seat',(x,y,.84),(.33,.17,.045),m[color],.012)
    for dx in [-.12,.12]:
        for dy in [-.055,.055]:
            tapered('Stool_leg',(x+dx*1.2,y+dy*1.2,.41),(x+dx,y+dy,.82),.018,.013,m[color],6)
    box('Stool_footrest',(x,y-.06,.57),(.25,.016,.022),m[color],.002)


def table(m, x, y, width=.85):
    box('Cafe_tabletop',(x,y,1.16),(width,.39,.055),m['cream'],.013)
    for dx in [-width*.37,width*.37]:
        for dy in [-.14,.14]:
            cylinder('Cafe_table_leg',(x+dx,y+dy,.42),(x+dx,y+dy,1.13),.018,m['metal'],8)
    cylinder('Chopstick_holder',(x+.22,y,1.19),(x+.22,y,1.31),.035,m['red'],10)
    for i in range(4):
        cylinder('Chopsticks',(x+.20+i*.012,y,1.25),(x+.20+i*.012,y,1.43),.004,m['wood'],5)
    box('Napkin_box',(x-.17,y,1.22),(.16,.08,.09),m['cream'],.004)


def shelf(m, x, y, width, seed, levels=4):
    r=random.Random(seed)
    for dx in [-width*.49,width*.49]:
        box('Shop_shelf_upright',(x+dx,y,1.36),(.035,.20,1.88),m['metal'],.002)
    for level in range(levels):
        z=.55+level*.43
        box('Shop_shelf',(x,y,z),(width,.26,.035),m['cream'],.004)
        for i in range(max(3,int(width/.18))):
            xx=x-width*.43+i*width*.86/(max(3,int(width/.18))-1)
            h=r.uniform(.15,.28)
            mat=m[['cream','red','green','yellow'][r.randrange(4)]]
            box('Grocery_package',(xx,y-.012,z+h*.5+.018),(.105,.12,h),mat,.004)
            # Large printed label, not a noisy full-resolution texture per item.
            box('Package_label',(xx,y-.076,z+h*.53),(.07,.004,h*.32),m['white'],0)


def framed_opening(m, left, right, front, top=2.88):
    for x in [left,right]:
        box('Shop_jamb',(x,front+.025,1.64),(.045,.065,2.45),m['aluminum'],.003)
    box('Shop_transom',( .5*(left+right),front+.025,top),(right-left,.07,.055),m['aluminum'],.003)
    box('Door_track',(.5*(left+right),front+.025,.421),(right-left,.08,.025),m['aluminum'],.002)


def sign(m, x, front, width, title, color, subtitle=None, z=3.28):
    box('Shop_sign_case',(x,front-.13,z),(width,.16,.67),m[color],.017)
    box('Sign_top_cap',(x,front-.22,z+.33),(width+.07,.025,.035),m['aluminum'],.003)
    text_mesh('Shop_name',title,(x,front-.218,z-.21),.43,m['white'])
    if subtitle:
        text_mesh('Shop_service',subtitle,(x,front-.223,z-.05),.12,m['yellow'])


def awning(m, left, right, front, depth, color, z=2.95, fabric=False):
    # Sloping thin surface, with a narrow fabric valance or exposed metal lip.
    vertices=[]
    for i in range(13):
        x=left+(right-left)*i/12
        sag=.018*math.sin(i*math.pi/3)**2 if fabric else 0
        vertices.extend([(x,front-.11,z),(x,front-depth,z-.16-sag)])
    faces=[]
    for i in range(12):
        faces.extend([(2*i,2*i+1,2*i+3,2*i+2),(2*i+2,2*i+3,2*i+1,2*i)])
    mesh('Shop_canvas' if fabric else 'Shop_sheet_canopy',vertices,faces,m[color])
    box('Canopy_front_lip',((left+right)/2,front-depth,z-.20),
        (right-left,.017,.085 if fabric else .035),m[color],.002)
    for x in [left+.12,right-.12]:
        cylinder('Canopy_brace',(x,front-.04,z-.48),(x,front-depth+.03,z-.16),.013,m['metal'],8)
    if not fabric:
        for i in range(16):
            x=left+.05+i*(right-left-.1)/15
            cylinder('Canopy_fold',(x,front-.11,z+.006),(x,front-depth,z-.154),.008,m['aluminum'],5)


def shop_shell(m, front, wall):
    # Rear core starts at -4.20; this visible wall closes the actual recess.
    box('Shop_back_wall',(0,-4.27,1.69),(7.64,.12,2.58),m['cream'],.008)
    for x in [-3.91,3.91]:
        box('Shop_side_return',(x,-5.01,1.7),(.18,1.50,2.60),wall,.008)
    box('Shop_ceiling',(0,-5.02,2.96),(7.8,1.51,.10),m['cream'],.006)
    box('Shop_tile_floor',(0,-5.01,.405),(7.8,1.51,.025),m['cream'],.002)
    # Only a handful of joints; no small tile objects throughout the building.
    for x in [-3,-2,-1,0,1,2,3]:
        box('Floor_grout',(x,-5.01,.419),(.008,1.50,.003),m['concrete'],0)
    for y in [-5.65,-5.30,-4.95,-4.60]:
        box('Floor_grout',(0,y,.419),(7.8,.006,.003),m['concrete'],0)
    box('Shop_back_skirting',(0,-4.34,.65),(7.65,.025,.47),m['concrete'],.002)
    for x in [-2.1,1.8]:
        box('Interior_tube_housing',(x,-4.96,2.88),(1.22,.085,.06),m['aluminum'],.005)
        cylinder('Interior_fluorescent',(x-.52,-4.97,2.838),(x+.52,-4.97,2.838),.018,m['warm'],10)
    for x in [-3.86,3.86]:
        box('Tiled_pier',(x,front-.08,1.67),(.22,.23,2.52),m['cream'],.009)
        for j in range(9):
            box('Pier_grout',(x,front-.198,.52+j*.27),(.215,.005,.009),m['concrete'],0)
    box('Entrance_nosing',(0,front-.08,.39),(7.58,.22,.055),m['concrete'],.008)


def noodle_shop(m, front):
    sign(m,-.38,front,6.88,'阿荣面馆','red')
    # Offset painted address plaque and faded lower panel break the uniform sign.
    box('Shop_address',(3.52,front-.13,3.31),(.42,.06,.54),m['cream'],.006)
    text_mesh('Shop_address_number','28',(3.52,front-.166,3.20),.16,m['metal'])
    awning(m,-3.72,.8,front,.43,'aluminum')
    framed_opening(m,-3.7,.92,front)
    framed_opening(m,1.13,3.7,front)
    box('Noodle_partition_pier',(1.03,front+.05,1.68),(.16,.2,2.55),m['cream'],.006)
    # Open dining bay and an offset preparation counter, with a clear doorway.
    for x,y in [(-2.62,-4.93),(-.72,-4.88)]:
        table(m,x,y)
        stool(m,x-.52,y-.02)
        stool(m,x+.52,y-.035,'green')
    stool(m,-2.63,-5.38)
    box('Noodle_service_counter',(2.42,-5.15,.92),(2.13,.47,1.02),m['cream'],.012)
    box('Stainless_worktop',(2.42,-5.15,1.45),(2.18,.49,.045),m['aluminum'],.007)
    for x in [1.75,2.47,3.12]:
        cylinder('Soup_pot',(x,-5.06,1.48),(x,-5.06,1.70),.18,m['aluminum'],16)
        cylinder('Soup_pot_opening',(x,-5.06,1.704),(x,-5.06,1.71),.155,m['dark'],16)
        cylinder('Pot_rim',(x,-5.06,1.709),(x,-5.06,1.73),.18,m['aluminum'],16)
    box('Kitchen_tile_splash',(2.37,-4.35,1.46),(2.72,.018,1.07),m['white'],.001)
    for x in [1.1,1.6,2.1,2.6,3.1,3.6]:
        box('Kitchen_tile_joint',(x,-4.365,1.46),(.006,.006,1.05),m['concrete'],0)
    box('Kitchen_extract_hood',(2.4,-4.64,2.52),(2.60,.48,.28),m['aluminum'],.015)
    box('Hood_duct',(2.6,-4.48,2.78),(.39,.27,.28),m['metal'],.009)
    box('Wall_menu_board',(-1.66,-4.35,2.07),(1.38,.025,.81),m['red'],.008)
    text_mesh('Noodle_menu_title','云吞  汤粉',(-1.66,-4.37,2.25),.18,m['white'])
    for i in range(4):
        box('Menu_item_rule',(-1.75,-4.37,2.08-i*.125),(.94,.007,.014),m['cream'],0)
        box('Menu_item_price',(-1.12,-4.37,2.08-i*.125),(.12,.007,.018),m['yellow'],0)
    # Rolled-back accordion door leaves remain at the side rather than black glass.
    for i in range(5):
        box('Folded_door_leaf',(-3.62+i*.045,front+.12,1.61),(.027,.24,2.32),m['metal'],.002)


def convenience_shop(m, front):
    sign(m,-.75,front,6.16,'南城便利','green',z=3.34)
    sign(m,3.01,front,1.38,'修鞋','metal',z=3.18)
    awning(m,-3.71,-.38,front,.34,'green',z=2.96,fabric=True)
    framed_opening(m,-3.72,2.20,front)
    box('Shop_partition',(2.31,-5.02,1.68),(.16,1.47,2.54),m['concrete'],.008)
    shelf(m,-1.25,-4.50,2.76,184)
    shelf(m,1.15,-4.53,1.16,96)
    # Open refrigerator: contents, side walls, narrow gasket and top lit fascia.
    box('Fridge_back',(-3.06,-4.63,1.52),(1.01,.10,2.12),m['cream'],.01)
    for x in [-3.56,-2.56]:
        box('Fridge_side',(x,-4.86,1.52),(.055,.48,2.12),m['white'],.006)
    box('Fridge_advert',(-3.06,-5.08,2.48),(1.04,.05,.21),m['red'],.006)
    text_mesh('Fridge_label','冷饮',(-3.06,-5.11,2.41),.13,m['white'])
    for j in range(4):
        z=.65+j*.4
        box('Fridge_shelf',(-3.06,-4.86,z),(.95,.43,.025),m['aluminum'],.003)
        for i in range(5):
            x=-3.43+i*.18
            cylinder('Drink_bottle',(x,-4.98,z+.02),(x,-4.98,z+.23),.043,m['green'] if i%2 else m['yellow'],8)
            cylinder('Drink_cap',(x,-4.98,z+.23),(x,-4.98,z+.26),.026,m['red'],8)
    # Checkout pushed to one side leaves the entrance open at left.
    box('Checkout_counter',(.82,-5.36,.94),(1.88,.37,1.02),m['wood'],.013)
    box('Checkout_worktop',(.82,-5.36,1.47),(1.94,.41,.05),m['cream'],.008)
    box('Till_base',(.39,-5.30,1.53),(.28,.15,.08),m['metal'],.009)
    screen=box('Till_screen',(.39,-5.32,1.72),(.27,.035,.22),m['dark'],.008)
    screen.rotation_euler.x=-.22
    box('Till_stand',(.39,-5.27,1.64),(.035,.045,.15),m['metal'],.003)
    for i in range(3):
        box('Cardboard_stock_carton',(1.63,-4.95,.60+i*.31),(.53,.28,.29),m['cream'],.008)
        box('Carton_tape',(1.63,-5.096,.60+i*.31),(.08,.005,.29),m['yellow'],0)
    # Small neighbouring repair unit is partly closed, with a believable shutter.
    for i in range(19):
        box('Repair_shutter_slat',(3.04,front+.05,2.81-i*.061),(1.32,.045,.055),m['aluminum'],.003)
    for x in [2.38,3.70]:
        box('Shutter_runner',(x,front+.025,1.66),(.045,.08,2.5),m['metal'],.003)
    box('Repair_workbench',(3.05,-4.76,1.1),(1.2,.43,.07),m['wood'],.008)
    stool(m,2.90,-5.31,'metal')
    shelf(m,3.04,-4.49,1.16,41,levels=2)


def roast_shop(m, front):
    sign(m,.6,front,6.02,'巷口烧腊','metal',z=3.30)
    # Painted vertical sign and short canvas over the serving hatch only.
    box('Vertical_shop_sign',(-3.53,front-.13,3.25),(.56,.12,1.03),m['red'],.008)
    for i,t in enumerate('烧腊'):
        text_mesh('Vertical_letter',t,(-3.53,front-.195,3.44-i*.40),.31,m['white'])
    awning(m,-3.25,-.45,front,.48,'red',z=2.86,fabric=True)
    framed_opening(m,-3.28,-.39,front,top=2.70)
    framed_opening(m,-.15,3.70,front)
    box('Roast_partition_pier',(-.28,front+.03,1.66),(.16,.2,2.5),m['concrete'],.008)
    box('Roast_counter',(-1.83,-5.30,.91),(2.82,.46,.99),m['metal'],.012)
    box('Roast_worktop',(-1.83,-5.30,1.43),(2.89,.49,.055),m['aluminum'],.009)
    # An open display cabinet avoids fake opaque glazing; rails read as glass trim.
    for x in [-3.18,-.49]:
        box('Food_case_upright',(x,-5.48,1.93),(.032,.025,.98),m['aluminum'],.002)
    for z in [1.47,2.42]:
        box('Food_case_rail',(-1.83,-5.48,z),(2.72,.035,.035),m['aluminum'],.002)
    cylinder('Roast_hanging_rail',(-3.12,-5.23,2.31),(-.55,-5.23,2.31),.016,m['aluminum'],10)
    for i in range(7):
        x=-2.94+i*.35
        cylinder('Butcher_hook',(x,-5.23,2.31),(x,-5.23,2.15),.007,m['aluminum'],6)
        # Uneven cooked-duck silhouettes: rounded body, shoulders and short neck.
        # Avoid the scanned brick material, whose mortar reads as package bands.
        vertices=[];faces=[]
        for z,radius in [(1.77,.035),(1.82,.085),(1.94,.105),(2.05,.067),(2.15,.022)]:
            for j in range(9):
                a=j*math.tau/9
                vertices.append((x+math.cos(a)*radius,-5.23+math.sin(a)*radius*.50,
                                 z+(i%3)*.021))
        for ring in range(4):
            for j in range(9):
                n=ring*9+j;k=ring*9+(j+1)%9
                faces.append((n,k,k+9,n+9))
        faces.extend([tuple(reversed(range(9))),tuple(range(36,45))])
        mesh('Roast_display',vertices,faces,m['wood'])
        for side in [-1,1]:
            tapered('Roast_wing',(x+side*.08,-5.28,1.88),(x+side*.065,-5.27,2.01),.025,.035,m['wood'],6)
    cylinder('Chopping_block',(-1.15,-5.24,1.46),(-1.15,-5.24,1.56),.21,m['wood'],20)
    box('Kitchen_back_counter',(-1.79,-4.61,1.02),(2.8,.40,1.18),m['cream'],.009)
    box('Kitchen_back_worktop',(-1.79,-4.61,1.63),(2.86,.43,.035),m['aluminum'],.004)
    box('Roast_wall_menu',(-1.89,-4.35,2.19),(2.18,.025,.61),m['cream'],.008)
    text_mesh('Roast_wall_menu_title','烧鸭  叉烧  白切鸡',(-1.89,-4.37,2.14),.17,m['red'])
    for x,y in [(1.01,-4.89),(2.85,-4.98)]:
        table(m,x,y,.76)
        stool(m,x-.50,y-.10,'red')
        stool(m,x+.49,y-.06,'metal')
    # Folded door to the right reveals the dining bay and adds depth at its edge.
    for i in range(4):
        box('Stacked_sliding_door',(3.56-i*.04,front+.19,1.61),(.028,.28,2.29),m['aluminum'],.002)
    box('Wall_fan_box',(2.61,-4.37,2.39),(.44,.10,.42),m['cream'],.02)
    cylinder('Wall_fan_center',(2.61,-4.43,2.39),(2.61,-4.47,2.39),.16,m['metal'],16)
    for j in range(5):
        box('Wall_fan_grille',(2.61,-4.49,2.24+j*.075),(.34,.015,.009),m['aluminum'],0)


def storefront(m, variant, front, wall):
    shop_shell(m,front,wall)
    [noodle_shop,convenience_shop,roast_shop][variant](m,front)
