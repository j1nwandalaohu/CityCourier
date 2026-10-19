"""CC0 scans and purpose-built hanging ducks inside existing shop envelopes.

Coordinates are final kit meters, customer side -Y. Scanned meshes retain UV0;
duck geometry is original project work, not derived from unlicensed models.
"""
import json
import math
import re
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.art.realism_sources import validate_sources
from art_common import box, sphere, cylinder, join
from interior_finish_props import _tube, _oval_profile

SOURCE = ROOT/'AssetsSource/CityCourier'
EXTERNAL = SOURCE/'External/RealisticInteriors'
MODEL_MATERIALS = {}


def palette_definitions(rows):
    manifest = json.loads((EXTERNAL/'sources.json').read_text())
    errors = validate_sources(manifest, EXTERNAL)
    if errors:
        raise RuntimeError('External sources rejected: '+str(errors))
    rows = [dict(row) for row in rows]
    def maps(asset, **channels):
        return {key:(EXTERNAL/asset/file).relative_to(SOURCE).as_posix()
                for key,file in channels.items()}
    tile = maps('tiled_floor_001',color='tiled_floor_001_diffuse_2k.jpg',
                normal='tiled_floor_001_nor_gl_2k.jpg',rough='tiled_floor_001_rough_2k.jpg')
    wood = maps('wood_table_001',color='wood_table_001_diff_2k.jpg',
                normal='wood_table_001_nor_gl_2k.jpg',rough='wood_table_001_rough_2k.jpg')
    metal = maps('Metal009',color='Metal009_2K-JPG_Color.jpg',normal='Metal009_2K-JPG_NormalGL.jpg',
                 rough='Metal009_2K-JPG_Roughness.jpg',metal='Metal009_2K-JPG_Metalness.jpg')
    overrides = {
        'tile_grey':dict(pbr_maps=tile,tile_cm=150.,normal_strength=.65,paint_soffit=True),
        'tile_ivory':dict(pbr_maps=tile,tile_cm=150.,normal_strength=.5,color_tint=[1.15,1.15,1.12]),
        'wood_light':dict(pbr_maps=wood,tile_cm=120.,normal_strength=.5),
        'wood_dark':dict(pbr_maps=wood,tile_cm=120.,normal_strength=.5,color_tint=[.42,.32,.25]),
        'metal_brushed':dict(pbr_maps=metal,tile_cm=80.,normal_strength=.35,rough_min=.25),
    }
    # Existing verified painted plaster scan, now used as a real surface instead
    # of a 35% overlay on flat color. Keep green paint an intentional tint.
    plaster = {c:'External/PolyHaven/painted_plaster_wall_'+s+'_2k.jpg'
               for c,s in [('color','diff'),('rough','rough'),('normal','nor_gl')]}
    overrides['plaster'] = dict(pbr_maps=plaster,tile_cm=200.,normal_strength=.25,
                               color_tint=[1.0,1.0,.96])
    overrides['paint_green'] = dict(pbr_maps=plaster,tile_cm=200.,normal_strength=.25,
                                   color_tint=[.38,.55,.43])
    for row in rows:
        row.update(overrides.get(row['key'],{}))
    for asset in ('food_apple_01','food_ginger_01','wooden_cutting_board'):
        folder = EXTERNAL/asset
        gltf = json.loads((folder/(asset+'_1k.gltf')).read_text())
        mapping = {}
        for index, definition in enumerate(gltf['materials']):
            name = 'M_IF_PH'+''.join(x.title() for x in asset.split('_'))+str(index)
            mapping[definition['name']] = name
            pbr = definition.get('pbrMetallicRoughness',{})
            channels = {}
            for channel,info in [('color',pbr.get('baseColorTexture')),
                                 ('normal',definition.get('normalTexture')),
                                 ('arm',pbr.get('metallicRoughnessTexture'))]:
                if info:
                    uri = gltf['images'][gltf['textures'][info['index']]['source']]['uri']
                    channels[channel] = (folder/uri).relative_to(SOURCE).as_posix()
            rows.append(dict(key='ph_'+asset+'_'+str(index),name=name,kind='plain',
                color=pbr.get('baseColorFactor',[1,1,1,1])[:3],
                roughness=pbr.get('roughnessFactor',1),metallic=pbr.get('metallicFactor',1),
                tile_cm=100.,emission=0,uv_mode='authored',pbr_maps=channels,normal_strength=1,
                source='https://polyhaven.com/a/'+asset,license='CC0-1.0'))
        MODEL_MATERIALS[asset] = mapping
    rows.append(dict(key='roast_skin',name='M_IF_RoastSkin',kind='plain',
        color=[.20,.052,.012],roughness=.32,metallic=0,tile_cm=40.,emission=0,
        procedural_skin=True,source='Original CityCourier procedural roasted-skin material'))
    rows.append(dict(key='roast_char',name='M_IF_RoastChar',kind='plain',
        color=[.045,.009,.003],roughness=.43,metallic=0,tile_cm=40.,emission=0))
    return rows


