"""Author hollow facades, hinged doors and reusable furnished-room assets.

Run using the installed Blender. Original building/character assets are never
opened or overwritten; exterior detail is rebuilt through the established art
pipeline, with the obsolete solid cores and shallow shop sets removed.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bpy
from mathutils import Vector
import build_environment as environment
from art_common import reset, box, cylinder, text_mesh, join, export, save, SOURCE

MANIFEST = []
ORIGINAL_JOIN = environment.join
REMOVE = ("building_upper_core", "building_lower_rear_core", "ground_plinth", "shop_back_wall",
          "shop_side_return", "shop_ceiling", "shop_tile_floor", "ceiling_light", "fluorescent",
          "roof_coping", "roof_parapet", "roof_stairwell", "stairwell_roof", "entrance_nosing")


def hollow_join(objects, name):
    if not name.startswith('SM_ShopHouse'):
        return ORIGINAL_JOIN(objects, name)
    kept, removed = [], []
    for obj in objects:
        name_lower = obj.name.lower()
        bounds = [obj.matrix_world @ Vector(p) for p in obj.bound_box]
        lo = Vector(tuple(min(v[i] for v in bounds) for i in range(3)))
        hi = Vector(tuple(max(v[i] for v in bounds) for i in range(3)))
        center = (lo + hi) * .5
        solid_core = name_lower.startswith(REMOVE) or 'door' in name_lower
        shallow_inside = hi.z < 3.95 and abs(center.x) < 3.92 and center.y > -5.54
        main_aperture = lo.z < 2.65 and hi.z > .4 and lo.x < .75 and hi.x > -.75 and center.y < -5.35
        if solid_core or shallow_inside or main_aperture:
            removed.append(obj.name)
            if obj == objects[0]:
                # Keep an empty, unrotated anchor so joining cannot inherit the
                # local axis of a randomly chosen window or trim component.
                obj.data = bpy.data.meshes.new('Empty_hollow_shell_anchor')
                kept.append(obj)
            else:
                bpy.data.objects.remove(obj, do_unlink=True)
        else:
            kept.append(obj)
    if not any(n.lower().startswith('building_upper_core') for n in removed):
        raise RuntimeError('Expected solid core was not removed; refusing to export')
    result = ORIGINAL_JOIN(kept, name.replace('SM_ShopHouse', 'SM_EnterableShopHouse'))
    MANIFEST.append(dict(name=result.name, removed=removed, retained_components=len(kept),
                         source='tools/art/build_environment.py', hollow=True))
    return result


def finish(name, start):
    objects = sorted(set(bpy.data.objects) - start, key=lambda o: o.name)
    # A clean, unrotated anchor establishes a repeatable local axis and pivot.
    anchor = bpy.data.objects.new('Asset_anchor', bpy.data.meshes.new('Asset_anchor_mesh'))
    bpy.context.collection.objects.link(anchor)
    obj = join([anchor] + objects, name)
    export(name, [obj], 'Buildings')
    save(name)
    MANIFEST.append(dict(name=name, author='project procedural room kit', source=__file__.split('/')[-1]))
    return obj


def door(m):
    reset(); start = set(bpy.data.objects)
    box('Door_leaf', (.5, 0, 1.1), (1, .045, 2.2), m['wood'], .008)
    for x in [.035, .965]:
        box('Door_stile', (x, -.027, 1.1), (.035, .012, 2.13), m['cream'], .004)
    for z in [.04, .83, 2.15]:
        box('Door_panel_moulding', (.5, -.027, z), (.9, .012, .025), m['cream'], .003)
    for side in [-1, 1]:
        cylinder('Handle_plate', (.86, side*.025, .96), (.86, side*.038, .96), .035, m['metal'], 16)
        cylinder('Handle_spindle', (.86, side*.032, .96), (.86, side*.07, .96), .013, m['aluminum'], 12)
        cylinder('Handle_lever', (.86, side*.07, .96), (.74, side*.07, .96), .013, m['aluminum'], 12)
    for z in [.23, 1.1, 1.97]:
        cylinder('Door_hinge', (0, .022, z-.045), (0, .022, z+.045), .012, m['metal'], 12)
    finish('SM_CourierDoor', start)


def annex(m):
    reset(); start = set(bpy.data.objects)
    # The actual 6.5 x 5m station shell is supplied by canonical structure.
    # This mesh provides front detailing without a baked blocking wall.
    for x in [-3.18, 3.18]:
        box('Annex_corner', (x, -2.54, 1.85), (.12, .16, 3.7), m['cream'], .012)
    box('Annex_sign_case', (0, -2.59, 3.27), (6.15, .18, .62), m['green'], .02)
    text_mesh('Annex_sign', '骑手驿站', (0, -2.69, 3.07), .36, m['white'])
    box('Annex_canopy', (0, -2.82, 2.9), (5.9, .8, .08), m['aluminum'], .015)
    for x in [-2.2, 2.2]:
        box('Annex_notice_frame', (x, -2.54, 1.6), (.9, .06, 1.1), m['metal'], .008)
        box('Annex_notice', (x, -2.577, 1.6), (.82, .015, 1.02), m['cream'], .002)
    finish('SM_EnterableAnnex', start)


def furniture(m, kind):
    reset(); start = set(bpy.data.objects)
    if kind == 'Bed':
        box('Bed_frame', (0, 0, .27), (.70, 1.70, .14), m['wood'], .018)
        for x in [-.28, .28]:
            for y in [-.73, .73]:
                box('Bed_leg', (x, y, .12), (.07, .07, .24), m['wood'], .008)
        box('Mattress', (0, 0, .40), (.68, 1.65, .16), m['cream'], .035)
        box('Duvet', (0, -.18, .49), (.67, 1.22, .06), m['green'], .025)
        box('Pillow', (0, .60, .51), (.48, .31, .10), m['white'], .035)
        box('Headboard', (0, .82, .59), (.70, .05, .75), m['wood'], .015)
    elif kind == 'Desk':
        box('Desktop', (0, 0, .76), (.95, .44, .045), m['wood'], .012)
        for x in [-.4, .4]:
            for y in [-.16, .16]:
                box('Desk_leg', (x, y, .38), (.035, .035, .76), m['metal'], .004)
        box('Monitor_stand', (.16, .04, .80), (.18, .12, .035), m['metal'], .004)
        box('Monitor_neck', (.16, .07, .91), (.035, .035, .22), m['metal'], .003)
        box('Monitor_frame', (.16, .075, 1.07), (.43, .035, .28), m['metal'], .008)
        box('Monitor_screen', (.16, .054, 1.07), (.39, .005, .235), m['glass'], .002)
        box('Keyboard', (.13, -.10, .795), (.33, .11, .02), m['cream'], .004)
    elif kind in ('Kitchen', 'Counter', 'Shelf'):
        width, depth = 1.20, (.35 if kind == 'Shelf' else .44)
        if kind == 'Shelf':
            for x in [-.57, .57]:
                box('Shelf_side', (x, 0, .85), (.035, depth, 1.7), m['wood'], .005)
            for level in range(5):
                z = .08 + level*.39
                box('Shelf', (0, 0, z), (width, depth, .035), m['wood'], .004)
                for book in range(5):
                    box('Stored_box', (-.45+book*.21, 0, z+.14), (.16, .25, .25), m['cream'] if book%2 else m['red'], .007)
        else:
            box('Counter_base', (0, 0, .43), (width, depth, .84), m['cream'], .012)
            box('Counter_top', (0, 0, .87), (width+.02, depth+.02, .045), m['aluminum'], .009)
            for x in [-.3, .3]:
                box('Cabinet_front', (x, -depth/2-.012, .43), (.56, .018, .73), m['wood'], .006)
                cylinder('Cabinet_pull', (x+.15, -depth/2-.032, .58), (x+.15, -depth/2-.032, .69), .008, m['metal'], 10)
            if kind == 'Kitchen':
                box('Sink', (-.28, 0, .90), (.42, .30, .035), m['metal'], .012)
                cylinder('Tap', (-.28, .12, .9), (-.28, .12, 1.14), .014, m['aluminum'], 12)
                cylinder('Tap_outlet', (-.28, .12, 1.14), (-.28, 0, 1.14), .014, m['aluminum'], 12)
                for x in [.24, .48]:
                    cylinder('Stove_ring', (x, 0, .90), (x, 0, .92), .08, m['metal'], 16)
            else:
                box('Pickup_tray', (.20, 0, .92), (.38, .29, .055), m['metal'], .008)
                for x in [.10, .27]:
                    box('Takeout_box', (x, 0, 1.01), (.13, .15, .13), m['cream'], .007)
    elif kind == 'Toilet':
        box('Toilet_foot', (0, -.04, .15), (.31, .45, .30), m['white'], .04)
        box('Toilet_bowl', (0, -.10, .34), (.42, .64, .18), m['white'], .055)
        box('Seat', (0, -.10, .45), (.39, .58, .045), m['cream'], .045)
        box('Cistern', (0, .20, .56), (.40, .18, .43), m['white'], .025)
        box('Flush', (.10, .20, .79), (.05, .045, .012), m['aluminum'], .004)
    elif kind == 'Sofa':
        box('Sofa_base', (0, 0, .24), (1.20, .50, .30), m['wood'], .02)
        box('Sofa_cushion', (0, -.03, .44), (1.12, .44, .16), m['green'], .035)
        box('Sofa_back', (0, .20, .62), (1.20, .10, .62), m['green'], .025)
        for x in [-.55, .55]:
            box('Sofa_arm', (x, 0, .51), (.10, .50, .30), m['green'], .02)
        for x in [-.48, .48]:
            for y in [-.17, .17]:
                box('Sofa_leg', (x, y, .06), (.05, .05, .12), m['metal'], .004)
    else:
        raise ValueError(kind)
    finish('SM_Interior' + kind, start)


def main():
    m = environment.palette()
    environment.join = hollow_join
    for variant in range(3):
        reset()
        obj = environment.building(m, variant)
        export(obj.name, [obj], 'Buildings')
        save(obj.name)
    environment.join = ORIGINAL_JOIN
    annex(m)
    door(m)
    for kind in ['Bed', 'Desk', 'Kitchen', 'Toilet', 'Shelf', 'Sofa', 'Counter']:
        furniture(m, kind)
    (SOURCE/'Interiors').mkdir(exist_ok=True)
    (SOURCE/'Interiors/asset-manifest.json').write_text(json.dumps(dict(schema_version=1, assets=MANIFEST), ensure_ascii=False, indent=2), encoding='utf-8')
    print('ENTERABLE ASSETS COMPLETE', len(MANIFEST), flush=True)


if __name__ == '__main__':
    main()
