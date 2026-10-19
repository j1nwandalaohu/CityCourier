"""Extract retained storefront art as room-sized, blocking native assemblies.

Uses the original noodle/grocery/roast source geometry. No facade, floor,
fake door or light is added. Existing interior assets/palette are untouched.
Run with Blender 4.5.3 --background --python-exit-code 1 --python this_file.py.
"""
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/art'))
import build_interior_finish as kit
import storefront_detail as source
from art_common import MATERIALS, reset


def export_shop(obj, name):
    """The legacy FBX importer mirrors glyph X; compensate only at export.

    Keep editable Blender furniture readable and preserve its authored -Y
    serving face. The generic kit's adapter already handles the Y conversion.
    """
    original = obj.data
    temporary = original.copy()
    reflection = Matrix.Diagonal((-1.,1.,1.,1.))
    normals = {}
    if temporary.has_custom_normals:
        for face in temporary.polygons:
            for loop in face.loop_indices:
                normals[(face.index,temporary.loops[loop].vertex_index)] = reflection.to_3x3()@temporary.corner_normals[loop].vector
    try:
        obj.data = temporary
        temporary.transform(reflection)
        temporary.flip_normals()
        if normals:
            values=[None]*len(temporary.loops)
            for face in temporary.polygons:
                for loop in face.loop_indices:
                    values[loop]=normals[(face.index,temporary.loops[loop].vertex_index)]
            temporary.normals_split_custom_set(values)
        original_export(obj,name)
    finally:
        obj.data = original
        bpy.data.meshes.remove(temporary)


original_export = kit.export_canonical


def selected(kind, obj):
    n = obj.name
    corners = [obj.matrix_world@Vector(c) for c in obj.bound_box]
    center_x = (min(p.x for p in corners)+max(p.x for p in corners))*.5
    if kind == 'ShopNoodleCooking':
        return n.startswith(('Noodle_service_counter','Stainless_worktop','Soup_pot',
            'Pot_rim','Kitchen_tile','Kitchen_extract_hood','Hood_duct','Wall_menu',
            'Noodle_menu','Menu_item'))
    if kind == 'ShopGroceryCheckout':
        return n.startswith(('Checkout_','Till_','Cardboard_stock_carton','Carton_tape'))
    if kind == 'ShopGroceryShelf':
        return n.startswith(('Fridge_','Drink_')) or (
            n.startswith(('Shop_shelf','Grocery_package','Package_label')) and center_x < .2)
    if kind == 'ShopRoastDisplay':
        return n.startswith(('Roast_counter','Roast_worktop','Food_case','Roast_hanging',
            'Butcher_hook','Roast_display','Roast_wing','Chopping_block'))
    # One genuine dining table with stools, napkin box and chopstick holder;
    # do not compress an entire row of tables into a single small-room prop.
    if kind.endswith('Dining'):
        return n.startswith(('Cafe_table','Chopstick','Napkin','Stackable_stool','Stool_')) and (
            center_x < -1.7 if kind=='ShopNoodleDining' else .3 < center_x < 1.8)
    raise ValueError(kind)


def build():
    # Full-kit authoring invokes this in the same Blender process; factory
    # reset invalidates cached material datablocks from the preceding assets.
    MATERIALS.clear()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    kit.ASSETS.clear()
    kit.PALETTE.clear()
    m = kit.palette()
    # Surface upgrades retain existing material identities so all rooms receive
    # them; new UV-mapped scan slots are included in the normal import contract.
    (kit.OUT/'palette.json').write_text(json.dumps({'materials':kit.PALETTE},indent=2)+'\n',encoding='utf-8')
    original = dict(cream=m['laminate_white'], white=m['ceramic'], aluminum=m['metal_brushed'],
        metal=m['metal_dark'], red=m['accent_red'], green=m['paint_green'],
        yellow=m['carton'], wood=m['wood_light'], dark=m['rubber'], concrete=m['tile_grey'])
    names = [('ShopNoodleCooking',source.noodle_shop),('ShopNoodleDining',source.noodle_shop),
             ('ShopGroceryCheckout',source.convenience_shop),('ShopGroceryShelf',source.convenience_shop),
             ('ShopRoastDisplay',source.roast_shop),('ShopRoastDining',source.roast_shop)]
    for kind, author in names:
        reset()
        author(original, -5.76)
        bpy.context.view_layer.update()
        kept = []
        provenance = []
        for obj in list(bpy.data.objects):
            if not selected(kind,obj):
                bpy.data.objects.remove(obj,do_unlink=True)
                continue
            provenance.append(obj.name)
            if kind=='ShopGroceryShelf':
                # Repack one refrigerator and one stocked shelf side by side;
                # avoid shrinking a five-metre row into miniature appliances.
                fridge = obj.name.startswith(('Fridge_','Drink_'))
                sx = .60 if fridge else .28
                tx = 3.06*sx-.43 if fridge else 1.25*sx+.37
                obj.matrix_world = Matrix.Translation(Vector((tx,0 if fridge else -.23,0))) @ Matrix.Diagonal((sx,1,1,1)) @ obj.matrix_world
            # Move the original wall menu above the compact cooking station.
            if kind=='ShopNoodleCooking' and obj.name.startswith(('Wall_menu','Noodle_menu','Menu_item')):
                obj.location.x += 4.06
            kept.append(obj)
        assert kept, kind
        if kind.endswith('Dining'):
            assert any(n.startswith('Cafe_table_leg') for n in provenance)
            assert any(n.startswith('Stool_leg') for n in provenance)
        bpy.context.view_layer.update()
        points = [obj.matrix_world@Vector(c) for obj in kept for c in obj.bound_box]
        lo = [min(p[i] for p in points) for i in range(3)]
        hi = [max(p[i] for p in points) for i in range(3)]
        # Keep actual furniture heights; the old shallow storefront stood on
        # a 40cm display plinth. XY is adapted once at source, not per building.
        width, depth = (1.05,.65) if kind.endswith('Dining') else (1.50,.45)
        sx, sy = width/(hi[0]-lo[0]), depth/(hi[1]-lo[1])
        transform = Matrix.Translation(Vector((-(lo[0]+hi[0])*.5*sx,
            -(lo[1]+hi[1])*.5*sy,-lo[2]))) @ Matrix.Diagonal((sx,sy,1,1))
        for obj in kept:
            obj.matrix_world = transform@obj.matrix_world
        realistic_sources=[]
        if (kit.SOURCE/'External/RealisticInteriors/sources.json').is_file():
            from realistic_shop_props import enrich
            kept,realistic_sources=enrich(kind,kept,m)
        kit.export_canonical = export_shop
        try:
            kit.finish_asset('SM_IF_'+kind,kept,'BlockAll',family='furniture',kind=kind,
                source='tools/art/storefront_detail.py',source_components=sorted(provenance),
                realistic_sources=realistic_sources,
                export_axis_correction='legacy FBX glyph X reflection; authored serving front retained')
        finally:
            kit.export_canonical = original_export
    path = kit.OUT/'asset-manifest.json'
    manifest = json.loads(path.read_text())
    new = {a['name'] for a in kit.ASSETS}
    manifest['assets'] = [a for a in manifest['assets'] if a['name'] not in new]+kit.ASSETS
    manifest['shop_generator_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    manifest['storefront_source_sha256'] = hashlib.sha256((ROOT/'tools/art/storefront_detail.py').read_bytes()).hexdigest()
    path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('SHOP FURNISHINGS:',len(kit.ASSETS),'retained-source assemblies',flush=True)


if __name__=='__main__':
    build()