def bounds(objects):
    bpy.context.view_layer.update()
    points = [obj.matrix_world@Vector(p) for obj in objects for p in obj.bound_box]
    return [min(p[i] for p in points) for i in range(3)], [max(p[i] for p in points) for i in range(3)]


def imported(asset, center, maximum_size, mats, yaw=0, lay_flat=False):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(EXTERNAL/asset/(asset+'_1k.gltf')))
    objects = [o for o in set(bpy.data.objects)-before if o.type=='MESH']
    assert objects, asset
    if lay_flat:
        for obj in objects:obj.matrix_world=Matrix.Rotation(math.pi/2,4,'Y')@obj.matrix_world
    lo,hi = bounds(objects)
    scale = min(maximum_size[i]/max(hi[i]-lo[i],1e-6) for i in range(3))
    offset = Vector(((lo[0]+hi[0])/2,(lo[1]+hi[1])/2,lo[2]))
    transform = Matrix.Translation(Vector(center)) @ Matrix.Rotation(yaw,4,'Z') @ Matrix.Diagonal((scale,scale,scale,1)) @ Matrix.Translation(-offset)
    for obj in objects:
        obj.matrix_world = transform@obj.matrix_world
        obj.name = 'CC0_'+asset
        for slot in obj.material_slots:
            # Blender may append .001 on repeat imports. Map by source prefix.
            original = re.sub(r'\.\d{3}$','',slot.material.name)
            target = MODEL_MATERIALS[asset].get(original)
            if not target:
                raise RuntimeError('Unknown scanned material '+original)
            slot.material = next(m for m in mats.values() if m.name==target)
        triangles = sum(len(f.vertices)-2 for f in obj.data.polygons)
        if triangles>3500:
            bpy.context.view_layer.objects.active=obj
            modifier=obj.modifiers.new('Small-prop triangle budget','DECIMATE')
            modifier.ratio=3500/triangles
            bpy.ops.object.modifier_apply(modifier=modifier.name)
    # The importer also creates collection/empty nodes. Bake world transforms
    # before removing parents, preserving UVs and each model's relative pieces.
    for obj in objects:
        matrix=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=matrix
    for obj in set(bpy.data.objects)-before-set(objects):
        bpy.data.objects.remove(obj,do_unlink=True)
    return objects


def _duck(x, y, bottom, size, angle, m):
    before=set(bpy.data.objects)
    # Anatomical silhouette: breast, shoulder, curved neck/head, folded wings,
    # drumsticks and narrow lower legs. All surfaces have smooth normals.
    sphere('Duck_breast',(0,0,.245),(.092,.073,.157),m['roast_skin'])
    sphere('Duck_shoulders',(0,.012,.345),(.078,.065,.083),m['roast_skin'])
    sphere('Duck_tail',(0,.038,.105),(.041,.045,.056),m['roast_char'])
    _tube('Duck_curved_neck',[(0,.012,.37),(.006,.012,.43),(.018,-.005,.485)],.024,m['roast_skin'])
    sphere('Duck_head',(.018,-.012,.494),(.033,.039,.030),m['roast_skin'])
    sphere('Duck_beak',(.018,-.057,.485),(.021,.034,.010),m['roast_char'])
    for side in (-1,1):
        wing=sphere('Duck_folded_wing',(side*.071,-.035,.235),(.027,.023,.111),m['roast_skin'])
        wing.rotation_euler.y=side*.24
        sphere('Duck_wing_tip',(side*.057,-.046,.14),(.020,.014,.043),m['roast_char'])
        sphere('Duck_drumstick',(side*.052,.002,.125),(.039,.039,.065),m['roast_skin'])
        _tube('Duck_lower_leg',[(side*.052,.002,.105),(side*.063,-.012,.057),(side*.057,-.020,.027)],.013,m['roast_char'])
        sphere('Duck_eye',(.018+side*.026,-.031,.501),(.0025,.003,.0025),m['rubber'])
    # Organic parts must form one smooth surface. Leaving intersecting ovals
    # creates visible shoulder/breast seams even with physically correct skin.
    skin=[o for o in set(bpy.data.objects)-before if o.type=='MESH' and
          any(s.material==m['roast_skin'] for s in o.material_slots)]
    body=join(skin,'Duck_continuous_skin')
    bpy.context.view_layer.objects.active=body
    remesh=body.modifiers.new('Continuous anatomical surface','REMESH')
    remesh.mode='VOXEL';remesh.voxel_size=.0035;remesh.use_smooth_shade=True
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    smooth=body.modifiers.new('Cooked skin transitions','SMOOTH');smooth.factor=.75;smooth.iterations=4
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    triangles=sum(len(p.vertices)-2 for p in body.data.polygons)
    if triangles>5500:
        modifier=body.modifiers.new('Hanging food budget','DECIMATE');modifier.ratio=5500/triangles
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in body.data.polygons:polygon.use_smooth=True
    transform=Matrix.Translation(Vector((x,y,bottom)))@Matrix.Rotation(angle,4,'Z')@Matrix.Diagonal((size,size,size,1))
    objects=list(set(bpy.data.objects)-before)
    for obj in objects:obj.matrix_world=transform@obj.matrix_world
    return objects


