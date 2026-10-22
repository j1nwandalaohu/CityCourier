"""Retarget retained library clips without synthesizing replacement body poses.

Run with Blender --background --python-exit-code 1 --python this_file.
Never saves a pre-existing blend, mesh, rig or source action.
"""
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_common import SOURCE, export
from build_motion_probe import MAPPING

ROOT = SOURCE.parents[1]
OUT = ROOT / 'Saved/Validation/LibraryActions'
PROFILES = {
    'A_IdleRelaxed': ('Idle_A', None, True),
    'A_Ride': ('Driving', None, True),
    'A_JumpStart': ('Jump_Start', .18, False),
    'A_JumpLoop': ('Jump_air', .4, True),
    'A_JumpFall': ('Jump_air', .2, False),
    'A_Land': ('Jump_Land', .25, False),
    'A_Knockdown': ('Death_D', None, False),
    'A_GetUp': ('LayToIdle', None, False),
}


def source_action(name):
    normalized = name.replace(' ', '_')
    matches = [a for a in bpy.data.actions if a.name.replace(' ', '_') in (normalized, normalized+'_Armature')]
    if len(matches) != 1:
        raise RuntimeError('Ambiguous or missing source action: '+name+'; available: '+repr([a.name for a in bpy.data.actions]))
    return matches[0]


def sample(rig, action, frame):
    rig.animation_data_create()
    rig.animation_data.action = action
    if getattr(action, 'slots', None):
        rig.animation_data.action_slot = action.slots[0]
    bpy.context.scene.frame_set(math.floor(frame), subframe=frame-math.floor(frame))
    return {b.name: b.matrix.copy() for b in rig.pose.bones}


def bake(rig, rest, name, poses):
    rig.animation_data_create()
    rig.animation_data_clear()
    rig.animation_data_create()
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    previous = {}
    for frame, pose in enumerate(poses, 1):
        bpy.context.scene.frame_set(frame)
        for bone in rig.data.bones:
            parent = bone.parent
            basis = pose[parent.name] @ rest[parent.name].inverted() @ rest[bone.name] if parent else rest[bone.name]
            pb = rig.pose.bones[bone.name]
            pb.rotation_mode = 'QUATERNION'
            pb.matrix_basis = basis.inverted() @ pose[bone.name]
            if bone.name in previous:
                pb.rotation_quaternion.make_compatible(previous[bone.name])
            previous[bone.name] = pb.rotation_quaternion.copy()
            pb.keyframe_insert('location', frame=frame)
            pb.keyframe_insert('rotation_quaternion', frame=frame)
    for curve in action.fcurves:
        for key in curve.keyframe_points:
            key.interpolation = 'LINEAR'
    return action


def preview(rig, mesh, name, poses, tiles):
    """Retain evaluated meshes so a reviewer sees the exported body poses."""
    for u in (0., .5, 1.):
        scene = bpy.context.scene
        scene.frame_set(1+round(u*(len(poses)-1)))
        bpy.context.view_layer.update()
        evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        obj = bpy.data.objects.new(name+'_'+str(u), bpy.data.meshes.new_from_object(evaluated))
        bpy.context.collection.objects.link(obj)
        tiles.append(obj)


