"""Retarget complete CC0 clips; no authored arm paths or synthetic recovery poses."""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_common import SOURCE
from build_motion_probe import MAPPING
from build_library_actions import sample, source_action, bake

ROOT = SOURCE.parents[1]
PROFILES = {
    'A_CitizenIdle': ('Idle_A', 2., True, 'Reactions'),
    'A_CitizenWalk': ('Walk', 1., True, 'Reactions'),
    'A_CitizenHit': ('Hit_Chest', 29/60, False, 'Reactions'),
    'A_CitizenFall': ('Death_D', 1.05, False, 'Reactions'),
    'A_CitizenGetUp': ('LayToIdle', 2.7, False, 'Reactions'),
    'A_CitizenGuard': ('Fighting Idle', 2., True, 'Behavior'),
    'A_CitizenPunchL': ('Punch_Jab', .8, False, 'Behavior'),
    'A_CitizenPunchR': ('Punch_Cross', .8, False, 'Behavior'),
    'A_CitizenRun': ('Sprint', .65, True, 'Behavior'),
    'A_CitizenCower': ('Shivering', 1.5, True, 'Behavior'),
}


def import_source(filename, names):
    import struct
    data = (SOURCE/'External/Mesh2MotionProbe/static/animations'/filename).read_bytes()
    size, kind = struct.unpack_from('<II', data, 12)
    doc = json.loads(data[20:20+size])
    doc['animations'] = [a for a in doc['animations'] if a['name'] in names]
    if {a['name'] for a in doc['animations']} != names:
        raise RuntimeError('Missing library clips: '+filename)
    payload = json.dumps(doc, separators=(',', ':')).encode()
    payload += b' '*((-len(payload)) % 4)
    tail = data[20+size:]
    out = ROOT/'Saved/Validation/LibraryActions'
    out.mkdir(parents=True, exist_ok=True)
    path = out/('citizen-'+filename)
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 20+len(payload)+len(tail))+struct.pack('<II', len(payload), kind)+payload+tail)
    before = set(bpy.data.objects)
    before_actions = set(bpy.data.actions)
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = set(bpy.data.objects)-before
    source = next(o for o in imported if o.type == 'ARMATURE')
    action_names = {}
    for name in names:
        candidates = [a for a in set(bpy.data.actions)-before_actions
                      if a.name.split('.')[0] in (name, name+'_Armature')]
        if len(candidates) != 1:
            raise RuntimeError('Missing or ambiguous newly imported source action: '+name)
        action_names[name] = candidates[0].name
    source['library_actions_json'] = json.dumps(action_names)
    for obj in imported:
        if obj != source:
            bpy.data.objects.remove(obj, do_unlink=True)
    return source, hashlib.sha256(data).hexdigest()


