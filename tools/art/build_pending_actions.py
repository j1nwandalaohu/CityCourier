"""Author the seven outstanding in-place clips without rebuilding the rider.

Uses retained CC0 Mesh2Motion movement, plus original directed hit/back and
sprint transition editing. Never saves any pre-existing source or UE package.
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_common import SOURCE, export
import build_motion_probe as retarget

PROFILES = {
    'A_JumpStart': {'duration_s': .18, 'loop': False, 'source': 'Jump_Start'},
    'A_JumpLoop': {'duration_s': .40, 'loop': True, 'source': 'Jump_air'},
    'A_Land': {'duration_s': .25, 'loop': False, 'source': 'Jump_Land'},
    'A_HitReact_Front': {'duration_s': .40, 'loop': False, 'source': 'Hit_Chest'},
    'A_HitReact_Back': {'duration_s': .40, 'loop': False, 'source': 'Hit_Chest / original directional editing'},
    'A_RunStart': {'duration_s': .20, 'loop': False, 'source': 'delivered A_Sprint / original transition'},
    'A_RunStop': {'duration_s': .25, 'loop': False, 'source': 'delivered A_Sprint / original transition'},
}
# 100 Hz exactly represents .18/.20/.25/.40, unlike the nominal 30 Hz
# contract. Every pose is baked; the importer preserves the exact durations.
FPS = 100


def reference(rig):
    return [(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local))) for b in rig.data.bones]


def smooth(t):
    t = max(0., min(1., t))
    return t*t*(3-2*t)


def copy_pose(pose):
    return {n: m.copy() for n, m in pose.items()}


def blend(a, b, t):
    result = {}
    for n in a:
        m = a[n].to_quaternion().slerp(b[n].to_quaternion(), t).to_matrix().to_4x4()
        m.translation = a[n].translation.lerp(b[n].translation, t)
        result[n] = m
    return result


def sample_action(rig, action, u):
    rig.animation_data_create()
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    first, last = action.frame_range
    frame = first + max(0., min(1., u))*(last-first)
    bpy.context.scene.frame_set(math.floor(frame), subframe=frame-math.floor(frame))
    return {b.name: b.matrix.copy() for b in rig.pose.bones}


def source_rig():
    retarget.CLIPS = {n: n for n in ('Jump_Start', 'Jump_air', 'Jump_Land', 'Hit_Chest')}
    previous = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(retarget.filtered_source()))
    imported = set(bpy.data.objects)-previous
    source = next(o for o in imported if o.type == 'ARMATURE')
    for obj in imported:
        if obj != source:
            bpy.data.objects.remove(obj, do_unlink=True)
    source.hide_render = True
    return source


def inspect():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 30
    source = source_rig()
    for name in retarget.CLIPS:
        rows = []
        for i in range(21):
            pose = sample_action(source, bpy.data.actions[name], i/20)
            rows.append([i/20, *[round(v, 4) for v in pose['pelvis'].translation],
                         round(pose['foot_l'].translation.z, 4), round(pose['foot_r'].translation.z, 4),
                         round(pose['head'].translation.y-pose['pelvis'].translation.y, 4)])
        print('SOURCE TRAJECTORY', name, json.dumps(rows), flush=True)


def build():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPhotoreal.blend'))
    rig = bpy.data.objects['CourierRig']
    mesh = bpy.data.objects['SK_Courier']
    scene = bpy.context.scene
    expected = reference(rig)
    original_actions = {a.name: action_digest(a) for a in bpy.data.actions}
    mesh_digest = mesh_signature(mesh)
    rig.animation_data_create()
    # Append the unchanged delivered motion data, never rerun build_rider (it
    # replaces the formal rider with the older source mesh).
    with bpy.data.libraries.load(str(SOURCE/'Blender/Mesh2MotionProbe.blend'), link=False) as (src, dst):
        dst.actions = ['A_M2M_Run', 'A_M2M_Idle', 'A_Idle']
    sprint, idle_action, production_idle_action = dst.actions
    idle = sample_action(rig, idle_action, 0.)
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    game_idle = sample_action(rig, production_idle_action, .35)
    game_idle['root'] = rest['root'].copy()
    source = source_rig()
    source_rest = {b.name: b.matrix_local.copy() for b in source.data.bones}
    align = {}
    for target, name in retarget.MAPPING.items():
        if target in ('pelvis', 'spine', 'chest', 'neck', 'head') or target.startswith('clavicle_'):
            q = rest[target].to_quaternion()
        else:
            a = rest[target].to_quaternion() @ Vector((0, 1, 0))
            b = source_rest[name].to_quaternion() @ Vector((0, 1, 0))
            if target.startswith('hand_'):
                side = target[-1]
                a = rest['finger_3_1_'+side].translation-rest[target].translation
                b = source_rest['middle_01_'+side].translation-source_rest[name].translation
            q = a.normalized().rotation_difference(b.normalized()) @ rest[target].to_quaternion()
        align[target] = source_rest[name].to_quaternion().inverted() @ q
    leg_scale = sum(rig.data.bones[n+'_l'].length for n in ('thigh', 'shin')) / sum(source.data.bones[n+'_l'].length for n in ('thigh', 'calf'))
    soles = sole_vertices(mesh)
    ground_pose(mesh, rest, game_idle, soles)

    def transfer(name, u):
        source_pose = sample_action(source, bpy.data.actions[name], u)
        matrices = {'root': rest['root'].copy()}
        for bone in rig.data.bones:
            if bone.name == 'root':
                continue
            head = matrices[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
            q = source_pose[retarget.MAPPING[bone.name]].to_quaternion() @ align[bone.name]
            if bone.name == 'pelvis':
                head = bone.head_local+(source_pose['pelvis'].translation-source_rest['pelvis'].translation)*leg_scale
            mat = q.to_matrix().to_4x4()
            mat.translation = head
            matrices[bone.name] = mat
        for side in ('l', 'r'):
            foot = 'foot_'+side
            ankle = rest[foot].translation+(source_pose[foot].translation-source_rest[foot].translation)*leg_scale
            pole = source_pose['calf_'+side].translation-source_pose['thigh_'+side].translation
            solve_leg(rig, rest, matrices, side, ankle, pole)
        return matrices

    # Authored event poses retain genuine jump crouch/flight anatomy. The
    # source start begins fully crouched and launches within its first 8%;
    # remove its long flight hold, and bring the response forward to .07 s.
    crouch = transfer('Jump_Start', 0.)
    takeoff = transfer('Jump_Start', .052)
    air_samples = [blend(idle, transfer('Jump_air', .5-.5*math.cos(math.tau*i/40)), .60) for i in range(41)]
    air_lowest = min(deformed_point(mesh, rest, pose, j).z for pose in air_samples for ids in soles.values() for j in ids)
    for pose in air_samples:
        for joint in pose:
            if joint != 'root':
                pose[joint].translation.z += .002-air_lowest
    flight = copy_pose(air_samples[0])
    landing = transfer('Jump_Land', .15)
    recoil = transfer('Hit_Chest', 0.)
    def directed_arms(pose, swing):
        for side in ('l', 'r'):
            names = ['clavicle_'+side]+[b.name for b in rig.data.bones['clavicle_'+side].children_recursive]
            for joint in names:
                pose[joint] = idle[joint].copy()
        rebuild_chain(rig, rest, pose)
        for side in ('l', 'r'):
            rotate_subtree(rig, pose, 'upperarm_'+side, swing)
    directed_arms(crouch, .25)
    directed_arms(takeoff, -.65)
    for pose in air_samples:
        directed_arms(pose, -.40)
    flight = copy_pose(air_samples[0])
    directed_arms(landing, -.25)
    for pose in (crouch, takeoff, landing, recoil):
        ground_pose(mesh, rest, pose, soles)
    # Neutral flight height is a local tuck only; no root or actor trajectory.
    # Limit the source's deep landing squat to a readable short soft landing.
    crouch = blend(idle, crouch, .30)
    takeoff = blend(idle, takeoff, .75)
    landing = blend(idle, landing, .38)
    ground_pose(mesh, rest, crouch, soles)
    ground_pose(mesh, rest, landing, soles)
    back = copy_pose(idle)
    # Original directional edit: fold forward at waist, lift guarding arms,
    # and recover. Reuse the source hit's arm angles, not its backward lean.
    for name in ('upperarm_l', 'forearm_l', 'hand_l', 'upperarm_r', 'forearm_r', 'hand_r'):
        back[name] = recoil[name].copy()
    back['pelvis'].translation += Vector((0, -.018, -.025))
    rebuild_chain(rig, rest, back)
    rotate_subtree(rig, back, 'spine', math.radians(17))
    rotate_subtree(rig, back, 'chest', math.radians(10))
    for side in ('l', 'r'):
        solve_leg(rig, rest, back, side, idle['foot_'+side].translation, Vector((0, -1, 0)))
    ground_pose(mesh, rest, back, soles)
    # Match an actual sprint source contact pose. Start's last frame is phase
    # zero; stop begins at the same phase and eases through a short final step.
    run_zero = sample_action(rig, sprint, 0.)

    def key_pose(points, u):
        for (ta, a), (tb, b) in zip(points, points[1:]):
            if u <= tb:
                return blend(a, b, smooth((u-ta)/(tb-ta)))
        return copy_pose(points[-1][1])

    clips = {}
    for name, profile in PROFILES.items():
        frames = round(profile['duration_s']*FPS)
        samples = []
        for i in range(frames+1):
            u = i/frames
            if name == 'A_JumpStart':
                pose = key_pose([(0, idle), (.34, crouch), (.72, takeoff), (1, flight)], u)
            elif name == 'A_JumpLoop':
                # A cosine reparameterization gives a closed loop with zero
                # endpoint velocity; the original air animation remains moving.
                pose = copy_pose(air_samples[i])
            elif name == 'A_Land':
                pose = key_pose([(0, idle), (.28, landing), (1, idle)], u)
            elif name == 'A_HitReact_Front':
                pose = key_pose([(0, idle), (.22, recoil), (.52, blend(idle, recoil, .58)), (1, idle)], u)
            elif name == 'A_HitReact_Back':
                pose = key_pose([(0, idle), (.22, back), (.52, blend(idle, back, .58)), (1, idle)], u)
            elif name == 'A_RunStart':
                running = sample_action(rig, sprint, ((u-1)*profile['duration_s']/.8333333333) % 1.)
                pose = blend(idle, running, smooth(u))
            else:
                running = sample_action(rig, sprint, u*profile['duration_s']/.8333333333)
                pose = blend(running, idle, smooth(u))
            pose['root'] = rest['root'].copy()
            ankles = {s: pose['foot_'+s].translation.copy() for s in ('l', 'r')}
            poles = {s: pose['shin_'+s].translation-pose['thigh_'+s].translation for s in ('l', 'r')}
            rebuild_chain(rig, rest, pose)
            for side in ('l', 'r'):
                solve_leg(rig, rest, pose, side, ankles[side], poles[side])
            # Grounded shoes use measured sole vertices, not ankle height.
            if name in ('A_Land', 'A_HitReact_Front', 'A_HitReact_Back'):
                plant_feet(rig, rest, pose, idle)
                ground_pose(mesh, rest, pose, soles)
            elif name.startswith('A_Run'):
                ground_pose(mesh, rest, pose, soles, only_penetration=True)
            elif name in ('A_JumpStart', 'A_JumpLoop'):
                ground_pose(mesh, rest, pose, soles, only_penetration=True)
            corrected = calibrate_pose(rig, rest, pose, idle, game_idle)
            corrected = collar_envelope(rig, rest, corrected, game_idle, amount=0.)
            # Keep both exact delivered Sprint endpoints. The collar-safe
            # production-idle calibration fades only near those endpoints;
            # existing Sprint's own skinning limits are reported separately.
            correction = 1.
            if name == 'A_RunStart':
                correction = 1-smooth(u)
            elif name == 'A_RunStop':
                correction = smooth(u)
            pose = blend_local(rig, rest, pose, corrected, correction)
            ankles = {s: pose['foot_'+s].translation.copy() for s in ('l', 'r')}
            poles = {s: pose['shin_'+s].translation-pose['thigh_'+s].translation for s in ('l', 'r')}
            rebuild_chain(rig, rest, pose)
            for side in ('l', 'r'):
                solve_leg(rig, rest, pose, side, ankles[side], poles[side])
            if name in ('A_Land', 'A_HitReact_Front', 'A_HitReact_Back'):
                plant_feet(rig, rest, pose, game_idle)
                ground_pose(mesh, rest, pose, soles)
            else:
                ground_pose(mesh, rest, pose, soles, only_penetration=True)
            samples.append(pose)
        if profile['loop']:
            samples[-1] = copy_pose(samples[0])
        generated = bake(rig, rest, name, samples)
        scene.render.fps = FPS
        scene.frame_start = 1
        scene.frame_end = frames+1
        scene.frame_set(1)
        export(name, [rig], 'PendingActions', True)
        clips[name] = {**profile, 'fps': FPS, 'samples': frames+1,
                       'fbx_sha256': hashlib.sha256((SOURCE/'Exports/PendingActions'/f'{name}.fbx').read_bytes()).hexdigest()}
    assert reference(rig) == expected
    assert mesh_signature(mesh) == mesh_digest
    assert all(action_digest(bpy.data.actions[n]) == value for n, value in original_actions.items())
    for action in bpy.data.actions:
        action.use_fake_user = True
    bpy.data.objects.remove(source, do_unlink=True)
    rig.animation_data.action = bpy.data.actions['A_JumpStart']
    scene.frame_start = 1
    scene.frame_end = 19
    scene.frame_set(1)
    report = {'source_revision': '653a9698f5f315523072f1c3ff3496100401317b',
              'license': 'CC0 retained Mesh2Motion human-base-animations plus original project edits',
              'source_glb_sha256': hashlib.sha256((SOURCE/'External/Mesh2MotionProbe/static/animations/human-base-animations.glb').read_bytes()).hexdigest(),
              'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'skeleton_reference_unchanged': True, 'rider_mesh_unchanged': True,
              'preexisting_source_actions_unchanged': True, 'root_motion': False,
              'source_reference': expected, 'mesh_signature': mesh_digest,
              'protected_source_action_digests': original_actions, 'clips': clips,
              'sample_rate_note': '100 Hz exact .18/.20/.25/.40 second durations; source transfer evaluated from retained motion, no gameplay timing changes',
              'upper_body_calibration': 'Production A_Idle local @ inverse(retarget Idle local) @ motion local; incompatible differential collar roll removed while preserving authored sagittal lean and face orientation. Existing Sprint endpoints remain unchanged.',
              'validation': {'status': 'pending'}, 'ue_import': {'status': 'pending'}}
    (SOURCE/'pending_actions.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierPendingActions.blend'))
    print('PENDING ACTIONS EXPORTED', json.dumps(clips), flush=True)


def action_digest(action):
    values = [(f.data_path, f.array_index, [(list(k.co), k.interpolation) for k in f.keyframe_points]) for f in action.fcurves]
    return hashlib.sha256(json.dumps(values, sort_keys=True).encode()).hexdigest()


def mesh_signature(mesh):
    h = hashlib.sha256()
    for v in mesh.data.vertices:
        h.update(repr((tuple(v.co), [(g.group, round(g.weight, 8)) for g in v.groups])).encode())
    for p in mesh.data.polygons:
        h.update(repr((tuple(p.vertices), p.material_index)).encode())
    h.update(repr([g.name for g in mesh.vertex_groups]).encode())
    h.update(repr([m.name for m in mesh.data.materials]).encode())
    h.update(repr([tuple(row) for row in mesh.matrix_world]).encode())
    return h.hexdigest()


def sole_vertices(mesh):
    result = {}
    shoe = {i for p in mesh.data.polygons if mesh.data.materials[p.material_index].name == 'M_WorkShoes' for i in p.vertices}
    for side, sign in (('l', 1), ('r', -1)):
        ids = [i for i in shoe if mesh.data.vertices[i].co.x*sign > 0]
        low = min(mesh.data.vertices[i].co.z for i in ids)
        result[side] = [i for i in ids if mesh.data.vertices[i].co.z < low+.012]
    return result


def deformed_point(mesh, rest, pose, index):
    vertex = mesh.data.vertices[index]
    point = Vector()
    for group in vertex.groups:
        n = mesh.vertex_groups[group.group].name
        point += (pose[n] @ rest[n].inverted() @ vertex.co)*group.weight
    return point


def ground_pose(mesh, rest, pose, soles, only_penetration=False):
    lowest = min(deformed_point(mesh, rest, pose, i).z for ids in soles.values() for i in ids)
    lift = max(0., .002-lowest) if only_penetration else .002-lowest
    for name, mat in pose.items():
        if name != 'root':
            mat.translation.z += lift


def solve_leg(rig, rest, pose, side, ankle, pole):
    thigh, shin, foot = (n+'_'+side for n in ('thigh', 'shin', 'foot'))
    hip = pose[thigh].translation
    a, b = rig.data.bones[thigh].length, rig.data.bones[shin].length
    delta = ankle-hip
    distance = max(.001, min(delta.length, a+b-.0001))
    axis = delta.normalized()
    pole = (pole-axis*pole.dot(axis)).normalized()
    along = (a*a-b*b+distance*distance)/(2*distance)
    knee = hip+axis*along+pole*math.sqrt(max(0., a*a-along*along))
    for joint, head, end in ((thigh, hip, knee), (shin, knee, hip+axis*distance)):
        old = pose[joint].to_quaternion()
        q = (old @ Vector((0, 1, 0))).rotation_difference((end-head).normalized()) @ old
        mat = q.to_matrix().to_4x4()
        mat.translation = head
        pose[joint] = mat
    pose[foot].translation = hip+axis*distance


def rebuild_chain(rig, rest, pose):
    for bone in rig.data.bones:
        if bone.parent and bone.name != 'pelvis':
            pose[bone.name].translation = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local


def rotate_subtree(rig, pose, name, radians):
    pivot = pose[name].translation.copy()
    transform = Matrix.Translation(pivot) @ Quaternion((1, 0, 0), radians).to_matrix().to_4x4() @ Matrix.Translation(-pivot)
    descendants = {name, *[b.name for b in rig.data.bones[name].children_recursive]}
    for joint in descendants:
        pose[joint] = transform @ pose[joint]


def plant_feet(rig, rest, pose, idle):
    rebuild_chain(rig, rest, pose)
    for side in ('l', 'r'):
        foot = 'foot_'+side
        pose[foot] = idle[foot].copy()
        solve_leg(rig, rest, pose, side, idle[foot].translation, Vector((0, -1, 0)))


def calibrate_pose(rig, rest, pose, source_idle, target_idle):
    """Transfer local motion deltas onto the actual shipped idle pose.

    The legacy retarget idle has a different static chest/neck/clavicle basis
    from production A_Idle. Removing that offset preserves animation deltas
    while avoiding a spurious neck/shoulder twist in the unchanged formal mesh.
    """
    result = {}
    for bone in rig.data.bones:
        name, parent = bone.name, bone.parent.name if bone.parent else None
        if name == 'root':
            result[name] = rest[name].copy()
            continue
        def local(p):
            return p[parent].to_quaternion().inverted() @ p[name].to_quaternion()
        q = local(target_idle) @ local(source_idle).inverted() @ local(pose)
        q = result[parent].to_quaternion() @ q
        matrix = q.to_matrix().to_4x4()
        matrix.translation = (target_idle[name].translation+pose[name].translation-source_idle[name].translation if name == 'pelvis' else
                              result[parent] @ rest[parent].inverted() @ bone.head_local)
        result[name] = matrix
    for side in ('l', 'r'):
        foot = 'foot_'+side
        ankle = target_idle[foot].translation+pose[foot].translation-source_idle[foot].translation
        q = target_idle[foot].to_quaternion() @ source_idle[foot].to_quaternion().inverted() @ pose[foot].to_quaternion()
        result[foot] = q.to_matrix().to_4x4()
        result[foot].translation = ankle
        solve_leg(rig, rest, result, side, ankle, result['shin_'+side].translation-result['thigh_'+side].translation)
    return result


def blend_local(rig, rest, a, b, amount):
    """Blend anatomical local rotations, avoiding global finger-chain flips."""
    result = {}
    for bone in rig.data.bones:
        name, parent = bone.name, bone.parent.name if bone.parent else None
        qa, qb = a[name].to_quaternion(), b[name].to_quaternion()
        if parent:
            qa = a[parent].to_quaternion().inverted() @ qa
            qb = b[parent].to_quaternion().inverted() @ qb
        q = qa.slerp(qb, amount)
        if parent:
            q = result[parent].to_quaternion() @ q
        matrix = q.to_matrix().to_4x4()
        matrix.translation = (a[name].translation.lerp(b[name].translation, amount) if name in ('root', 'pelvis') else
                              result[parent] @ rest[parent].inverted() @ bone.head_local)
        result[name] = matrix
    for side in ('l', 'r'):
        foot = 'foot_'+side
        ankle = a[foot].translation.lerp(b[foot].translation, amount)
        q = a[foot].to_quaternion().slerp(b[foot].to_quaternion(), amount)
        result[foot] = q.to_matrix().to_4x4()
        result[foot].translation = ankle
        solve_leg(rig, rest, result, side, ankle, result['shin_'+side].translation-result['thigh_'+side].translation)
    return result


def collar_envelope(rig, rest, pose, idle, amount=0., preserve_lean=True):
    """Limit differential collar twist while retaining the authored torso lean.

    Chest, neck and clavicle deltas can move differently weighted skin/shirt
    surfaces through one another. Coherent sagittal torso lean has the same
    visible intent without adding an incompatible local roll at the collar.
    """
    original = copy_pose(pose)
    joints = {'chest', 'neck', 'clavicle_l', 'clavicle_r'}
    for bone in rig.data.bones:
        name, parent = bone.name, bone.parent.name if bone.parent else None
        if not parent:
            continue
        q = original[parent].to_quaternion().inverted() @ original[name].to_quaternion()
        if name in joints:
            baseline = idle[parent].to_quaternion().inverted() @ idle[name].to_quaternion()
            q = baseline.slerp(q, amount)
        q = pose[parent].to_quaternion() @ q
        matrix = q.to_matrix().to_4x4()
        matrix.translation = (original[name].translation if name == 'pelvis' else
                              pose[parent] @ rest[parent].inverted() @ bone.head_local)
        pose[name] = matrix
    if preserve_lean:
        pivot = pose['spine'].translation
        delta = pose['head'].translation-pivot
        target_y = original['head'].translation.y-pivot.y
        radius = math.hypot(delta.y, delta.z)
        theta = math.atan2(delta.y, delta.z)-math.asin(max(-.95, min(.95, target_y/radius)))
        rotate_subtree(rig, pose, 'spine', theta)
        # Keep the authored face/helmet world orientation; the coherent collar
        # correction acts below the head joint, without forcing a downwards gaze.
        for name in {'head', *[b.name for b in rig.data.bones['head'].children_recursive]}:
            head = pose[name].translation.copy()
            pose[name] = original[name].to_quaternion().to_matrix().to_4x4()
            pose[name].translation = head
    return pose


def bake(rig, rest, name, samples):
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    previous = {}
    for frame, pose in enumerate(samples, 1):
        for bone in rig.data.bones:
            parent = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ rest[bone.name] if bone.parent else rest[bone.name]
            pb = rig.pose.bones[bone.name]
            pb.rotation_mode = 'QUATERNION'
            pb.matrix_basis = parent.inverted() @ pose[bone.name]
            if bone.name in previous:
                pb.rotation_quaternion.make_compatible(previous[bone.name])
            previous[bone.name] = pb.rotation_quaternion.copy()
            pb.keyframe_insert('location', frame=frame)
            pb.keyframe_insert('rotation_quaternion', frame=frame)
    for curve in action.fcurves:
        for key in curve.keyframe_points:
            key.interpolation = 'LINEAR'
    return action


if __name__ == '__main__':
    inspect() if '--inspect' in sys.argv else build()