def enrich(kind, kept, mats):
    """Stay within the original blocking envelope, never add floor clutter."""
    lo,hi=bounds(kept)
    original=set(bpy.data.objects)
    used=[]
    def top(prefix):
        objects=[o for o in kept if o.name.startswith(prefix)]
        a,b=bounds(objects)
        return [(a[i]+b[i])/2 for i in range(2)]+[b[2]]
    if kind=='ShopRoastDisplay':
        for obj in list(kept):
            if obj.name.startswith(('Roast_display','Roast_wing','Butcher_hook')):
                kept.remove(obj);bpy.data.objects.remove(obj,do_unlink=True)
        surface=top('Roast_worktop')
        for i,(x,scale,rotation) in enumerate([(-.46,1.02,-.12),(-.03,.94,.09),(.37,.99,-.06)]):
            birds=_duck(x,.035,surface[2]+.075,scale,rotation,mats)
            a,b=bounds(birds)
            _tube('Duck_s_hook',[(x,.035,b[2]-.014),(x,.035,1.89),(x+.022,.035,1.91)],.004,mats['metal_brushed'])
        used+=['original_anatomical_hanging_ducks','wooden_cutting_board']
        kept+=imported('wooden_cutting_board',[-.42,-.075,surface[2]+.002],[.34,.20,.023],mats)
        box('Food_case_service_hatch',(-.66,-.205,.63),(.10,.016,.27),mats['metal_brushed'],.006)
        for x in (-.29,.29):
            box('Roast_cabinet_panel',(x,-.205,.48),(.54,.014,.82),mats['metal_brushed'],.008)
            cylinder('Cabinet_pull',(x-.07,-.215,.81),(x+.07,-.215,.81),.007,mats['metal_dark'],10)
    elif kind=='ShopNoodleCooking':
        surface=top('Stainless_worktop')
        kept+=imported('wooden_cutting_board',[-.48,-.095,surface[2]+.003],[.28,.17,.022],mats)
        kept+=imported('food_ginger_01',[-.47,-.095,surface[2]+.024],[.16,.11,.055],mats,lay_flat=True)
        used+=['wooden_cutting_board','food_ginger_01']
    elif kind=='ShopGroceryCheckout':
        surface=top('Checkout_worktop')
        box('Produce_tray',(.32,-.025,surface[2]+.012),(.41,.29,.024),mats['wood_light'],.008)
        for x,y in ((.20,-.09),(.31,-.09),(.42,-.09),(.25,.02),(.37,.02)):
            kept+=imported('food_apple_01',[x,y,surface[2]+.025],[.085,.085,.09],mats,yaw=x*6)
        used+=['food_apple_01']
    elif kind.endswith('Dining'):
        surface=top('Cafe_tabletop')
        x,y,z=surface
        for xx in (x-.13,x+.13):
            _oval_profile('Ceramic_rice_bowl',(xx,y-.07),
                [(.025,.025,z+.002),(.031,.031,z+.012),(.073,.073,z+.067),
                 (.071,.071,z+.073),(.067,.067,z+.065),(.024,.024,z+.015)],mats['ceramic'],32)
        # One small condiment bottle, no taller than the retained chopsticks.
        cylinder('Soy_bottle',(x,y+.10,z+.003),(x,y+.10,z+.12),.022,mats['wood_dark'],16)
        cylinder('Soy_cap',(x,y+.10,z+.12),(x,y+.10,z+.145),.014,mats['accent_red'],16)
        used+=['original_ceramic_tableware']
    # Include added original geometry, but imported meshes were already added.
    for obj in set(bpy.data.objects)-original:
        if obj.type=='MESH' and obj not in kept:kept.append(obj)
    a,b=bounds(kept)
    if any(a[i]<lo[i]-.0001 or b[i]>hi[i]+.0001 for i in range(3)):
        raise RuntimeError('Realistic props escaped original blocking envelope: '+kind+' '+str((lo,hi,a,b)))
    return kept,used
