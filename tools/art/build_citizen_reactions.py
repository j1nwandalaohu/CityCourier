"""Retarget retained CC0 Mesh2Motion actions onto the independent CharMorph NPC rig.

Run with Blender --background --python tools/art/build_citizen_reactions.py.
Never changes the input blend, mesh, player or reference skeleton.
"""
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'AssetsSource/CityCourier'
OUT = SOURCE/'Ambient/CharMorph/Reactions'
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(Path(__file__).parent))
from build_pending_actions import bake


def smooth(t):
    t = max(0., min(1., t))
    return t*t*(3-2*t)


def import_actions(filename, names):
    data = (SOURCE/'External/Mesh2MotionProbe/static/animations'/filename).read_bytes()
    size, kind = struct.unpack_from('<II', data, 12)
    doc = json.loads(data[20:20+size])
    doc['animations'] = [a for a in doc['animations'] if a['name'] in names]
    assert {a['name'] for a in doc['animations']} == set(names)
    payload = json.dumps(doc, separators=(',', ':')).encode()
    payload += b' '*((-len(payload)) % 4)
    tail = data[20+size:]
    path = ROOT/'Saved/Validation'/('citizen-'+filename)
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 20+len(payload)+len(tail))+
                     struct.pack('<II', len(payload), kind)+payload+tail)
    previous = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = set(bpy.data.objects)-previous
    rig = next(o for o in imported if o.type == 'ARMATURE')
    for obj in imported:
        if obj != rig:
            bpy.data.objects.remove(obj, do_unlink=True)
    return rig, hashlib.sha256(data).hexdigest()


