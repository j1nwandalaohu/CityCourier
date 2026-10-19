"""Retarget retained CC0 Idle_Subtle onto the shipped rider, without rebuilding it."""
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_common import SOURCE, export
import build_motion_probe as motion
import build_pending_actions as author

ROOT = SOURCE.parents[1]
OUT = ROOT/'Saved/RelaxedIdle'
NAME = 'A_IdleRelaxed'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPhotoreal.blend'))
    rig, mesh = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier']
    protected_mesh, protected_rig = author.mesh_signature(mesh), author.reference(rig)
    with bpy.data.libraries.load(str(SOURCE/'Blender/Mesh2MotionProbe.blend'), link=False) as (src, dst):
        dst.actions = ['A_Idle']
    old_idle = dst.actions[0]
    baseline = author.sample_action(rig, old_idle, .35)
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    scene = bpy.context.scene
    scene.render.fps = 30
    data = (SOURCE/'External/Mesh2MotionProbe/static/animations/human-addon-animations.glb').read_bytes()
    assert hashlib.sha256(data).hexdigest() == 'a0d64d555e0d492026b72d58bf8e16c5e86779295f9093e376dcc001915c2c95'
    size, kind = struct.unpack_from('<II', data, 12)
    doc = json.loads(data[20:20+size])
    doc['animations'] = [a for a in doc['animations'] if a['name'] == 'Idle_Subtle']
    assert len(doc['animations']) == 1
    payload = json.dumps(doc, separators=(',', ':')).encode()
    payload += b' '*((-len(payload)) % 4)
    tail = data[20+size:]
    selected = OUT/'idle-source.glb'
    selected.write_bytes(struct.pack('<III', 0x46546c67, 2, 20+len(payload)+len(tail))+
                         struct.pack('<II', len(payload), kind)+payload+tail)
    previous = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(selected))
    imported = set(bpy.data.objects)-previous
    source = next(o for o in imported if o.type == 'ARMATURE')
    for obj in imported:
        if obj != source:
            bpy.data.objects.remove(obj, do_unlink=True)
    source_rest = {b.name: b.matrix_local.copy() for b in source.data.bones}
    align = {}
    for target, name in motion.MAPPING.items():
        q = rest[target].to_quaternion()
        if target not in ('pelvis', 'spine', 'chest', 'neck', 'head') and not target.startswith('clavicle_'):
            a = q @ Vector((0, 1, 0))
            b = source_rest[name].to_quaternion() @ Vector((0, 1, 0))
            if target.startswith('hand_'):
                a = rest['finger_3_1_'+target[-1]].translation-rest[target].translation
                b = source_rest['middle_01_'+target[-1]].translation-source_rest[name].translation
            q = a.normalized().rotation_difference(b.normalized()) @ q
        align[target] = source_rest[name].to_quaternion().inverted() @ q
    scale = sum(rig.data.bones[n+'_l'].length for n in ('thigh', 'shin'))/sum(source.data.bones[n+'_l'].length for n in ('thigh', 'calf'))
    action = bpy.data.actions['Idle_Subtle']
    duration = (action.frame_range[1]-action.frame_range[0])/30
    frames = round(duration*30)
    soles = author.sole_vertices(mesh)
    samples = []
    for i in range(frames+1):
        src_pose = author.sample_action(source, action, i/frames)
        pose = {'root': rest['root'].copy()}
        for bone in rig.data.bones:
            name = bone.name
            if name == 'root':
                continue
            q = src_pose[motion.MAPPING[name]].to_quaternion() @ align[name]
            matrix = q.to_matrix().to_4x4()
            matrix.translation = (rest[name].translation+(src_pose['pelvis'].translation-source_rest['pelvis'].translation)*scale
                                  if name == 'pelvis' else pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local)
            pose[name] = matrix
        # The collar uses legacy weights. Keep coherent torso motion while
        # removing incompatible differential neck/clavicle roll from retargeting.
        author.collar_envelope(rig, rest, pose, baseline, amount=0.)
        for side in ('l', 'r'):
            foot = 'foot_'+side
            ankle = rest[foot].translation+(src_pose[foot].translation-source_rest[foot].translation)*scale
            author.solve_leg(rig, rest, pose, side, ankle, Vector((0, -1, 0)))
        samples.append(pose)
    # This source's absolute pose is a wide, half-crouched ready stance.
    # Transfer its small motion deltas onto a relaxed courier stance instead.
    source_neutral = author.copy_pose(samples[0])
    neutral = author.copy_pose(baseline)
    neutral['root'] = rest['root'].copy()
    relaxed_hands = author.blend_local(rig, rest, baseline, source_neutral, .42)
    for name in neutral:
        if name.startswith('finger_'):
            neutral[name] = relaxed_hands[name].copy()
    # Relax locked elbows and shift a little weight onto one leg. Keep the
    # shipped foot spacing, rather than importing the source's combat stance.
    neutral['pelvis'].translation += Vector((.012, 0, -.008))
    author.rebuild_chain(rig, rest, neutral)
    for side in ('l', 'r'):
        joint = 'clavicle_'+side
        pivot = neutral[joint].translation.copy()
        lower = Matrix.Translation(pivot) @ Quaternion((0, 1, 0), .04 if side == 'l' else -.04).to_matrix().to_4x4() @ Matrix.Translation(-pivot)
        for name in [joint]+[b.name for b in rig.data.bones[joint].children_recursive]:
            neutral[name] = lower @ neutral[name]
        joint = 'upperarm_'+side
        pivot = neutral[joint].translation.copy()
        outward = Matrix.Translation(pivot) @ Quaternion((0, 1, 0), -.10 if side == 'l' else .10).to_matrix().to_4x4() @ Matrix.Translation(-pivot)
        for name in [joint]+[b.name for b in rig.data.bones[joint].children_recursive]:
            neutral[name] = outward @ neutral[name]
        author.rotate_subtree(rig, neutral, 'forearm_'+side, -.08)
    author.plant_feet(rig, rest, neutral, baseline)
    author.ground_pose(mesh, rest, neutral, soles)
    for i, pose in enumerate(samples):
        transferred = author.calibrate_pose(rig, rest, pose, source_neutral, neutral)
        pose = author.blend_local(rig, rest, neutral, transferred, .25)
        author.plant_feet(rig, rest, pose, neutral)
        author.ground_pose(mesh, rest, pose, soles)
        samples[i] = pose
    # Ease the closing half-second onto the opening pose to avoid a positional
    # seam if the retained source isn't perfectly cyclic.
    seam = min(15, frames//4)
    for i in range(frames-seam, frames+1):
        samples[i] = author.blend_local(rig, rest, samples[i], samples[0], author.smooth((i-frames+seam)/seam))
    samples[-1] = author.copy_pose(samples[0])
    lowest_sole = min(author.deformed_point(mesh, rest, p, j).z
                      for p in samples for ids in soles.values() for j in ids)
    foot_drift = max((p['foot_'+s].translation-samples[0]['foot_'+s].translation).length
                     for p in samples for s in ('l', 'r'))
    assert lowest_sole > -.001 and foot_drift < .0001, (lowest_sole, foot_drift)
    assert all((p['root'].translation-rest['root'].translation).length < 1e-6 for p in samples)
    generated = author.bake(rig, rest, NAME, samples)
    assert author.mesh_signature(mesh) == protected_mesh
    assert author.reference(rig) == protected_rig
    rig.animation_data.action = generated
    scene.frame_start, scene.frame_end = 1, frames+1
    scene.frame_set(1)
    export(NAME, [rig], 'RelaxedIdle', True)
    bpy.data.objects.remove(source, do_unlink=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierIdle.blend'), compress=True)
    report = {'license': 'CC0-1.0', 'upstream': 'https://github.com/Mesh2Motion/mesh2motion-app',
              'source_revision': '653a9698f5f315523072f1c3ff3496100401317b', 'source_action': 'Idle_Subtle',
              'source_glb_sha256': hashlib.sha256(data).hexdigest(), 'fps': 30, 'source_duration_s': duration,
              'samples': len(samples), 'duration_s': frames/30, 'skeleton_reference_unchanged': True, 'rider_mesh_unchanged': True,
              'mesh_signature': protected_mesh, 'root_motion': False,
              'fbx_sha256': hashlib.sha256((SOURCE/'Exports/RelaxedIdle'/f'{NAME}.fbx').read_bytes()).hexdigest(),
              'joint_motion_cm': {n: max((p[n].translation-samples[0][n].translation).length*100 for p in samples)
                                  for n in ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')},
              'loop_error_cm': max((samples[0][n].translation-samples[-1][n].translation).length*100 for n in rest),
              'validation': {'status': 'passed', 'lowest_sole_cm': lowest_sole*100,
                             'foot_drift_cm': foot_drift*100, 'samples_checked': len(samples)}}
    (SOURCE/'relaxed_idle.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    # Fast shape preview of the actual formal mesh, alongside its old idle.
    for label, clip, offset in (('Before', old_idle, -0.65), ('After', generated, .65)):
        author.sample_action(rig, clip, .35)
        bpy.context.view_layer.update()
        copy = bpy.data.objects.new(label, bpy.data.meshes.new_from_object(mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())))
        bpy.context.collection.objects.link(copy)
        copy.location.x = offset
    mesh.hide_render = True
    camera_data = bpy.data.cameras.new('IdleCamera')
    camera = bpy.data.objects.new('IdleCamera', camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (2.7, -6.5, 2.4)
    camera.rotation_euler = (Vector((0, 0, .9))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type, camera_data.ortho_scale = 'ORTHO', 3.2
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1200, 900, 100
    scene.render.filepath = str(OUT/'idle-comparison.png')
    bpy.ops.render.render(write_still=True)
    print('RELAXED IDLE', json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
