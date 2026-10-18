"""Build a clothed CC0 comparison bust, never touching CourierRider.blend.

blender -b --python tools/art/build_charmorph_probe.py
Optional -- --render uses CPU Cycles only. Original UDIM UVs are retained as UV1;
UV0 subtracts integer tile offsets only for conventional UE texture materials.
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'AssetsSource/CityCourier'
INPUT = SOURCE / 'External/CharMorphProbe'
OUTPUT = SOURCE / 'Exports/CharacterComparison'
TEX = OUTPUT / 'Textures'
CATALOG = {}


def pixels(path, size=2048):
    image = bpy.data.images.load(str(path), check_existing=False)
    image.colorspace_settings.name = 'Non-Color'  # upstream settings: all EXRs linear
    if tuple(image.size) != (size, size):
        image.scale(size, size)
    result = np.empty(size * size * 4, dtype=np.float32)
    image.pixels.foreach_get(result)
    bpy.data.images.remove(image)
    return result.reshape(size, size, 4)


def save_texture(name, data, srgb=False):
    path = TEX / (name + '.png')
    h, w = data.shape[:2]
    image = bpy.data.images.new(name, width=w, height=h, alpha=True, float_buffer=True)
    image.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    # Image pixels in Blender are linear; save() handles conversion to PNG sRGB.
    image.pixels.foreach_set(data.astype(np.float32).reshape(-1))
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)
    return path


def derived(prefix, color, roughness=None, height=None, size=2048, tint=None):
    paths = {}
    expected = {'base_color': TEX / (prefix + '_BaseColor.png')}
    if roughness:
        expected['roughness'] = TEX / (prefix + '_Roughness.png')
    if height:
        expected['normal'] = TEX / (prefix + '_NormalGL.png')
    if all(p.exists() for p in expected.values()) and prefix not in ('CharMorph_Sclera', 'CharMorph_Iris'):
        return expected
    rgba = pixels(INPUT / 'textures/4K' / color, size)
    if tint:
        # Retain authored iris detail while selecting a natural brown chroma.
        luminance = rgba[:, :, :3] @ np.array([.2126, .7152, .0722], dtype=np.float32)
        rgba[:, :, :3] = luminance[:, :, None] * np.array(tint, dtype=np.float32)
    # Verified Mix Shader socket indices: alpha=0 selects glass, alpha=1 skin.
    # CorneaCol vertex mask independently confirms the source alpha is opacity.
    if prefix != 'CharMorph_Sclera':
        rgba[:, :, 3] = 1
    paths['base_color'] = save_texture(prefix + '_BaseColor', rgba, True)
    del rgba
    if roughness:
        rgba = pixels(INPUT / 'textures/4K' / roughness, size)
        rgba[:, :, :3] = rgba[:, :, :1]
        rgba[:, :, 3] = 1
        paths['roughness'] = save_texture(prefix + '_Roughness', rgba)
        del rgba
    if height:
        rgba = pixels(INPUT / 'textures/4K' / height, size)
        h = rgba[:, :, 0].copy()
        dy, dx = np.gradient(h)
        # Tangent-space OpenGL (+Y) height derivative. Keep intensity modest.
        normal = np.stack((-dx * size * .006, -dy * size * .006, np.ones_like(h)), axis=2)
        normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-8)
        rgba[:, :, :3] = normal * .5 + .5
        rgba[:, :, 3] = 1
        paths['normal'] = save_texture(prefix + '_NormalGL', rgba)
    return paths


def material(name, color, roughness=.5, paths=None, sss=0, transmission=0, masked=False):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.diffuse_color = (*color, 1)
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['IOR'].default_value = 1.4
    bsdf.inputs['Subsurface Weight'].default_value = sss
    bsdf.inputs['Subsurface Radius'].default_value = (1.0, .35, .2)
    bsdf.inputs['Subsurface Scale'].default_value = .003
    bsdf.inputs['Transmission Weight'].default_value = transmission
    for channel, path in (paths or {}).items():
        node = mat.node_tree.nodes.new('ShaderNodeTexImage')
        node.image = bpy.data.images.load(str(path), check_existing=True)
        node.image.colorspace_settings.name = 'sRGB' if channel == 'base_color' else 'Non-Color'
        uv = mat.node_tree.nodes.new('ShaderNodeUVMap')
        uv.uv_map = 'UE_UV0'
        mat.node_tree.links.new(uv.outputs['UV'], node.inputs['Vector'])
        if channel == 'normal':
            normal = mat.node_tree.nodes.new('ShaderNodeNormalMap')
            normal.uv_map = 'UE_UV0'
            mat.node_tree.links.new(node.outputs['Color'], normal.inputs['Color'])
            mat.node_tree.links.new(normal.outputs['Normal'], bsdf.inputs['Normal'])
        else:
            mat.node_tree.links.new(node.outputs['Color'], bsdf.inputs['Base Color' if channel == 'base_color' else 'Roughness'])
            if masked and channel == 'base_color':
                mat.node_tree.links.new(node.outputs['Alpha'], bsdf.inputs['Alpha'])
                mat.surface_render_method = 'DITHERED'
    CATALOG[name] = dict(base_color=list(color), roughness=roughness, metallic=0,
                         textures={k: str(v.relative_to(SOURCE)).replace('\\', '/') for k, v in (paths or {}).items()},
                         texture_color_spaces={'base_color': 'sRGB', 'normal': 'linear', 'roughness': 'linear'},
                         normal=dict(space='tangent', convention='OpenGL +Y', unreal_flip_green=True,
                                     source='Finite differences from original CC0 scalar height; UV amplitude 0.006; approximation'),
                         alpha=dict(mode='masked' if masked else 'opaque', constant=1,
                                    texture_channel='base_color.A' if masked else None,
                                    clip_threshold=.5,
                                    source='upstream Sclera_Color EXR alpha directly; alpha=0 glass/cornea, alpha=1 opaque sclera, confirmed by Mix Shader input indices and CorneaCol' if masked else None),
                         subsurface=dict(weight=sss, radius_rgb=[1, .35, .2], scale_m=.003,
                                         unreal_shading_model='Subsurface' if sss else 'Default Lit',
                                         unreal_opacity=0.15 if sss else 0, texture=None),
                         transmission=transmission, uv_channel=0)
    return mat


def mesh_subset(name, source, coordinates, face_ids, materials, face_materials, scale, floor):
    faces = [source.polygons[i] for i in face_ids]
    ids = sorted({i for p in faces for i in p.vertices})
    remap = {old: new for new, old in enumerate(ids)}
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(coordinates[i][0]*scale, coordinates[i][1]*scale, (coordinates[i][2]-floor)*scale)
                      for i in ids], [], [[remap[i] for i in p.vertices] for p in faces])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for mat in materials:
        mesh.materials.append(mat)
    # Preserve all authored layers. UE_UV0 is first, original UDIM is second.
    original = source.uv_layers.get('VitruvianUV.UDIM') or source.uv_layers.active
    ue = mesh.uv_layers.new(name='UE_UV0')
    source_layers = [original] + [u for u in source.uv_layers if u != original]
    for layer in source_layers[:7]:
        out = mesh.uv_layers.new(name='Source_' + layer.name)
        for a, b in zip(mesh.polygons, faces):
            for x, y in zip(a.loop_indices, b.loop_indices):
                out.data[x].uv = layer.data[y].uv
    for a, b in zip(mesh.polygons, faces):
        a.material_index = face_materials[b.index]
        a.use_smooth = True
        center = sum((original.data[i].uv for i in b.loop_indices), Vector((0, 0))) / len(b.loop_indices)
        tile = Vector((math.floor(center.x), math.floor(center.y)))
        for x, y in zip(a.loop_indices, b.loop_indices):
            ue.data[x].uv = original.data[y].uv - tile
    mesh.uv_layers.active_index = 0
    ue.active_render = True
    return obj


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    TEX.mkdir(exist_ok=True)
    # Verify every source byte before authoring. The original source stays untouched.
    manifest = json.loads((INPUT / 'sources.json').read_text())
    for record in manifest['files']:
        if hashlib.sha256((INPUT / record['path']).read_bytes()).hexdigest() != record['sha256']:
            raise RuntimeError('Source SHA256 mismatch: ' + record['path'])
    bpy.ops.wm.open_mainfile(filepath=str(INPUT / 'char.blend'), use_scripts=False)
    if bpy.context.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    body = bpy.data.objects['cm_vitruvian']
    original = np.load(INPUT / 'morphs/L1/Default.npy').astype(np.float32)
    coordinates = original.copy()
    morphs = {'Gender_Male': 1.0, 'Race_EastAsian': 1.0}
    for name, weight in morphs.items():
        delta = np.load(INPUT / 'morphs/L2' / (name + '.npz'))
        coordinates[delta['idx']] += delta['delta'] * weight
    floor = float(coordinates[:, 2].min())
    scale = 1.76 / float(coordinates[:, 2].max() - floor)
    # Bust cutoff and skin cutoff ensure no unclothed body is saved or exported.
    skin_cut = 1.44
    bust_cut = 1.22
    keep = [p.index for p in body.data.polygons
            if p.material_index not in (5, 7) and min(coordinates[i][2] for i in p.vertices) > skin_cut]
    tile_values = sorted({math.floor(sum(body.data.uv_layers.active.data[i].uv.x for i in body.data.polygons[p].loop_indices)
                                     / len(body.data.polygons[p].loop_indices)) for p in keep if body.data.polygons[p].material_index == 4})
    mats = []
    tile_mats = {}
    for tile in tile_values:
        udim = 1001 + tile
        paths = derived(f'CharMorph_Skin_{udim}', f'Light_Skin_Color.{udim}.exr',
                        f'Skin_Roughness.{udim}.exr', f'Skin_Height.{udim}.exr',
                        size=4096 if udim == 1001 else 2048)
        tile_mats[tile] = len(mats)
        mats.append(material(f'M_CM_Skin_{udim}', (.55, .32, .22), paths=paths, sss=.22))
    source_mats = {}
    for index, label, base, rough, height, tint in [
            (0, 'Iris', 'Iris_Color.1007.exr', 'Iris_Roughness.1007.exr', 'Iris_Height.1007.exr', (.25, .09, .025)),
            (1, 'Mouth', 'Mouth_Color.1008.exr', 'Mouth_Roughness.1008.exr', 'Mouth_Height.1008.exr', None),
            (3, 'Sclera', 'Sclera_Color.1005.exr', None, 'Sclera_Height.1005.exr', None)]:
        source_mats[index] = len(mats)
        paths = derived('CharMorph_' + label, base, rough, height, tint=tint)
        mats.append(material('M_CM_' + label, (.5, .5, .5), roughness=.18 if index == 3 else .4, paths=paths, masked=index == 3))
    source_mats[2] = len(mats)
    mats.append(material('M_CM_Pupil', (.002, .001, .001), .2))
    mappings = {}
    for p in keep:
        face = body.data.polygons[p]
        if face.material_index == 4:
            tile = math.floor(sum(body.data.uv_layers.active.data[i].uv.x for i in face.loop_indices) / len(face.loop_indices))
            mappings[p] = tile_mats[tile]
        else:
            mappings[p] = source_mats[face.material_index]
    head = mesh_subset('CharMorph_HeadNeck', body.data, coordinates, keep, mats, mappings, scale, floor)
    # Fit original CC0 shirt using nearest original-body morph displacement.
    tree = KDTree(len(original))
    for i, point in enumerate(original):
        tree.insert(point, i)
    tree.balance()
    with bpy.data.libraries.load(str(INPUT / 'assets/Shirt.blend'), link=False) as (src, dst):
        dst.objects = ['Shirt']
    shirt = dst.objects[0]
    bpy.context.collection.objects.link(shirt)
    shirt_points = np.array([v.co[:] for v in shirt.data.vertices], dtype=np.float32)
    for i, v in enumerate(shirt.data.vertices):
        _, nearest, _ = tree.find(v.co)
        shirt_points[i] += coordinates[nearest] - original[nearest]
        shirt_points[i] += np.array(v.normal[:]) * .004
    shirt_m = material('M_CM_BlueShirt', (.025, .15, .32), .88)
    shirt_ids = [p.index for p in shirt.data.polygons if min(shirt_points[i][2] for i in p.vertices) > bust_cut]
    cloth = mesh_subset('CharMorph_BlueShirt', shirt.data, shirt_points, shirt_ids, [shirt_m],
                        {p.index: 0 for p in shirt.data.polygons}, scale, floor)
    # Convert original CC0 eyebrow strands to geometry, fitted to the same morphs.
    hair_data = np.load(INPUT / 'hairstyles/mind_eyebrows_11_Default.npz')
    curves = bpy.data.curves.new('CC0_OriginalEyebrowStrands', 'CURVE')
    curves.dimensions = '3D'
    curves.bevel_depth = .00012
    curves.bevel_resolution = 0
    offset = 0
    for count in hair_data['cnt']:
        count = int(count)
        strand = hair_data['data'][offset:offset+count]
        offset += count
        _, nearest, _ = tree.find(strand[0])
        displacement = coordinates[nearest] - original[nearest]
        points = strand[::4]
        spline = curves.splines.new('POLY')
        spline.points.add(len(points)-1)
        for p, co in zip(spline.points, points):
            transformed = (co + displacement) * scale
            transformed[2] -= floor * scale
            p.co = (*transformed, 1)
    brows = bpy.data.objects.new('CharMorph_SourceEyebrows', curves)
    bpy.context.collection.objects.link(brows)
    curves.materials.append(material('M_CM_DarkBrows', (.018, .009, .005), .7))
    bpy.ops.object.select_all(action='DESELECT')
    brows.select_set(True)
    bpy.context.view_layer.objects.active = brows
    bpy.ops.object.convert(target='MESH')
    brows = bpy.context.object
    parts = [head, cloth, brows]
    # Remove source body and all unrelated datablocks, including embedded addon UI texts.
    for obj in list(bpy.data.objects):
        if obj not in parts:
            bpy.data.objects.remove(obj, do_unlink=True)
    for text in list(bpy.data.texts):
        bpy.data.texts.remove(text)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = head
    bpy.ops.object.join()
    bust = bpy.context.object
    bust.name = 'SM_CharMorphBust'
    # UE supports eight UV channels. Keep UV0 plus all seven original body UVs.
    while len(bust.data.uv_layers) > 8:
        bust.data.uv_layers.remove(bust.data.uv_layers[-1])
    for mat in list(bpy.data.materials):
        if not mat.name.startswith('M_CM_'):
            bpy.data.materials.remove(mat, do_unlink=True)
    for group in bpy.data.node_groups:
        group.use_fake_user = False
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    bpy.ops.export_scene.fbx(filepath=str(OUTPUT / 'SM_CharMorphBust.fbx'), use_selection=True,
                            object_types={'MESH'}, global_scale=1, apply_unit_scale=True,
                            axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True,
                            mesh_smooth_type='FACE', bake_anim=False, path_mode='STRIP')
    report = dict(source_revision=manifest['revision'], license='CC0-1.0', measured_full_body_height_m=1.76,
                  source_shape='Vitruvian Default + Gender_Male 1 + Race_EastAsian 1',
                  phenotype_limit='Upstream EastAsian morph, not the same identity as MakeHuman young_asian_male; light skin color texture unchanged',
                  bust_cutoff_source_m=bust_cut, skin_cutoff_source_m=skin_cut,
                  units='meters in Blender; FBX unit metadata centimeters for UE',
                  object='SM_CharMorphBust', dimensions_m=list(bust.dimensions), vertices=len(bust.data.vertices),
                  triangles=sum(len(p.vertices)-2 for p in bust.data.polygons),
                  uv_layers=list(bust.data.uv_layers.keys()),
                  textures='Original source EXR bytes retained; 4K head and 2K other derived PNGs',
                  limitations=['Static look comparison; no gameplay rig or animation',
                               'Bald scalp; source eyebrow strand geometry retained; source eyelashes omitted',
                               'Skin uses original light color + roughness with derived tangent normal and explicit SSS, rather than full Blender procedural node graph',
                               'Source sclera/cornea alpha used directly as opacity mask; true corneal refraction and transparent tearline overlay omitted',
                               'Iris source luminance retains original detail, tinted natural brown [0.25,0.09,0.025] in linear RGB',
                               'Clothed bust crop, no unclothed full body saved or exported'],
                  materials=CATALOG)
    for mat in report['materials'].values():
        mat['texture_sha256'] = {k: hashlib.sha256((SOURCE / p).read_bytes()).hexdigest() for k,p in mat['textures'].items()}
    (SOURCE / 'character_comparison_materials.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    # Camera and lights are authoring-only; export already contains the static bust only.
    target = Vector((0, -.015, 1.57))
    bpy.ops.object.camera_add(location=(.55, -1.45, 1.63))
    camera = bpy.context.object
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type='ORTHO'
    camera.data.ortho_scale=.70
    scene.camera=camera
    for location, energy, size in [((-1,-2,2.5), 130, 1.4), ((1,-.5,2), 65, 1), ((0,1,2.3), 100, 1)]:
        bpy.ops.object.light_add(type='AREA', location=location)
        light=bpy.context.object
        light.data.energy=energy
        light.data.shape='DISK'
        light.data.size=size
        light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.new('CharMorphNeutralStudio')
    scene.world.use_nodes=True
    scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.12,.12,.12,1)
    scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.35
    scene.view_settings.view_transform='AgX'
    scene.render.engine='CYCLES'
    scene.cycles.device='CPU'
    scene.cycles.samples=16
    scene.cycles.use_denoising=True
    scene.render.resolution_x=640
    scene.render.resolution_y=800
    scene.render.resolution_percentage=100
    scene.render.filepath=str(OUTPUT / 'CharMorphBust_CPU.png')
    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE / 'Blender/CharMorphProbe.blend'))
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE / 'Blender/CharMorphProbe.blend'))
    print('CHARMORPH_REPORT', json.dumps({k:v for k,v in report.items() if k!='materials'}), flush=True)
    if '--render' in sys.argv:
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    main()
