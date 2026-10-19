"""Author the independent interior finish kit; never save a city map.

Blender 4.5.3: --background --python-exit-code 1 --python this_file.py
Meters in Blender, centimeters after the established FBX conversion.
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/art'))
from art_common import reset, box, cylinder, text_mesh, join, export, material
from interior_finish_props import build_prop

SOURCE = ROOT/'AssetsSource/CityCourier'
OUT = SOURCE/'InteriorArt'
ASSETS = []
PALETTE = []


def export_canonical(obj, name):
    """Compensate the legacy UE FBX Y reflection in an export-only mesh.

    Editable Blender geometry and handoff coordinates stay canonical. Reflect
    winding and authored corner normals together, so both sides of the
    handedness conversion produce the same front faces in Unreal.
    """
    original=obj.data
    temporary=original.copy()
    reflect=Matrix.Diagonal((1.,-1.,1.,1.))
    normals={}
    if temporary.has_custom_normals:
        for face in temporary.polygons:
            for loop in face.loop_indices:
                normals[(face.index,temporary.loops[loop].vertex_index)]=reflect.to_3x3()@temporary.corner_normals[loop].vector
    try:
        obj.data=temporary
        temporary.transform(reflect)
        temporary.flip_normals()
        if normals:
            values=[None]*len(temporary.loops)
            for face in temporary.polygons:
                for loop in face.loop_indices:
                    values[loop]=normals[(face.index,temporary.loops[loop].vertex_index)]
            temporary.normals_split_custom_set(values)
        export(name,[obj],'InteriorArt')
    finally:
        obj.data=original
        bpy.data.meshes.remove(temporary)


def palette():
    result = {}
    rows = [
        ('plaster', (.70,.69,.62), .78, 0, 'plaster', 'painted_plaster_wall'),
        ('paint_green', (.24,.34,.27), .62, 0, 'plaster', 'painted_plaster_wall'),
        ('tile_ivory', (.64,.61,.51), .28, 0, 'tile', None),
        ('tile_grey', (.29,.32,.32), .38, 0, 'tile', None),
        ('wood_light', (.38,.23,.105), .51, 0, 'wood', None),
        ('wood_dark', (.105,.048,.022), .55, 0, 'wood', None),
        ('laminate_white', (.70,.71,.67), .42, 0, 'plain', None),
        ('metal_brushed', (.47,.49,.48), .28, .86, 'metal', None),
        ('metal_dark', (.027,.032,.031), .40, .72, 'metal', None),
        ('glass', (.29,.39,.39), .19, 0, 'glass', None),
        ('fabric_teal', (.045,.16,.15), .86, 0, 'fabric', None),
        ('fabric_beige', (.49,.43,.33), .87, 0, 'fabric', None),
        ('paper', (.76,.74,.65), .88, 0, 'plain', None),
        ('carton', (.34,.22,.105), .91, 0, 'wood', None),
        ('ceramic', (.76,.78,.73), .18, 0, 'plain', None),
        ('rubber', (.012,.014,.012), .86, 0, 'plain', None),
        ('accent_red', (.38,.035,.018), .50, 0, 'plain', None),
        ('plastic_blue', (.028,.14,.24), .42, 0, 'plain', None),
        ('emissive_warm', (.90,.76,.48), .35, 0, 'emissive', None),
        ('sign_green', (.018,.24,.10), .40, 0, 'emissive', None),
    ]
    for key,color,rough,metal,kind,scan in rows:
        name='M_IF_'+''.join(part.title() for part in key.split('_'))
        emission=2.5 if kind=='emissive' else 0.
        result[key]=material(name,color,rough,metal,scan=scan,emission=emission)
        definition=dict(key=key,name=name,color=list(color),roughness=rough,
                        metallic=metal,kind=kind,tile_cm=50. if kind=='tile' else 200.,emission=emission)
        if scan:definition['scan']=scan
        PALETTE.append(definition)
    if (SOURCE/'External/RealisticInteriors/sources.json').is_file():
        from realistic_shop_props import palette_definitions
        PALETTE[:] = palette_definitions(PALETTE)
        for definition in PALETTE:
            key=definition['key']
            if key not in result:
                result[key]=material(definition['name'],definition['color'],
                                     definition['roughness'],definition['metallic'])
            # Editable Blender sources show the same scans as the game.
            maps=definition.get('pbr_maps',{})
            mat=result[key];principled=mat.node_tree.nodes.get('Principled BSDF')
            for channel,pin in [('color','Base Color'),('rough','Roughness'),('normal','Normal')]:
                if channel not in maps:continue
                texture=mat.node_tree.nodes.new('ShaderNodeTexImage')
                texture.image=bpy.data.images.load(str(SOURCE/maps[channel]),check_existing=True)
                if channel!='color':texture.image.colorspace_settings.name='Non-Color'
                output=texture.outputs['Color']
                if channel=='normal':
                    normal=mat.node_tree.nodes.new('ShaderNodeNormalMap')
                    normal.inputs['Strength'].default_value=definition.get('normal_strength',1)
                    mat.node_tree.links.new(output,normal.inputs['Color']);output=normal.outputs['Normal']
                mat.node_tree.links.new(output,principled.inputs[pin])
    return result


def finish_asset(name, objects, collision='NoCollision', **metadata):
    anchor=bpy.data.objects.new('IF_AssetOrigin',bpy.data.meshes.new('IF_AssetOrigin'))
    bpy.context.collection.objects.link(anchor)
    obj=join([anchor]+objects,name)
    for polygon in obj.data.polygons:
        if polygon.area<1e-12:continue
    bounds=[obj.matrix_world@Vector(v) for v in obj.bound_box]
    lo=[min(v[i] for v in bounds)*100 for i in range(3)]
    hi=[max(v[i] for v in bounds)*100 for i in range(3)]
    triangles=sum(len(face.vertices)-2 for face in obj.data.polygons)
    assert all(math.isfinite(v) for v in lo+hi)
    assert triangles<60000,(name,triangles)
    if metadata.get('family')=='frame':
        # Check real authored polygon extents: trim must not eat the canonical
        # 100x220cm clear hole, even when it has no gameplay collision.
        for face in obj.data.polygons:
            points=[obj.matrix_world@obj.data.vertices[i].co for i in face.vertices]
            overlaps_x=max(v.x for v in points)>1e-6 and min(v.x for v in points)<1-1e-6
            overlaps_z=max(v.z for v in points)>1e-6 and min(v.z for v in points)<2.2-1e-6
            assert not(overlaps_x and overlaps_z), 'Frame trim intrudes into clear aperture'
    export_canonical(obj,name)
    blend=OUT/'Blender'/(name+'.blend');blend.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    ASSETS.append(dict(name=name,fbx='Exports/InteriorArt/'+name+'.fbx',
                       blend=str(blend.relative_to(SOURCE)).replace('\\','/'),
                       minimum_cm=lo,maximum_cm=hi,triangles=triangles,collision=collision,
                       material_names=sorted({obj.data.materials[p.material_index].name for p in obj.data.polygons}),
                       export_axis_adapter='FBX-only Y reflection and reversed winding; canonical Blender geometry unchanged',
                       **metadata))


def collect(builder, name, m, **metadata):
    reset();before=set(bpy.data.objects)
    builder(m)
    finish_asset(name,sorted(set(bpy.data.objects)-before,key=lambda o:o.name),**metadata)


def door(m, style):
    glass=style=='Shop'
    main=m['wood_dark' if style=='Residential' else 'laminate_white']
    if glass:
        box('Glazed_leaf',(.5,0,1.1),(.90,.025,2.08),m['glass'],.003)
        for x in (.025,.975):box('Leaf_edge', (x,0,1.1),(.05,.05,2.2),m['metal_dark'],.004)
        for z in (.035,1.0,2.165):box('Leaf_crossbar',(.5,0,z),(.90,.05,.07),m['metal_dark'],.004)
        for side in (-1,1):
            cylinder('Vertical_pull',(.85,side*.075,.80),(.85,side*.075,1.20),.012,m['metal_brushed'],12)
            for z in (.82,1.18):cylinder('Pull_standoff',(.85,side*.025,z),(.85,side*.075,z),.010,m['metal_brushed'],12)
    else:
        box('Leaf_panel',(.5,0,1.1),(1,.044,2.2),main,.006)
        for side in (-1,1):
            if style=='Residential':
                for z,height in ((.48,.60),(1.45,.98)):
                    box('Recessed_panel',(.5,side*.023,z),(.76,.008,height),m['wood_light'],.008)
                    for x in (.105,.895):box('Panel_bead',(x,side*.030,z),(.014,.012,height+.035),main,.002)
            else:
                box('Office_inset',(.5,side*.024,1.42),(.72,.009,1.02),m['glass'],.004)
                for x in (.125,.875):box('Inset_frame',(x,side*.030,1.42),(.024,.014,1.065),m['metal_brushed'],.003)
            box('Lever_backplate',(.86,side*.028,.99),(.045,.014,.18),m['metal_brushed'],.006)
            cylinder('Lever_spindle',(.86,side*.035,1.02),(.86,side*.07,1.02),.012,m['metal_brushed'],12)
            cylinder('Lever_handle',(.86,side*.07,1.02),(.73,side*.07,1.02),.011,m['metal_brushed'],12)
            cylinder('Keyhole',(.86,side*.036,.95),(.86,side*.039,.95),.006,m['rubber'],10)
    for z in (.22,1.10,1.98):
        cylinder('Hinge_knuckle',(0,.018,z-.045),(0,.018,z+.045),.010,m['metal_brushed'],12)
    box('Kickplate',(.5,-.028,.12),(.90,.008,.14),m['metal_brushed'],.003)


def frame(m):
    for x in (-.04,1.04):
        box('Door_jamb',(x,0,1.10),(.08,.24,2.2),m['wood_dark'],.004)
        trim_x=-.05 if x<0 else 1.05
        for y in (-.125,.125):box('Architrave',(trim_x,y,1.12),(.10,.016,2.24),m['wood_light'],.003)
    box('Door_header',(.5,0,2.24),(1.16,.24,.08),m['wood_dark'],.004)
    for y in (-.125,.125):box('Header_bead',(.5,y,2.255),(1.20,.016,.08),m['wood_light'],.003)


def simple(m, kind):
    if kind=='Skirting':
        box('Skirting_face',(.5,0,.045),(1,.016,.09),m['wood_dark'],.002)
        cylinder('Skirting_bead',(0,-.003,.09),(1,-.003,.09),.005,m['wood_light'],12)
    elif kind=='DadoPanel':
        box('Lower_wall_finish',(.5,.006,.54),(1,.012,1.08),m['paint_green'],.001)
        box('Dado_cap',(.5,.005,1.09),(1,.025,.02),m['laminate_white'],.002)
    elif kind=='HandrailPost':
        box('Post_flange',(0,0,.006),(.07,.07,.012),m['metal_dark'],.003)
        cylinder('Rail_post',(0,0,.012),(0,0,.90),.017,m['metal_dark'],12)
        for x in (-.022,.022):
            for y in (-.022,.022):cylinder('Bolt',(x,y,.012),(x,y,.017),.004,m['metal_brushed'],8)
    elif kind=='HandrailBar':
        cylinder('Round_handrail',(0,0,0),(1,0,0),.022,m['metal_brushed'],16)
    elif kind=='NumberPlate':
        box('Blank_numberplate',(0,0,.04),(.24,.013,.08),m['metal_brushed'],.004)
        box('Blank_inset',(0,-.007,.04),(.22,.003,.06),m['wood_dark'],.002)
    elif kind=='LightPanel':
        box('Luminaire_case',(0,0,.023),(1,.18,.04),m['laminate_white'],.006)
        box('Light_diffuser',(0,0,.004),(.94,.135,.008),m['emissive_warm'],.003)
        for x in (-.46,.46):box('Endcap',(x,0,.009),(.025,.16,.018),m['metal_dark'],.002)
    elif kind=='ExitSign':
        box('Exit_case',(0,0,.075),(.40,.04,.15),m['metal_dark'],.004)
        box('Exit_face',(0,-.022,.075),(.37,.006,.125),m['sign_green'],.002)
        text_mesh('Exit_label','安全出口',(0,-.026,.042),.045,m['laminate_white'])
    elif kind in ('SwitchPlate','Outlet'):
        box('Wall_plate',(0,0,.0425),(.085,.014,.085),m['laminate_white'],.004)
        if kind=='SwitchPlate':box('Rocker',(0,-.009,.043),(.054,.006,.050),m['ceramic'],.003)
        else:
            for x in (-.015,.015):
                for z in (.027,.057):box('Socket_slot',(x,-.008,z),(.004,.003,.013),m['rubber'],.001)
    elif kind=='StairNosing':
        box('Stair_edge',(0,0,.0035),(1,.03,.007),m['metal_brushed'],.001)
        for y in (-.008,0,.008):box('Antislip_groove',(0,y,.007),(1,.002,.001),m['rubber'],0)
    elif kind=='WindowReturn':
        for x in (-.6,.6):box('Window_reveal',(x,0,.675),(.06,.16,1.35),m['laminate_white'],.003)
        for z in (.025,1.325):box('Window_header',(0,0,z),(1.20,.16,.05),m['laminate_white'],.003)
        box('Glazing',(0,.005,.675),(1.14,.018,1.25),m['glass'],.002)
        box('Window_mullion',(0,-.015,.675),(.03,.035,1.27),m['metal_dark'],.002)
        box('Window_sill',(0,-.045,.025),(1.28,.28,.05),m['tile_ivory'],.004)
    elif kind=='AirconWall':
        box('AC_body',(0,0,.14),(.85,.20,.28),m['laminate_white'],.025)
        for z in (.04,.065,.09):box('AC_louver',(0,-.101,z),(.75,.008,.01),m['metal_dark'],.002)
        box('AC_display',(.31,-.105,.16),(.055,.005,.022),m['plastic_blue'],.002)
    else:raise ValueError(kind)


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'.gitattributes').write_text('* -text whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol\n',encoding='utf-8')
    m=palette()
    for kind in ('Bed','Desk','Kitchen','Toilet','Shelf','Sofa','Counter'):
        reset()
        finish_asset('SM_IF_'+kind,build_prop(kind,m),'BlockAll',family='furniture',kind=kind)
    for style in ('Residential','Shop','Office'):
        collect(lambda mats:door(mats,style),'SM_IF_Door'+style,m,collision='BlockAll',family='door',hinge_origin_cm=[0,0,0],clear_width_cm=100,clear_height_cm=220)
    collect(frame,'SM_IF_DoorFrame',m,family='frame',clear_width_cm=100,clear_height_cm=220)
    for kind in ('Skirting','DadoPanel','HandrailPost','HandrailBar','NumberPlate','LightPanel','ExitSign','SwitchPlate','Outlet','StairNosing','WindowReturn','AirconWall'):
        collect(lambda mats:simple(mats,kind),'SM_IF_'+kind,m,
                collision='BlockAll' if kind.startswith('Handrail') else 'NoCollision',family='fixture')
    (OUT/'palette.json').write_text(json.dumps({'materials':PALETTE},indent=2)+'\n',encoding='utf-8')
    record={'schema_version':1,'asset_root':'/Game/CityCourier/Art/Interiors','assets':ASSETS,
            'license':'Original project geometry; existing CC0 Poly Haven scans; Noto Sans CJK OFL exit label',
            'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'no_city_maps_written':True,'unit':'centimeters in exported metadata; meters in Blender'}
    (OUT/'asset-manifest.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    print('INTERIOR FINISH KIT',len(ASSETS),'assets',sum(a['triangles'] for a in ASSETS),'triangles',flush=True)
    # Room-sized retained storefront assemblies are part of a full rebuild.
    from build_shop_furnishings import build as build_shops
    build_shops()


if __name__=='__main__':build()
