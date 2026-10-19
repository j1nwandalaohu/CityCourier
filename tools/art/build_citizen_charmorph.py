"""Ordinary clothed NPC from retained CC0 portrait sources, on an independent rig."""
import collections
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'AssetsSource/CityCourier'
OUT = SOURCE/'Ambient/CharMorph'
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(Path(__file__).parent))
from build_photoreal_rider import subset

bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPhotoreal.blend'))
rig, original = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier']
rig.animation_data_clear()
for bone in rig.pose.bones:
    bone.matrix_basis = Matrix.Identity(4)
rest = [(b.name, b.parent.name if b.parent else None, [list(r) for r in b.matrix_local]) for b in rig.data.bones]
# Hair strands are disconnected tubes. Retain whole, uniformly distributed
# strands, preserving brows/lashes near the face rather than collapsing tubes.
hair_faces = [p for p in original.data.polygons if original.data.materials[p.material_index].name == 'M_PortraitHair']
adjacency = collections.defaultdict(set)
for face in hair_faces:
    for a, b in zip(face.vertices, list(face.vertices[1:])+[face.vertices[0]]):
        adjacency[a].add(b)
        adjacency[b].add(a)
component, number = {}, 0
for start in sorted(adjacency):
    if start in component:
        continue
    todo = [start]
    while todo:
        v = todo.pop()
        if v in component:
            continue
        component[v] = number
        todo.extend(n for n in adjacency[v] if n not in component)
    number += 1
selected = []
for face in original.data.polygons:
    name = original.data.materials[face.material_index].name
    if name in ('M_Helmet', 'M_Rubber', 'M_ReflectiveStrip'):
        continue
    if name == 'M_PortraitHair':
        # The skin shader also retains the existing short-hair scalp mask.
        if component[face.vertices[0]] % 6:
            continue
    selected.append(face.index)
mesh = subset(original, 'SK_CitizenCharMorph', selected)
print('CITIZEN selected faces', len(selected), 'hair components', number, flush=True)
# Preserve the neck transfer UV and portrait vertex masks used by UE materials.
for layer in list(original.data.uv_layers)[1:]:
    uv = mesh.data.uv_layers.new(name=layer.name)
    for dst, src_index in zip(mesh.data.polygons, selected):
        src = original.data.polygons[src_index]
        for a, b in zip(dst.loop_indices, src.loop_indices):
            uv.data[a].uv = layer.data[b].uv
ids = sorted({i for index in selected for i in original.data.polygons[index].vertices})
for layer in original.data.color_attributes:
    if layer.domain != 'POINT':
        continue
    colors = mesh.data.color_attributes.new(name=layer.name, type='FLOAT_COLOR', domain='POINT')
    for dst, src in zip(colors.data, ids):
        dst.color = layer.data[src].color
mesh.parent = rig
bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True)
bpy.context.view_layer.objects.active = mesh
reduce = mesh.modifiers.new('NPC geometry budget', 'DECIMATE')
reduce.ratio = .30
reduce.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier=reduce.name)
deform = mesh.modifiers.new('Independent citizen rig', 'ARMATURE')
deform.object = rig
for obj in list(bpy.data.objects):
    if obj not in (rig, mesh):
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.material_slot_remove_unused()
assert rest == [(b.name, b.parent.name if b.parent else None, [list(r) for r in b.matrix_local]) for b in rig.data.bones]
scene = bpy.context.scene
scene.unit_settings.scale_length = 1
scene.render.fps = 60
bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = mesh
fbx = OUT/'SK_CitizenCharMorph.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH', 'ARMATURE'},
    global_scale=1, apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
    bake_anim=False, use_mesh_modifiers=True, use_triangles=True, use_tspace=True,
    mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.file.make_paths_relative()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CitizenCharMorph.blend'))
report = {'head': 'CharMorph-Vitruvian c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44 CC0',
    'body_clothes': 'Retained MakeHuman CC0 casual suit and shoes',
    'helmet_removed': True, 'reflective_accessories_removed': True,
    'source_reference_unchanged': True, 'bones': len(rest),
    'vertices': len(mesh.data.vertices), 'triangles': sum(len(p.vertices)-2 for p in mesh.data.polygons),
    'materials': [m.name for m in mesh.data.materials], 'retained_hair_strands_fraction': '1/6',
    'fbx_sha256': hashlib.sha256(fbx.read_bytes()).hexdigest(),
    'limitations': ['One adult male appearance; no crowd face/age variations yet',
                     'Shared retained portrait materials; simplified strand geometry']}
(OUT/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
print('CITIZEN_CHARMORPH', json.dumps(report), flush=True)