def render_preview(mesh, tiles):
    scene = bpy.context.scene
    for obj in scene.objects:
        if obj.type == 'MESH':
            obj.hide_render = obj not in tiles
    for index, obj in enumerate(tiles):
        obj.location.x = (index % 6)*1.6
        obj.location.y = (index//6)*2.2
    camera_data = bpy.data.cameras.new('LibraryReviewCamera')
    camera = bpy.data.objects.new('LibraryReviewCamera', camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (12, -15, 15)
    camera.rotation_euler = (Vector((4, 4, .6))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type, camera_data.ortho_scale = 'ORTHO', 15
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light, scene.display.shading.color_type = 'STUDIO', 'MATERIAL'
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1600, 1600, 100
    scene.render.filepath = str(OUT/'library-contact-sheet.png')
    bpy.ops.render.render(write_still=True)


def build_hits(rig, rest, scene, report):
    """Use actual forward/backward recoil clips, without inventing a back pose."""
    folder = SOURCE/'External/CombatRecovery/Exports/Source'
    for direction in ('Front', 'Back'):
        path = folder/f'MM_HitReact_{direction}_Med_01.fbx'
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(path))
        imported = set(bpy.data.objects)-before
        source = next(o for o in imported if o.type == 'ARMATURE')
        action = source.animation_data.action
        sr = {b.name: b.matrix_local.copy() for b in source.data.bones}
        scale = sum(rig.data.bones[n+'_l'].length for n in ('thigh', 'shin'))/sum(source.data.bones[n+'_l'].length for n in ('thigh', 'calf'))
        align = {}
        for target, name in MAPPING.items():
            q = rest[target].to_quaternion()
            if target not in ('pelvis', 'spine', 'chest', 'neck', 'head') and not target.startswith('clavicle_'):
                a, b = q @ Vector((0, 1, 0)), sr[name].to_quaternion() @ Vector((0, 1, 0))
                if target.startswith('hand_'):
                    a = rest['finger_3_1_'+target[-1]].translation-rest[target].translation
                    b = sr['middle_01_'+target[-1]].translation-sr[name].translation
                q = a.normalized().rotation_difference(b.normalized()) @ q
            align[target] = sr[name].to_quaternion().inverted() @ q
        first, last = map(float, action.frame_range)
        poses = []
        for i in range(41):
            sp = sample(source, action, first+(last-first)*i/40)
            pose = {'root': rest['root'].copy()}
            for bone in rig.data.bones:
                n = bone.name
                if n == 'root':
                    continue
                m = (sp[MAPPING[n]].to_quaternion() @ align[n]).to_matrix().to_4x4()
                m.translation = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
                if n == 'pelvis':
                    m.translation = rest[n].translation+(sp['pelvis'].translation-sr['pelvis'].translation)*scale
                    m.translation.x, m.translation.y = rest[n].translation.x, rest[n].translation.y
                pose[n] = m
            poses.append(pose)
        name = 'A_HitReact_'+direction
        scene.render.fps = 100
        bake(rig, rest, name, poses)
        scene.frame_start, scene.frame_end = 1, 41
        scene.frame_set(1)
        export(name, [rig], 'LibraryActions', True)
        report['clips'][name] = {'source_action': path.stem, 'source_library': 'Epic Content Examples',
            'license': 'Fab/Epic content license; see retained sample provenance', 'source_fbx_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'duration_s': .4, 'loop': False, 'root_motion': False,
            'fbx_sha256': hashlib.sha256((SOURCE/'Exports/LibraryActions'/f'{name}.fbx').read_bytes()).hexdigest()}
        for obj in imported:
            bpy.data.objects.remove(obj, do_unlink=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPhotoreal.blend'))
    rig = bpy.data.objects['CourierRig']
    mesh = bpy.data.objects['SK_Courier']
    rig.animation_data_clear()
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    reference = [(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local))) for b in rig.data.bones]
    scene = bpy.context.scene
    scene.render.fps = 100
    data = (SOURCE/'External/Mesh2MotionProbe/static/animations/human-base-animations.glb').read_bytes()
    size, kind = struct.unpack_from('<II', data, 12)
    doc = json.loads(data[20:20+size])
    doc['animations'] = [a for a in doc['animations'] if a['name'] in {p[0] for p in PROFILES.values()}]
    payload = json.dumps(doc, separators=(',', ':')).encode()
    payload += b' '*((-len(payload)) % 4)
    tail = data[20+size:]
    path = OUT/'selected.glb'
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 20+len(payload)+len(tail))+struct.pack('<II', len(payload), kind)+payload+tail)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = set(bpy.data.objects)-before
    source = next(o for o in imported if o.type == 'ARMATURE')
    for obj in imported:
        if obj != source:
            bpy.data.objects.remove(obj, do_unlink=True)
    sr = {b.name: b.matrix_local.copy() for b in source.data.bones}
    scale = sum(rig.data.bones[n+'_l'].length for n in ('thigh', 'shin'))/sum(source.data.bones[n+'_l'].length for n in ('thigh', 'calf'))
    align = {}
    for target, name in MAPPING.items():
        q = rest[target].to_quaternion()
        if target not in ('pelvis', 'spine', 'chest', 'neck', 'head') and not target.startswith('clavicle_'):
            a, b = q @ Vector((0, 1, 0)), sr[name].to_quaternion() @ Vector((0, 1, 0))
            if target.startswith('hand_'):
                a = rest['finger_3_1_'+target[-1]].translation-rest[target].translation
                b = sr['middle_01_'+target[-1]].translation-sr[name].translation
            q = a.normalized().rotation_difference(b.normalized()) @ q
        align[target] = sr[name].to_quaternion().inverted() @ q
    report = {'upstream': 'https://github.com/Mesh2Motion/mesh2motion-app', 'license': 'CC0-1.0',
              'source_revision': '653a9698f5f315523072f1c3ff3496100401317b',
              'source_sha256': hashlib.sha256(data).hexdigest(), 'clips': {}, 'fps': 100}
    tiles = []
    if '--hits-only' in sys.argv:
        report = json.loads((SOURCE/'library_actions.json').read_text(encoding='utf-8'))
    for name, (source_name, duration, loop) in ([] if '--hits-only' in sys.argv else PROFILES.items()):
        action = source_action(source_name)
        first, last = map(float, action.frame_range)
        duration = duration or (last-first)/100
        count = round(duration*100)
        poses = []
        for i in range(count+1):
            u = i/count
            # Separate flight phases are time slices of the library air clip.
            if name == 'A_JumpLoop':
                u *= .65
            elif name == 'A_JumpFall':
                u = .65+.35*u
            sp = sample(source, action, first+(last-first)*u)
            pose = {'root': rest['root'].copy()}
            for bone in rig.data.bones:
                n = bone.name
                if n == 'root':
                    continue
                m = (sp[MAPPING[n]].to_quaternion() @ align[n]).to_matrix().to_4x4()
                m.translation = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
                if n == 'pelvis':
                    m.translation = rest[n].translation+(sp['pelvis'].translation-sr['pelvis'].translation)*scale
                    m.translation.x, m.translation.y = rest[n].translation.x, rest[n].translation.y
                    if name.startswith('A_Jump'):
                        m.translation.z = rest[n].translation.z
                pose[n] = m
            assert all(math.isfinite(v) for m in pose.values() for row in m for v in row)
            poses.append(pose)
        if loop:
            poses[-1] = {n: m.copy() for n, m in poses[0].items()}
        bake(rig, rest, name, poses)
        preview(rig, mesh, name, poses, tiles)
        scene.frame_start, scene.frame_end = 1, count+1
        scene.frame_set(1)
        export(name, [rig], 'LibraryActions', True)
        fbx = SOURCE/'Exports/LibraryActions'/f'{name}.fbx'
        report['clips'][name] = {'source_action': source_name, 'duration_s': count/100, 'loop': loop,
            'fbx_sha256': hashlib.sha256(fbx.read_bytes()).hexdigest(),
            'endpoint_heights_m': [{n: p[n].translation.z for n in ('pelvis', 'head')} for p in (poses[0], poses[-1])],
            'root_motion': False}
    build_hits(rig, rest, scene, report)
    assert reference == [(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local))) for b in rig.data.bones]
    report['skeleton_reference_unchanged'] = True
    (SOURCE/'library_actions.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if tiles:
        render_preview(mesh, tiles)
    for obj in tiles:
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.orphans_purge(do_recursive=True)
    # Preserve generated actions as editable sources; preview duplicates are temporary.
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierLibraryActions.blend'), compress=True)
    print('LIBRARY_ACTIONS_COMPLETE '+json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
