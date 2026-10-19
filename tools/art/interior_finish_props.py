"""Compact, detailed room-kit furnishings; meters, Z up, customer side -Y.

The caller owns materials, scene lifecycle, pivots, joining and export.  No
external images, fonts, brands or scene state are required by this module.
"""
import math

import bpy
import bmesh
from art_common import box, cylinder, finish


def _mesh(name, verts, faces, mat, smooth=False):
    mesh = bpy.data.meshes.new(name + '_mesh')
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return finish(obj, name, mat, smooth=smooth)


def _tube(name, points, radius, mat):
    """A continuous bent cable, pipe or stitched welt, exported as a mesh."""
    curve = bpy.data.curves.new(name + '_path', 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 8
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    curve.use_fill_caps = True
    closed = points[0] == points[-1]
    if closed:
        points = points[:-1]
    path = curve.splines.new('BEZIER')
    path.use_cyclic_u = closed
    path.bezier_points.add(len(points) - 1)
    for bp, point in zip(path.bezier_points, points):
        bp.co = point
        bp.handle_left_type = bp.handle_right_type = 'AUTO'
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.convert(target='MESH')
    obj = bpy.context.object
    # Curve conversion can retain separate cap vertices. Weld those rings and
    # close any remaining terminal loops so cables survive physical validation.
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.0000005)
    boundary = [edge for edge in bm.edges if edge.is_boundary]
    if boundary:
        bmesh.ops.holes_fill(bm, edges=boundary, sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def _oval_profile(name, xy, rings, mat, segments=48):
    """Closed revolved oval profile, including a real inner well when specified.

    Rings are (radius_x, radius_y, height).  The profile closes from its final
    ring to its first; tiny central rings avoid collapsed zero-area polygons.
    """
    verts = [(xy[0] + rx * math.cos(a * math.tau / segments),
              xy[1] + ry * math.sin(a * math.tau / segments), z)
             for rx, ry, z in rings for a in range(segments)]
    faces = []
    for r in range(len(rings)):
        r2 = (r + 1) % len(rings)
        for a in range(segments):
            b = (a + 1) % segments
            faces.append((r*segments+a, r*segments+b,
                          r2*segments+b, r2*segments+a))
    return _mesh(name, verts, faces, mat, True)


def _cloth(name, loc, size, mat, folds=.008, phase=0, nx=18, ny=26):
    """Closed soft pad/blanket with restrained wrinkles and softened perimeter."""
    width, depth, thick = size
    verts = []
    for layer in range(2):
        for j in range(ny+1):
            v = j/ny
            for i in range(nx+1):
                u = i/nx
                edge = math.sin(math.pi*u)**.4 * math.sin(math.pi*v)**.4
                ripple = folds * (math.sin(u*29 + v*8 + phase) +
                                   .45*math.sin(v*35-u*7)) * edge
                crown = thick * .3 * edge
                z = loc[2] + (thick*.5 + crown + ripple if layer else -thick*.5)
                # Round the plan corners without beveling the fold grid.
                x = (u-.5)*width * (1-.035*abs(2*v-1)**8)
                y = (v-.5)*depth * (1-.035*abs(2*u-1)**8)
                verts.append((loc[0]+x, loc[1]+y, z))
    stride = nx+1
    count = stride*(ny+1)
    faces = []
    for j in range(ny):
        for i in range(nx):
            a = j*stride+i
            faces.append((a+count, a+1+count, a+stride+1+count, a+stride+count))
            faces.append((a+stride, a+stride+1, a+1, a))
    boundary = list(range(stride))
    boundary += [j*stride+nx for j in range(1, ny+1)]
    boundary += [ny*stride+i for i in range(nx-1, -1, -1)]
    boundary += [j*stride for j in range(ny-1, 0, -1)]
    for a, b in zip(boundary, boundary[1:]+boundary[:1]):
        faces.append((a, b, b+count, a+count))
    return _mesh(name, verts, faces, mat, True)


def _welt(name, loc, size, mat, radius=.002):
    x, y, z = loc
    w, d = size
    r = min(.025, d*.18)
    return _tube(name, [(x-w/2+r,y-d/2,z), (x+w/2-r,y-d/2,z),
                        (x+w/2,y-d/2+r,z), (x+w/2,y+d/2-r,z),
                        (x+w/2-r,y+d/2,z), (x-w/2+r,y+d/2,z),
                        (x-w/2,y+d/2-r,z), (x-w/2,y-d/2+r,z),
                        (x-w/2+r,y-d/2,z)], radius, mat)


def _pull(name, x, y, z, mat, width=.12):
    for dx in [-width/2, width/2]:
        cylinder(name+'_mount', (x+dx,y+.018,z), (x+dx,y,z), .006, mat, 10)
    cylinder(name, (x-width/2,y,z), (x+width/2,y,z), .006, mat, 12)


def _cabinet(m, top=.88):
    box('Recessed_toe_kick', (0,.015,.055), (1.12,.43,.11), m['metal_dark'], .007)
    box('Cabinet_bottom', (0,.015,.13), (1.18,.47,.045), m['wood_light'], .005)
    for x in [-.587,.587]:
        box('Cabinet_side_panel', (x,.015,(top+.12)/2), (.026,.47,top-.12), m['laminate_white'], .003)
    box('Cabinet_back_panel', (0,.24,top/2), (1.16,.018,top-.10), m['laminate_white'], .002)
    for x in [-.392,0,.392]:
        box('Separate_cabinet_door', (x,-.235,(top+.12)/2), (.38,.025,top-.14), m['paint_green'], .006)
        box('Recessed_door_inset', (x,-.251,(top+.12)/2), (.322,.008,top-.205), m['paint_green'], .005)
        _pull('Brushed_cabinet_pull', x,-.265,top-.12,m['metal_brushed'],.10)


def _carton(name, loc, size, m, opened=False):
    x,y,z = loc
    w,d,h = size
    box(name,loc,size,m['carton'],.006)
    box(name+'_paper_tape',(x,y,z+h/2+.001),(w*.15,d*.97,.002),m['paper'],.0005)
    for dx in [-w*.31,w*.31]:
        box(name+'_fold',(x+dx,y-d/2-.001,z),(w*.005,.002,h*.82),m['wood_light'],.0002)
    box(name+'_blank_shipping_patch',(x+w*.2,y-d/2-.002,z+h*.1),(w*.31,.002,h*.29),m['paper'],.0005)
    if opened:
        flap = box(name+'_raised_flap',(x,y+d*.44,z+h*.54),(w*.94,d*.3,.008),m['carton'],.001)
        flap.rotation_euler.x = -.22


def _bottle(name, x, y, z, m, accent='plastic_blue', scale=1):
    _oval_profile(name,(x,y),[(.001,.001,z),(.027*scale,.027*scale,z),
        (.032*scale,.032*scale,z+.018*scale),(.032*scale,.032*scale,z+.14*scale),
        (.015*scale,.015*scale,z+.17*scale),(.014*scale,.014*scale,z+.20*scale),
        (.001,.001,z+.20*scale)],m[accent],24)
    cylinder(name+'_cap',(x,y,z+.196*scale),(x,y,z+.212*scale),.016*scale,m['laminate_white'],20)
    _oval_profile(name+'_plain_label',(x,y),[(.0324*scale,.0324*scale,z+.045*scale),
        (.0324*scale,.0324*scale,z+.11*scale),(.0319*scale,.0319*scale,z+.11*scale),
        (.0319*scale,.0319*scale,z+.045*scale)],m['paper'],24)


def _bed(m):
    for x in [-.315,.315]:
        for y in [-.75,.75]:
            box('Bed_tapered_foot',(x,y,.105),(.065,.065,.21),m['wood_dark'],.009)
    box('Bed_underframe',(0,0,.245),(.76,1.73,.15),m['wood_light'],.014)
    for x in [-.374,.374]:
        box('Bed_side_rail',(x,0,.31),(.025,1.69,.13),m['wood_dark'],.006)
    box('Raised_headboard',(0,.85,.59),(.79,.065,1.00),m['wood_light'],.014)
    for x in [-.245,0,.245]:
        box('Headboard_inset_panel',(x,.811,.68),(.223,.013,.47),m['wood_dark'],.008)
    _cloth('Bound_mattress',(0,-.005,.415),(.735,1.66,.15),m['fabric_beige'],.002)
    _welt('Mattress_bound_edge',(0,-.005,.445),(.73,1.65),m['paper'],.003)
    _cloth('Draped_cotton_duvet',(0,-.245,.515),(.75,1.14,.055),m['fabric_teal'],.009,1.1)
    _cloth('Duvet_folded_header',(0,.28,.55),(.745,.17,.027),m['fabric_beige'],.005,2)
    _welt('Duvet_edge_binding',(0,-.245,.536),(.735,1.125),m['fabric_teal'],.0025)
    _cloth('Pillow',(0,.59,.548),(.56,.32,.085),m['fabric_beige'],.004,2.2,20,12)
    _welt('Pillow_sewn_edge',(0,.59,.552),(.55,.31),m['paper'],.0015)


def _desk(m):
    box('Desktop_laminate_core',(0,0,.747),(1.025,.58,.044),m['wood_light'],.009)
    box('Desktop_work_surface',(0,0,.772),(1.025,.58,.008),m['laminate_white'],.003)
    for y in [-.225,.225]:
        box('Desk_square_leg',(.443,y,.37),(.034,.034,.74),m['metal_dark'],.004)
        box('Desk_foot_pad',(.443,y,.008),(.045,.045,.016),m['rubber'],.003)
    box('Desk_rear_stretcher',(.10,.223,.19),(.69,.025,.028),m['metal_dark'],.003)
    box('Drawer_pedestal',(-.345,.035,.379),(.28,.47,.69),m['laminate_white'],.008)
    for z,h in [(.205,.27),(.45,.19),(.635,.15)]:
        box('Separate_drawer_front',(-.345,-.21,z),(.263,.025,h),m['paint_green'],.004)
        _pull('Drawer_pull',-.345,-.244,z+h*.23,m['metal_brushed'],.085)
    box('Monitor_foot',(.145,.137,.791),(.22,.16,.026),m['metal_dark'],.009)
    box('Monitor_neck',(.145,.18,.928),(.045,.034,.25),m['metal_dark'],.005)
    box('Monitor_bezel',(.145,.173,1.086),(.47,.035,.302),m['metal_dark'],.01)
    box('Monitor_screen',(.145,.153,1.09),(.436,.004,.263),m['glass'],.003)
    cylinder('Monitor_status_LED',(.345,.150, .955),(.345,.147,.955),.002,m['emissive_warm'],10)
    box('Keyboard_body',(.12,-.127,.79),(.35,.127,.021),m['metal_dark'],.005)
    for row in range(4):
        for col in range(12):
            box('Keyboard_key',(-.025+col*.026,-.166+row*.025,.805),(.022,.020,.009),m['laminate_white'],.002)
    box('Keyboard_spacebar',(.116,-.177,.808),(.12,.012,.007),m['laminate_white'],.002)
    box('Mouse_mat',(.389,-.13,.779),(.15,.20,.004),m['fabric_teal'],.007)
    _cloth('Mouse',(.39,-.11,.795),(.047,.082,.026),m['metal_dark'],0,0,8,12)
    _tube('Monitor_power_cable',[(.145,.20,1.00),(.19,.256,.88),(.24,.257,.72),(.29,.24,.41),(.38,.235,.34)],.004,m['rubber'])
    _tube('Keyboard_cable',[(.13,-.05,.8),(.12,.019,.784),(.27,.09,.784),(.29,.25,.77)],.0023,m['rubber'])
    box('Paper_stack',(-.365,.10,.786),(.18,.24,.018),m['paper'],.002)
    box('Paper_top_sheet',(-.355,.087,.797),(.18,.235,.002),m['paper'],.0003)
    cylinder('Pen',(-.42,-.015,.804),(-.29,-.015,.804),.003,m['plastic_blue'],10)
    # A small chair fits underneath, entirely inside the real desk footprint.
    for x in [-.095,.17]:
        for y in [-.235,.005]:
            cylinder('Tucked_chair_leg',(x,y,.015),(x,y,.44),.012,m['metal_dark'],12)
    _cloth('Tucked_chair_seat',(.035,-.112,.451),(.31,.29,.041),m['fabric_teal'],.002,0,10,10)
    for x in [-.103,.173]:
        cylinder('Tucked_chair_back_post',(x,-.245,.39),(x,-.245,.69),.011,m['metal_dark'],12)
    box('Tucked_chair_back',(.035,-.255,.626),(.30,.035,.12),m['wood_light'],.016)


def _kitchen(m):
    _cabinet(m,.845)
    # Countertop is built around an aperture; there is no solid slab over the sink.
    for x,w in [(-.55,.14),(.265,.71)]:
        box('Worktop_solid_section',(x,0,.876),(w,.52,.045),m['tile_ivory'],.005)
    for y in [-.222,.222]:
        box('Sink_worktop_edge',(-.29,y,.876),(.39,.076,.045),m['tile_ivory'],.004)
    _oval_profile('Hollow_stainless_sink',(-.29,0),[(.205,.18,.899),(.211,.185,.89),
        (.195,.169,.879),(.16,.128,.745),(.005,.005,.739),(.005,.005,.748),
        (.145,.114,.754),(.181,.151,.885),(.197,.174,.901)],m['metal_brushed'])
    cylinder('Sink_drain',(-.29,0,.748),(-.29,0,.752),.027,m['metal_dark'],24)
    _tube('Gooseneck_faucet',[(-.29,.205,.901),(-.29,.205,1.09),(-.29,.16,1.15),(-.29,.074,1.12),(-.29,.063,1.075)],.011,m['metal_brushed'])
    cylinder('Tap_lever',(-.248,.202,.94),(-.215,.202,1.00),.006,m['metal_brushed'],12)
    box('Induction_hob',(.30,.018,.907),(.43,.39,.018),m['glass'],.008)
    for x,y,r in [(.205,.085,.067),(.40,-.06,.071)]:
        _oval_profile('Inset_cooking_ring',(x,y),[(r,r,.918),(r+.002,r+.002,.918),(r+.002,r+.002,.920),(r,r,.920)],m['metal_brushed'],40)
    for x in [.28,.315,.35]:
        cylinder('Hob_control_dot',(x,-.145,.918),(x,-.145,.92),.004,m['paper'],12)
    _bottle('Dish_soap',-.535,.147,.9,m,'plastic_blue',.7)
    box('Folded_dishcloth',(.51,-.12,.926),(.105,.16,.02),m['fabric_beige'],.008)
    box('Backsplash_panel',(0,.257,1.08),(1.22,.012,.37),m['tile_ivory'],.003)
    for x in [-.405,0,.405]:
        box('Backsplash_grout',(x,.249,1.08),(.002,.001,.36),m['tile_grey'],.0002)
    box('Backsplash_horizontal_grout',(0,.249,1.08),(1.21,.001,.002),m['tile_grey'],.0002)


def _toilet(m):
    # Continuous ceramic profile travels up the outside, over the rim and down
    # into the bowl. The small central closure lies below the visible trap well.
    _oval_profile('Toilet_foot',(0,-.02),[(.001,.001,0),(.126,.19,0),(.148,.215,.026),
        (.112,.16,.22),(.082,.12,.28),(.001,.001,.28)],m['ceramic'])
    _oval_profile('Hollow_toilet_bowl',(0,-.095),[(.003,.004,.235),(.12,.19,.24),
        (.19,.28,.36),(.207,.292,.415),(.201,.285,.439),(.165,.245,.44),
        (.15,.222,.412),(.12,.18,.325),(.065,.09,.271),(.003,.004,.267)],m['ceramic'])
    _oval_profile('Open_toilet_seat',(0,-.095),[(.209,.294,.441),(.209,.294,.462),
        (.16,.239,.466),(.16,.239,.442)],m['laminate_white'])
    cylinder('Bowl_water_surface',(0,-.095,.28),(0,-.095,.282),.06,m['glass'],32)
    box('Cistern',(0,.265,.585),(.385,.17,.405),m['ceramic'],.03)
    box('Cistern_lid',(0,.265,.795),(.407,.186,.029),m['ceramic'],.011)
    for x in [-.028,.028]:
        cylinder('Dual_flush_button',(x,.265,.811),(x,.265,.817),.024,m['metal_brushed'],24)
    for x in [-.112,.112]:
        cylinder('Seat_hinge',(x,.145,.449),(x,.185,.449),.013,m['metal_brushed'],16)
    _tube('Cistern_supply_pipe',[(.15,.29,.39),(.16,.31,.24),(.21,.30,.16)],.008,m['metal_brushed'])
    cylinder('Supply_isolation_valve',(.20,.30,.16),(.245,.30,.16),.018,m['metal_brushed'],12)
    # Slim brush and holder tuck beside the cistern, inside the allotted tile.
    _oval_profile('Brush_holder',(.255,.18),[(.001,.001,0),(.039,.039,0),(.041,.041,.14),
        (.033,.033,.145),(.029,.029,.13),(.029,.029,.025),(.001,.001,.025)],m['laminate_white'],28)
    cylinder('Brush_handle',(.255,.18,.08),(.255,.18,.36),.007,m['metal_brushed'],12)
    cylinder('Brush_grip',(.255,.18,.29),(.255,.18,.37),.012,m['rubber'],16)


def _shelf(m):
    for x in [-.593,.593]:
        for y in [-.17,.17]:
            box('Shelf_upright',(x,y,.905),(.028,.028,1.81),m['metal_dark'],.003)
            box('Shelf_foot',(x,y,.012),(.048,.048,.024),m['rubber'],.003)
        for z in [.22,.61,1.00,1.39,1.77]:
            box('Shelf_side_rail',(x,0,z),(.025,.385,.04),m['metal_dark'],.003)
    for z in [.18,.57,.96,1.35,1.74]:
        box('Shelf_board',(0,0,z),(1.20,.405,.034),m['wood_light'],.004)
        box('Shelf_front_lip',(0,-.204,z-.005),(1.20,.018,.046),m['metal_brushed'],.003)
    for a,b in [((-.58,.18,.22),(.58,.18,1.73)),((.58,.18,.22),(-.58,.18,1.73))]:
        cylinder('Shelf_back_diagonal',a,b,.007,m['metal_dark'],10)
    _carton('Stock_carton_A',(-.345,0,.365),(.42,.33,.33),m)
    _carton('Stock_carton_B',(.25,.015,.32),(.47,.32,.24),m)
    _carton('Part_open_carton',(.31,.02,.745),(.44,.32,.30),m,True)
    for i in range(7):
        x=-.49+i*.068
        color=['paint_green','fabric_beige','plastic_blue','wood_dark'][i%4]
        box('Archive_binder',(x,-.005,.755),(.055,.29,.32),m[color],.004)
        box('Binder_label',(x,-.153,.78),(.035,.003,.12),m['paper'],.001)
        cylinder('Binder_ring_hole',(x,-.155,.655),(x,-.16,.655),.008,m['metal_brushed'],12)
    for i in range(10):
        x=-.495+i*.065
        h=.22+.035*(i%3)
        mat=['paper','paint_green','accent_red','wood_light'][i%4]
        box('Stored_book',(x,.015,.982+h/2),(.051,.255,h),m[mat],.002)
        box('Book_spine_band',(x,-.114,1.02),(.046,.003,.009),m['paper'],.0005)
    box('Document_tray',(.39,-.015,1.001),(.28,.31,.045),m['metal_dark'],.005)
    for i in range(3):
        box('Stacked_document_ream',(.39,-.01,1.036+i*.025),(.255,.285,.022),m['paper'],.002)
    for i in range(3):
        _carton('Upper_archive_carton',(-.405+i*.4,.01,1.50),(.34,.30,.26),m)
    _carton('Small_top_storage',(-.30,.025,1.79),(.40,.29,.065),m)


def _sofa(m):
    for x in [-.49,.49]:
        for y in [-.22,.22]:
            box('Sofa_wood_foot',(x,y,.07),(.055,.055,.14),m['wood_dark'],.007)
    box('Sofa_lower_frame',(0,0,.235),(1.19,.59,.22),m['fabric_beige'],.035)
    box('Sofa_back',(0,.235,.60),(1.20,.14,.64),m['fabric_teal'],.045)
    for x in [-.548,.548]:
        box('Sofa_padded_arm',(x,-.005,.43),(.13,.595,.40),m['fabric_teal'],.045)
    for x in [-.25,.25]:
        _cloth('Sofa_seat_cushion',(x,-.065,.395),(.477,.435,.13),m['fabric_teal'],.002,1,14,14)
        _welt('Seat_cushion_piping',(x,-.065,.425),(.465,.422),m['fabric_beige'],.0018)
        # Upright cushions use soft closed mesh, then rotate about their center.
        cushion=_cloth('Sofa_back_cushion',(0,0,0),(.475,.40,.13),m['fabric_teal'],.003,2,12,10)
        cushion.location=(x,.138,.66)
        cushion.rotation_euler.x=math.pi/2-.12
    _cloth('Folded_sofa_throw',(-.34,-.10,.49),(.25,.36,.027),m['fabric_beige'],.004,1,10,14)
    for x in [-.42,-.39,-.36,-.33,-.30,-.27,-.24]:
        _tube('Throw_woven_fringe',[(x,-.279,.49),(x+.002,-.295,.48),(x,-.30,.471)],.001,m['fabric_beige'])


def _counter(m):
    _cabinet(m,.905)
    box('Counter_top',(0,0,.936),(1.23,.52,.05),m['wood_light'],.01)
    box('Counter_front_trim',(0,-.261,.858),(1.205,.018,.04),m['wood_dark'],.003)
    box('Pickup_tray_bottom',(.24,-.02,.973),(.47,.35,.022),m['metal_brushed'],.004)
    for y in [-.192,.152]:
        box('Pickup_tray_edge',(.24,y,.987),(.47,.012,.035),m['metal_brushed'],.004)
    for x in [.012,.468]:
        box('Pickup_tray_edge',(x,-.02,.987),(.012,.34,.035),m['metal_brushed'],.004)
    for x,y in [(.13,-.065),(.34,.015)]:
        box('Takeout_food_container',(x,y,1.027),(.16,.20,.082),m['paper'],.009)
        box('Takeout_lid_lip',(x,y,1.073),(.172,.211,.014),m['laminate_white'],.005)
        box('Takeout_sealing_tab',(x,y,1.082),(.032,.217,.003),m['carton'],.0005)
    box('Receipt_pad',(-.25,-.15,.97),(.13,.17,.013),m['paper'],.002)
    cylinder('Counter_pen',(-.31,-.205,.982),(-.21,-.13,.982),.003,m['plastic_blue'],10)
    _bottle('Takeaway_bottle_A',-.49,.14,.962,m,'plastic_blue',.85)
    _bottle('Takeaway_bottle_B',-.39,.14,.962,m,'paint_green',.85)
    # Compact card terminal with a recessed screen and individual plain keys.
    box('Payment_terminal',(-.14,.12,.991),(.145,.17,.06),m['metal_dark'],.012)
    box('Payment_terminal_screen',(-.14,.155,1.023),(.105,.062,.004),m['glass'],.003)
    for row in range(3):
        for col in range(3):
            box('Terminal_key',(-.17+col*.03,.065+row*.022,1.024),(.024,.016,.004),m['laminate_white'],.002)
    _tube('Terminal_cable',[(-.14,.2,.99),(-.12,.231,.97),(.02,.232,.965),(.05,.23,.93)],.003,m['rubber'])


_BUILDERS = {'Bed': _bed, 'Desk': _desk, 'Kitchen': _kitchen,
             'Toilet': _toilet, 'Shelf': _shelf, 'Sofa': _sofa, 'Counter': _counter}


def build_prop(kind: str, m: dict) -> list[bpy.types.Object]:
    """Build one reusable furnishing; return only newly created mesh objects.

    Every assembly rests at Z=0 and fits the documented room-kit envelope.
    Materials are borrowed from ``m`` and are never created or changed here.
    """
    if kind not in _BUILDERS:
        raise ValueError('Unsupported interior prop: ' + kind)
    start = set(bpy.data.objects)
    _BUILDERS[kind](m)
    return sorted(set(bpy.data.objects)-start, key=lambda obj: obj.name)