def build():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Ambient/CharMorph/CitizenCharMorph.blend'))
    target, mesh = bpy.data.objects['CourierRig'], bpy.data.objects['SK_CitizenCharMorph']
    rest = {b.name: b.matrix_local.copy() for b in target.data.bones}
    signature = {n: [list(row) for row in m] for n, m in rest.items()}
    mesh_signature = [(tuple(v.co), [(g.group, g.weight) for g in v.groups]) for v in mesh.data.vertices]
    addon, addon_hash = import_actions('human-addon-animations.glb', ['Death_C', 'Zombie_Rise'])
    base, base_hash = import_actions('human-base-animations.glb', ['Hit_Chest', 'Idle_A', 'Walk'])
    scene = bpy.context.scene
    scene.render.fps = 60
    # The independent modern citizen retains the validated portrait rig's
    # reference axes and units. Never attach it to the player's Skeleton asset.
    from build_motion_probe import MAPPING
    mapping = MAPPING
    turn = Matrix.Identity(4)
    bones = list(rest)
    bone_index = {n: i for i, n in enumerate(bones)}
    ids, joints, coefficients = [], [], []
    for v in mesh.data.vertices:
        for g in v.groups:
            name = mesh.vertex_groups[g.group].name
            if g.weight <= 0 or name not in rest:
                continue
            point = rest[name].inverted()@v.co
            ids.append(v.index)
            joints.append(bone_index[name])
            coefficients.append([point.x*g.weight, point.y*g.weight, point.z*g.weight, g.weight])
    ids, joints = np.array(ids), np.array(joints)
    coefficients = np.array(coefficients)

    def retarget(rig, name, u):
        action = bpy.data.actions[name]
        rig.animation_data_create()
        rig.animation_data.action = action
        if action.slots:
            rig.animation_data.action_slot = action.slots[0]
        start, end = action.frame_range
        frame = start+(end-start)*u
        scene.frame_set(math.floor(frame), subframe=frame % 1)
        src_rest = {b.name: turn@b.matrix_local for b in rig.data.bones}
        src_pose = {b.name: turn@b.matrix for b in rig.pose.bones}
        scale = sum(target.data.bones[n+'_l'].length for n in ('thigh', 'shin')) / sum(rig.data.bones[n+'_l'].length for n in ('thigh', 'calf'))
        pose = {'root': rest['root'].copy()}
        for bone in target.data.bones:
            n = bone.name
            if n == 'root':
                continue
            src = mapping[n]
            reference = rest[n].to_quaternion()
            if n not in ('pelvis', 'spine', 'chest', 'neck', 'head') and not n.startswith('clavicle_'):
                a = (reference@Vector((0, 1, 0))).normalized()
                b = (src_rest[src].to_quaternion()@Vector((0, 1, 0))).normalized()
                if n.startswith('hand_'):
                    side = n[-1]
                    a = (rest['finger_3_1_'+side].translation-rest[n].translation).normalized()
                    b = (src_rest['middle_01_'+side].translation-src_rest[src].translation).normalized()
                reference = a.rotation_difference(b)@reference
            align = src_rest[src].to_quaternion().inverted()@reference
            q = src_pose[src].to_quaternion()@align
            head = pose[bone.parent.name]@rest[bone.parent.name].inverted()@bone.head_local
            if n == 'pelvis':
                head = bone.head_local+(src_pose[src].translation-src_rest[src].translation)*scale
            pose[n] = q.to_matrix().to_4x4()
            pose[n].translation = head
        return pose

    def mix(a, b, t):
        """Blend in parent space so the unchanged limb lengths remain exact."""
        result = {}
        for bone in target.data.bones:
            n, p = bone.name, bone.parent.name if bone.parent else None
            aa = a[p].inverted()@a[n] if p else a[n]
            bb = b[p].inverted()@b[n] if p else b[n]
            local = aa.to_quaternion().slerp(bb.to_quaternion(), t).to_matrix().to_4x4()
            local.translation = aa.translation.lerp(bb.translation, t)
            result[n] = result[p]@local if p else local
        return result

    def ground(pose):
        rows = np.array([list(pose[n][2]) for n in bones])
        lowest = np.bincount(ids, weights=np.einsum('ij,ij->i', coefficients, rows[joints]),
                             minlength=len(mesh.data.vertices)).min()
        for n, matrix in pose.items():
            if n != 'root':
                matrix.translation.z += .004-lowest
        return pose

    idle = ground(retarget(base, 'Idle_A', 0.))
    fallen = ground(retarget(addon, 'Death_C', 1.))
    clips = {}
    for name, duration in [('A_CitizenIdle', 2.), ('A_CitizenWalk', 1.), ('A_CitizenHit', .48),
                           ('A_CitizenFall', 1.05), ('A_CitizenGetUp', 2.7)]:
        samples = []
        for i in range(round(duration*60)+1):
            t = i/60
            u = t/duration
            if name.endswith('Idle'):
                pose = retarget(base, 'Idle_A', u)
            elif name.endswith('Walk'):
                pose = retarget(base, 'Walk', u)
            elif name.endswith('Hit'):
                pose = retarget(base, 'Hit_Chest', u)
                envelope = smooth(t/.07)*smooth((duration-t)/.14)
                pose = mix(idle, pose, envelope)
            elif name.endswith('Fall'):
                pose = retarget(addon, 'Death_C', u)
                pose = mix(idle, pose, smooth(t/.12))
            else:
                pose = retarget(addon, 'Zombie_Rise', max(0., min(1., (t-.22)/2.13)))
                pose = mix(fallen, pose, smooth(t/.22))
                pose = mix(pose, idle, smooth((t-2.35)/.35))
            samples.append(ground(pose))
        if name.endswith(('Idle', 'Walk')):
            samples[-1] = {n: m.copy() for n, m in samples[0].items()}
        target.hide_set(False)
        target.animation_data_create()
        action = bake(target, rest, name, samples)
        scene.frame_start, scene.frame_end = 1, len(samples)
        scene.frame_set(1)
        bpy.ops.object.select_all(action='DESELECT')
        target.select_set(True)
        bpy.context.view_layer.objects.active = target
        path = OUT/(name+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True,
            object_types={'ARMATURE'}, axis_forward='-Y', axis_up='Z', global_scale=1,
            apply_unit_scale=True, add_leaf_bones=False, use_armature_deform_only=True,
            bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
            bake_anim_simplify_factor=0)
        clips[name] = {'duration_s': (len(samples)-1)/60, 'fps': 60, 'samples': len(samples),
                       'fbx_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                       'head_end_cm': [x*100 for x in samples[-1]['head'].translation],
                       'pelvis_end_cm': [x*100 for x in samples[-1]['pelvis'].translation]}
    assert signature == {b.name: [list(row) for row in b.matrix_local] for b in target.data.bones}
    assert mesh_signature == [(tuple(v.co), [(g.group, g.weight) for g in v.groups]) for v in mesh.data.vertices]
    for obj in (base, addon):
        bpy.data.objects.remove(obj, do_unlink=True)
    for obj in list(bpy.data.objects):
        if obj not in (target, mesh):
            bpy.data.objects.remove(obj, do_unlink=True)
    mesh.hide_set(False)
    target.animation_data.action = bpy.data.actions['A_CitizenIdle']
    scene.frame_start, scene.frame_end = 1, 121
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CitizenReactions.blend'))
    report = {'license': 'CC0-1.0', 'upstream': 'https://github.com/Mesh2Motion/mesh2motion-app',
        'revision': '653a9698f5f315523072f1c3ff3496100401317b',
        'source_sha256': {'human-base-animations.glb': base_hash, 'human-addon-animations.glb': addon_hash},
        'clips': clips, 'reference_skeleton_unchanged': True, 'mesh_unchanged': True,
        'grounding': 'All skinned mesh vertices, 0.4 cm clearance; root remains fixed.',
        'mapping': mapping}
    (OUT/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('CITIZEN_REACTIONS', json.dumps(report), flush=True)


if __name__ == '__main__':
    build()
