"""Preview and author CC0 Run Jump with FK bone lengths and no duplicated trajectory."""
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
import build_motion_probe as motion

OUT = SOURCE.parents[1]/'Saved/JumpAuthoring'
SOURCE_ACTION = sys.argv[sys.argv.index('--source')+1] if '--source' in sys.argv else 'Jump_2'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierIdle.blend'))
    rig, mesh, scene = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier'], bpy.context.scene
    protected_mesh, protected_rig = author.mesh_signature(mesh), author.reference(rig)
    idle_action = bpy.data.actions['A_IdleRelaxed']
    idle = author.sample_action(rig, idle_action, 0.)
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    scene.render.fps = 30
    data = (SOURCE/'External/Mesh2MotionProbe/static/animations/human-addon-animations.glb').read_bytes()
    assert hashlib.sha256(data).hexdigest() == 'a0d64d555e0d492026b72d58bf8e16c5e86779295f9093e376dcc001915c2c95'
    size, kind = struct.unpack_from('<II', data, 12)
    doc = json.loads(data[20:20+size])
    doc['animations'] = [a for a in doc['animations'] if a['name'] == SOURCE_ACTION]
    assert len(doc['animations']) == 1
    payload = json.dumps(doc, separators=(',', ':')).encode()
    payload += b' '*((-len(payload)) % 4)
    tail = data[20+size:]
    selected = OUT/'source.glb'
    selected.write_bytes(struct.pack('<III', 0x46546c67, 2, 20+len(payload)+len(tail))+struct.pack('<II', len(payload), kind)+payload+tail)
    previous = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(selected))
    imported = set(bpy.data.objects)-previous
    source = next(o for o in imported if o.type == 'ARMATURE')
    for obj in imported:
        if obj != source:
            bpy.data.objects.remove(obj, do_unlink=True)
    sr = {b.name: b.matrix_local.copy() for b in source.data.bones}
    align = {}
    for target, name in motion.MAPPING.items():
        q = rest[target].to_quaternion()
        if target not in ('pelvis', 'spine', 'chest', 'neck', 'head') and not target.startswith('clavicle_'):
            a, b = q @ Vector((0, 1, 0)), sr[name].to_quaternion() @ Vector((0, 1, 0))
            if target.startswith('hand_'):
                a = rest['finger_3_1_'+target[-1]].translation-rest[target].translation
                b = sr['middle_01_'+target[-1]].translation-sr[name].translation
            q = a.normalized().rotation_difference(b.normalized()) @ q
        align[target] = sr[name].to_quaternion().inverted() @ q

    def transfer(u):
        sp = author.sample_action(source, bpy.data.actions[SOURCE_ACTION], u)
        pose = {'root': rest['root'].copy()}
        for bone in rig.data.bones:
            n = bone.name
            if n == 'root':
                continue
            matrix = (sp[motion.MAPPING[n]].to_quaternion() @ align[n]).to_matrix().to_4x4()
            # FK, not absolute source ankle targets: the imported source has
            # different proportions and a trajectory already handled by gameplay.
            matrix.translation = idle[n].translation if n == 'pelvis' else pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
            pose[n] = matrix
        pose = author.collar_envelope(rig, rest, pose, idle, amount=0.)
        # Keep the courier's relaxed fingers, rather than the source's grip.
        # Local rotations preserve finger lengths and their palm attachment.
        for bone in rig.data.bones:
            n, parent = bone.name, bone.parent.name if bone.parent else None
            if n.startswith(('hand_', 'finger_')):
                local = idle[parent].inverted() @ idle[n]
                pose[n] = pose[parent] @ local
        # Moderate the source's deep stylized tuck while keeping a clear jump.
        return author.blend_local(rig, rest, idle, pose, .80)

    if '--export' in sys.argv:
        assert SOURCE_ACTION == 'Jump_2'
        profiles = {'A_JumpStart': (.18, False), 'A_JumpLoop': (.40, True), 'A_JumpFall': (.20, False)}
        flight = transfer(.20)
        frames_by_clip, report = {}, {}
        for name, (duration, loop) in profiles.items():
            samples = []
            for i in range(round(duration*100)+1):
                u = i/(duration*100)
                if name == 'A_JumpStart':
                    pose = author.blend_local(rig, rest, idle, flight, author.smooth(u))
                elif name == 'A_JumpLoop':
                    pose = transfer(.20+.04*(.5-.5*math.cos(math.tau*u)))
                else:
                    pose = author.blend_local(rig, rest, flight, transfer(.65), author.smooth(u))
                pose['root'] = rest['root'].copy()
                # No per-frame floor lifting or ankle pinning in the air:
                # those operations change the pelvis/leg relationship.
                author.rebuild_chain(rig, rest, pose)
                samples.append(pose)
            if loop:
                samples[-1] = author.copy_pose(samples[0])
            max_length_error = max(abs((p[b.name].translation-p[b.parent.name].translation).length-
                (rest[b.name].translation-rest[b.parent.name].translation).length)
                for p in samples for b in rig.data.bones if b.parent and b.name != 'pelvis')
            assert max_length_error < .00001, max_length_error
            generated = author.bake(rig, rest, name, samples)
            scene.render.fps = 100
            scene.frame_start, scene.frame_end = 1, len(samples)
            scene.frame_set(1)
            export(name, [rig], 'JumpActions', True)
            frames_by_clip[name] = samples
            report[name] = {'duration_s': duration, 'fps': 100, 'loop': loop, 'samples': len(samples),
                'max_joint_length_error_cm': max_length_error*100,
                'fbx_sha256': hashlib.sha256((SOURCE/'Exports/JumpActions'/f'{name}.fbx').read_bytes()).hexdigest()}
        assert author.mesh_signature(mesh) == protected_mesh and author.reference(rig) == protected_rig
        bpy.data.objects.remove(source, do_unlink=True)
        rig.animation_data.action = bpy.data.actions['A_JumpLoop']
        scene.frame_start, scene.frame_end = 1, 41
        scene.frame_set(1)
        bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierJump.blend'), compress=True)
        manifest = {'upstream': 'https://github.com/Mesh2Motion/mesh2motion-app',
            'source_revision': '653a9698f5f315523072f1c3ff3496100401317b', 'license': 'CC0-1.0',
            'source_action': SOURCE_ACTION, 'source_glb_sha256': hashlib.sha256(data).hexdigest(),
            'edits': 'FK-only proportional retarget; fixed pelvis/root trajectory; relaxed wrists/fingers; 80% anatomical motion; takeoff, small tuck loop, extension for descent',
            'rider_mesh_unchanged': True, 'skeleton_reference_unchanged': True, 'clips': report,
            'validation': {'status': 'passed', 'root_motion': False}}
        (SOURCE/'jump_actions.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
        candidates = [idle, frames_by_clip['A_JumpStart'][9], flight, frames_by_clip['A_JumpLoop'][20], frames_by_clip['A_JumpFall'][10], frames_by_clip['A_JumpFall'][-1]]
    else:
        candidates = [transfer(u) for u in (0., .15, .3, .45, .6, .75, .9)]

    for i, pose in enumerate(candidates):
        action = author.bake(rig, rest, 'Probe_'+str(i), [pose, pose])
        author.sample_action(rig, action, 0.)
        bpy.context.view_layer.update()
        copy = bpy.data.objects.new('Pose'+str(i), bpy.data.meshes.new_from_object(mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())))
        bpy.context.collection.objects.link(copy)
        copy.location.x = (i-(len(candidates)-1)/2)*1.1
    mesh.hide_render = True
    camera_data = bpy.data.cameras.new('Camera')
    camera = bpy.data.objects.new('Camera', camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (2, -10, 2.8)
    camera.rotation_euler = (Vector((0, 0, .9))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type, camera_data.ortho_scale = 'ORTHO', 8.8
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light, scene.display.shading.color_type = 'STUDIO', 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 2200, 700, 100
    scene.render.filepath = str(OUT/(SOURCE_ACTION.replace(' ', '-')+'-candidates.png'))
    bpy.ops.render.render(write_still=True)
    assert author.mesh_signature(mesh) == protected_mesh and author.reference(rig) == protected_rig


if __name__ == '__main__':
    main()
