"""Retarget retained CC0 crouch/crawl clips without changing the rider or skeleton."""
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
import build_pending_actions as author
from build_motion_probe import MAPPING


def main():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierIdle.blend'))
    rig, mesh, scene = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier'], bpy.context.scene
    original_mesh, original_reference = author.mesh_signature(mesh), author.reference(rig)
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    out = SOURCE.parents[1]/'Saved/PostureAuthoring'
    out.mkdir(parents=True, exist_ok=True)
    scene.render.fps = 60
    report = {'license': 'CC0-1.0', 'source_revision': '653a9698f5f315523072f1c3ff3496100401317b',
              'upstream': 'https://github.com/Mesh2Motion/mesh2motion-app', 'clips': {}, 'sources': {}}
    profiles = [('human-base-animations.glb', [('Crouch_Idle', 'A_CrouchIdle', 2., False),
                                             ('Crouch_Walk', 'A_CrouchWalk', 1.2, False)]),
                ('human-addon-animations.glb', [('Crawl', 'A_ProneIdle', 1.5, True),
                                               ('Crawl', 'A_ProneCrawl', 1.5, False)])]
    for file, clips in profiles:
        data = (SOURCE/'External/Mesh2MotionProbe/static/animations'/file).read_bytes()
        report['sources'][file] = hashlib.sha256(data).hexdigest()
        size, kind = struct.unpack_from('<II', data, 12)
        doc = json.loads(data[20:20+size])
        doc['animations'] = [a for a in doc['animations'] if a['name'] in {c[0] for c in clips}]
        payload = json.dumps(doc, separators=(',', ':')).encode()
        payload += b' '*((-len(payload)) % 4)
        tail = data[20+size:]
        selected = out/file
        selected.write_bytes(struct.pack('<III', 0x46546c67, 2, 20+len(payload)+len(tail))
                             +struct.pack('<II', len(payload), kind)+payload+tail)
        previous = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=str(selected))
        imported = set(bpy.data.objects)-previous
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
        for action_name, name, duration, freeze in clips:
            samples = []
            for i in range(round(duration*60)+1):
                sp = author.sample_action(source, bpy.data.actions[action_name], 0. if freeze else i/(duration*60))
                pose = {'root': rest['root'].copy()}
                for bone in rig.data.bones:
                    n = bone.name
                    if n == 'root':
                        continue
                    m = (sp[MAPPING[n]].to_quaternion() @ align[n]).to_matrix().to_4x4()
                    m.translation = (rest[n].translation+(sp['pelvis'].translation-sr['pelvis'].translation)*scale
                                     if n == 'pelvis' else pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local)
                    if n == 'pelvis':
                        m.translation.x, m.translation.y = rest[n].translation.x, rest[n].translation.y
                    pose[n] = m
                samples.append(pose)
            samples[-1] = author.copy_pose(samples[0])
            clip = author.bake(rig, rest, name, samples)
            lowest = math.inf
            for frame in range(1, len(samples)+1, 5):
                scene.frame_set(frame)
                evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
                lowest = min(lowest, min(v[2] for v in evaluated.bound_box))
            for pose in samples:
                for n in pose:
                    if n != 'root':
                        pose[n].translation.z += .008-lowest
            # Replace the temporary bake so exported keys contain the floor correction.
            rig.animation_data.action = None
            bpy.data.actions.remove(clip)
            author.bake(rig, rest, name, samples)
            scene.frame_start, scene.frame_end = 1, len(samples)
            scene.frame_set(1)
            export(name, [rig], 'PostureActions', True)
            report['clips'][name] = {'source_action': action_name, 'duration_s': duration, 'fps': 60, 'loop': True,
                                    'fbx_sha256': hashlib.sha256((SOURCE/'Exports/PostureActions'/f'{name}.fbx').read_bytes()).hexdigest(),
                                    'pelvis_height_cm': samples[0]['pelvis'].translation.z*100,
                                    'head_height_cm': samples[0]['head'].translation.z*100}
        bpy.data.objects.remove(source, do_unlink=True)
    assert author.mesh_signature(mesh) == original_mesh and author.reference(rig) == original_reference
    scene.frame_start, scene.frame_end = 1, 91
    rig.animation_data.action = bpy.data.actions['A_ProneCrawl']
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierPostures.blend'), compress=True)
    report['validation'] = {'status': 'passed', 'root_motion': False, 'rider_mesh_unchanged': True, 'skeleton_reference_unchanged': True}
    (SOURCE/'posture_actions.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    for i, name in enumerate(report['clips']):
        author.sample_action(rig, bpy.data.actions[name], .25)
        evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        copy = bpy.data.objects.new(name, bpy.data.meshes.new_from_object(evaluated))
        bpy.context.collection.objects.link(copy)
        copy.location.x = (i-1.5)*1.5
    mesh.hide_render = True
    camera_data = bpy.data.cameras.new('PostureCamera')
    camera = bpy.data.objects.new('PostureCamera', camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (3, -8, 3)
    camera.rotation_euler = (Vector((0, 0, .6))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type, camera_data.ortho_scale = 'ORTHO', 7.8
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light, scene.display.shading.color_type = 'STUDIO', 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1800, 650, 100
    scene.render.filepath = str(out/'postures.png')
    bpy.ops.render.render(write_still=True)
    print('POSTURE ACTIONS EXPORTED', json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