def build(behavior=None):
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Ambient/CharMorph/CitizenCharMorph.blend'))
    target, mesh = bpy.data.objects['CourierRig'], bpy.data.objects['SK_CitizenCharMorph']
    target.animation_data_clear()
    rest = {b.name: b.matrix_local.copy() for b in target.data.bones}
    signature = [(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local))) for b in target.data.bones]
    scene = bpy.context.scene
    scene.render.fps = 60
    addon_names = {'Fighting Idle', 'Shivering'}
    base_names = {p[0] for p in PROFILES.values()}-addon_names
    base, base_hash = import_source('human-base-animations.glb', base_names)
    addon, addon_hash = import_source('human-addon-animations.glb', addon_names)
    source_rigs = {name: addon if name in addon_names else base for name in {p[0] for p in PROFILES.values()}}
    alignment = {}
    for source in (base, addon):
        sr = {b.name: b.matrix_local.copy() for b in source.data.bones}
        align = {}
        for n, src in MAPPING.items():
            q = rest[n].to_quaternion()
            if n not in ('pelvis', 'spine', 'chest', 'neck', 'head') and not n.startswith('clavicle_'):
                a, b = q @ Vector((0, 1, 0)), sr[src].to_quaternion() @ Vector((0, 1, 0))
                if n.startswith('hand_'):
                    a = rest['finger_3_1_'+n[-1]].translation-rest[n].translation
                    b = sr['middle_01_'+n[-1]].translation-sr[src].translation
                q = a.normalized().rotation_difference(b.normalized()) @ q
            align[n] = sr[src].to_quaternion().inverted() @ q
        scale = sum(target.data.bones[n+'_l'].length for n in ('thigh', 'shin'))/sum(source.data.bones[n+'_l'].length for n in ('thigh', 'calf'))
        alignment[source.name] = (sr, align, scale)
    # Floor calibration is a retarget adjustment, not a replacement motion curve.
    names = list(rest)
    bone_index = {n: i for i, n in enumerate(names)}
    ids, joints, coefficients = [], [], []
    for vertex in mesh.data.vertices:
        for group in vertex.groups:
            n = mesh.vertex_groups[group.group].name
            if n not in rest or group.weight <= 0:
                continue
            point = rest[n].inverted() @ vertex.co
            ids.append(vertex.index)
            joints.append(bone_index[n])
            coefficients.append([point.x*group.weight, point.y*group.weight, point.z*group.weight, group.weight])
    ids, joints, coefficients = np.array(ids), np.array(joints), np.array(coefficients)

    def transfer(source_name, u):
        source = source_rigs[source_name]
        action = bpy.data.actions[json.loads(source['library_actions_json'])[source_name]]
        first, last = action.frame_range
        sp = sample(source, action, first+(last-first)*u)
        sr, align, scale = alignment[source.name]
        pose = {'root': rest['root'].copy()}
        for bone in target.data.bones:
            n = bone.name
            if n == 'root':
                continue
            m = (sp[MAPPING[n]].to_quaternion() @ align[n]).to_matrix().to_4x4()
            m.translation = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
            if n == 'pelvis':
                m.translation = rest[n].translation+(sp['pelvis'].translation-sr['pelvis'].translation)*scale
                m.translation.x, m.translation.y = rest[n].translation.x, rest[n].translation.y
            pose[n] = m
        return pose

    def mirror(pose):
        reflection = Matrix.Diagonal((-1., 1., 1., 1.))
        result = {}
        for bone in target.data.bones:
            n = bone.name
            other = n[:-1]+('r' if n[-1] == 'l' else 'l') if n.endswith(('_l', '_r')) else n
            m = reflection @ pose[other] @ rest[other].inverted() @ reflection @ rest[n]
            if bone.parent and n != 'pelvis':
                m.translation = result[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
            result[n] = m
        return result

    reports = {group: {'license': 'CC0-1.0', 'upstream': 'https://github.com/Mesh2Motion/mesh2motion-app',
        'source_revision': '653a9698f5f315523072f1c3ff3496100401317b', 'clips': {},
        'source_hashes': {'human-base-animations.glb': base_hash, 'human-addon-animations.glb': addon_hash},
        'reference_skeleton_unchanged': True, 'mesh_unchanged': True,
        'pose_authoring': 'Library motion only; proportional retarget, root lock, floor calibration and timing alignment; no synthetic arm paths or recovery key poses'}
        for group in ('Reactions', 'Behavior')}
    selected = {n: p for n, p in PROFILES.items() if behavior is None or (p[3] == 'Behavior') == behavior}
    for name, (source_name, duration, loop, group) in selected.items():
        mirrored, contact = False, None
        if name.startswith('A_CitizenPunch'):
            probe = [transfer(source_name, i/100) for i in range(101)]
            excursion = {s: max(-p['hand_'+s].translation.y for p in probe)-min(-p['hand_'+s].translation.y for p in probe) for s in ('l', 'r')}
            side = 'l' if name.endswith('L') else 'r'
            mirrored = max(excursion, key=excursion.get) != side
            if mirrored:
                probe = [mirror(p) for p in probe]
            peak = max(range(101), key=lambda i: -probe[i]['hand_'+side].translation.y)
            if not 0 < peak < 100:
                raise RuntimeError('Library punch must extend and recover: '+name)
            contact = peak/100
        poses = []
        count = round(duration*60)
        for frame in range(count+1):
            t = frame/60
            u = frame/count
            if contact is not None:
                u = contact*t/.30 if t <= .30 else contact+(1-contact)*(t-.30)/(duration-.30)
            pose = transfer(source_name, u)
            if mirrored:
                pose = mirror(pose)
            rows = np.array([list(pose[n][2]) for n in names])
            lowest = np.bincount(ids, weights=np.einsum('ij,ij->i', coefficients, rows[joints]), minlength=len(mesh.data.vertices)).min()
            for n, m in pose.items():
                if n != 'root':
                    m.translation.z += .004-lowest
            if not all(math.isfinite(v) for m in pose.values() for row in m for v in row):
                raise RuntimeError('Invalid transform: '+name)
            poses.append(pose)
        if loop:
            poses[-1] = {n: m.copy() for n, m in poses[0].items()}
        bake(target, rest, name, poses)
        scene.frame_start, scene.frame_end = 1, len(poses)
        scene.frame_set(1)
        output = SOURCE/'Ambient/CharMorph'/group
        output.mkdir(parents=True, exist_ok=True)
        path = output/(name+'.fbx')
        bpy.ops.object.select_all(action='DESELECT')
        target.hide_set(False)
        target.select_set(True)
        bpy.context.view_layer.objects.active = target
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE'},
            axis_forward='-Y', axis_up='Z', global_scale=1, apply_unit_scale=True, add_leaf_bones=False,
            use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_actions=False,
            bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0)
        reports[group]['clips'][name] = {'source_action': source_name, 'duration_s': count/60, 'fps': 60,
            'samples': count+1, 'loop': loop, 'mirrored': mirrored, 'source_contact_normalized': contact,
            'fbx_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'pelvis_end_cm': list(poses[-1]['pelvis'].translation*100), 'head_end_cm': list(poses[-1]['head'].translation*100)}
        print('CITIZEN_LIBRARY_EXPORTED '+name, flush=True)
    if signature != [(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local))) for b in target.data.bones]:
        raise RuntimeError('Citizen reference skeleton changed')
    for group, report in reports.items():
        if report['clips']:
            (SOURCE/'Ambient/CharMorph'/group/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    for source in (base, addon):
        bpy.data.objects.remove(source, do_unlink=True)
    # Keep only retargeted actions in the new editable file.
    for action in list(bpy.data.actions):
        if action.name not in selected:
            if target.animation_data and target.animation_data.action == action:
                target.animation_data.action = None
            bpy.data.actions.remove(action)
    bpy.data.orphans_purge(do_recursive=True)
    bpy.context.preferences.filepaths.save_version = 0
    suffix = '' if behavior is None else ('Behavior' if behavior else 'Reactions')
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Ambient/CharMorph'/('CitizenLibrary'+suffix+'.blend')), compress=True)
    print('CITIZEN_LIBRARY_COMPLETE '+str(len(selected)), flush=True)


if __name__ == '__main__':
    build()
