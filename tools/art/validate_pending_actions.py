"""Measure actual baked rider deformation, continuity, roots and planted feet."""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_pending_actions as author
from art_common import SOURCE


def angle(a, b):
    value = a.rotation_difference(b).angle
    return math.degrees(min(value, math.tau-value))


def pose_error(a, b):
    return {'translation_cm': max((a[n].translation-b[n].translation).length*100 for n in a),
            'rotation_deg': max(angle(a[n].to_quaternion(), b[n].to_quaternion()) for n in a)}


def validate():
    report = json.loads((SOURCE/'pending_actions.json').read_text(encoding='utf-8'))
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPhotoreal.blend'))
    expected_mesh = author.mesh_signature(bpy.data.objects['SK_Courier'])
    expected_ref = author.reference(bpy.data.objects['CourierRig'])
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPendingActions.blend'))
    rig, mesh = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier']
    assert author.reference(rig) == expected_ref == [tuple(row) for row in report['source_reference']]
    assert author.mesh_signature(mesh) == expected_mesh == report['mesh_signature']
    for name, digest in report['protected_source_action_digests'].items():
        assert author.action_digest(bpy.data.actions[name]) == digest, ('Existing source action changed', name)
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    soles = author.sole_vertices(mesh)
    scene = bpy.context.scene
    idle = author.sample_action(rig, bpy.data.actions['A_Idle'], .35)
    sprint = author.sample_action(rig, bpy.data.actions['A_M2M_Run'], 0.)
    result, endpoints = {}, {}
    for name, profile in author.PROFILES.items():
        action = bpy.data.actions[name]
        count = round(profile['duration_s']*author.FPS)
        assert tuple(action.frame_range) == (1., float(count+1))
        assert abs((action.frame_range[1]-action.frame_range[0])/scene.render.fps-profile['duration_s']) < 1e-8
        assert hashlib.sha256((SOURCE/'Exports/PendingActions'/f'{name}.fbx').read_bytes()).hexdigest() == report['clips'][name]['fbx_sha256']
        poses, feet, pelvis, lean = [], {s: [] for s in soles}, [], []
        max_chain_error, root_angle, root_distance, max_deform_extent = 0., 0., 0., 0.
        for index in range(count+1):
            pose = author.sample_action(rig, action, index/count)
            assert all(math.isfinite(v) for mat in pose.values() for row in mat for v in row)
            root_distance = max(root_distance, (pose['root'].translation-rest['root'].translation).length)
            root_angle = max(root_angle, angle(pose['root'].to_quaternion(), rest['root'].to_quaternion()))
            for bone in rig.data.bones:
                if bone.parent and bone.name != 'pelvis':
                    expected = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local
                    max_chain_error = max(max_chain_error, (pose[bone.name].translation-expected).length)
            for side, ids in soles.items():
                points = [author.deformed_point(mesh, rest, pose, i) for i in ids]
                feet[side].append({'low_z': min(p.z for p in points), 'center': sum(points, Vector())/len(points)})
            # Inspect a deterministic sample throughout the real formal mesh,
            # including portrait, workwear, fingers and shoes, using its actual
            # vertex weights (no proxy stick figure deformation).
            for vertex in list(mesh.data.vertices)[::max(1, len(mesh.data.vertices)//500)]:
                point = author.deformed_point(mesh, rest, pose, vertex.index)
                assert all(math.isfinite(v) for v in point)
                max_deform_extent = max(max_deform_extent, point.length)
            pelvis.append(pose['pelvis'].translation.z)
            lean.append(pose['head'].translation.y-pose['pelvis'].translation.y)
            poses.append(pose)
        endpoint = pose_error(poses[0], poses[-1])
        max_step = max(max((b[n].translation-a[n].translation).length for n in a) for a, b in zip(poses, poses[1:]))
        max_angle = max(max(angle(a[n].to_quaternion(), b[n].to_quaternion()) for n in a) for a, b in zip(poses, poses[1:]))
        lowest = min(f['low_z'] for samples in feet.values() for f in samples)
        foot_travel = max(max((f['center']-samples[0]['center']).length for f in samples) for samples in feet.values())
        print('CLIP METRICS', name, 'step', max_step, 'angle', max_angle, 'chain', max_chain_error, 'sole', lowest, 'feet', foot_travel,
              'worst', sorted(((max((b[n].translation-a[n].translation).length for a, b in zip(poses, poses[1:])), n) for n in poses[0]), reverse=True)[:3],
              'angular_worst', sorted(((angle(a[n].to_quaternion(), b[n].to_quaternion()), n, i) for i, (a,b) in enumerate(zip(poses,poses[1:])) for n in poses[0]), reverse=True)[:3], flush=True)
        assert root_distance < 1e-6 and root_angle < .01, ('Animated root', name, root_distance, root_angle)
        assert max_chain_error < 2e-5, ('Detached/stretched joint', name, max_chain_error)
        assert lowest >= -.0002, ('Shoe penetration', name, lowest)
        assert max_deform_extent < 2.5, ('Deformation explosion', name, max_deform_extent)
        assert max_step < .12 and max_angle < 30, ('Discontinuous pose', name, max_step, max_angle)
        if profile['loop']:
            assert endpoint['translation_cm'] < .001 and endpoint['rotation_deg'] < .02
            seam = max(((poses[1][n].translation-poses[0][n].translation)-(poses[-1][n].translation-poses[-2][n].translation)).length for n in poses[0])
            assert seam < .01, ('Visible loop velocity seam', seam)
        if name in ('A_Land', 'A_HitReact_Front', 'A_HitReact_Back'):
            assert foot_travel < .007, ('Planted foot sliding', name, foot_travel)
            assert endpoint['translation_cm'] < .001 and endpoint['rotation_deg'] < .02
        result[name] = {'duration_s': count/author.FPS, 'samples': count+1,
                        'root_translation_cm': root_distance*100, 'root_rotation_deg': root_angle,
                        'max_joint_attachment_error_cm': max_chain_error*100,
                        'lowest_sole_cm': lowest*100, 'max_sole_center_travel_cm': foot_travel*100,
                        'max_joint_step_cm': max_step*100, 'max_joint_angular_step_deg': max_angle,
                        'pelvis_range_cm': (max(pelvis)-min(pelvis))*100,
                        'head_relative_forward_y_range_cm': [min(lean)*100, max(lean)*100],
                        'endpoint_error': endpoint, 'deformed_vertex_sample_count_per_frame': 500,
                        'max_deformed_vertex_distance_m': max_deform_extent}
        endpoints[name] = poses
    assert result['A_JumpStart']['pelvis_range_cm'] > 5, 'Missing anticipation compression'
    assert result['A_Land']['pelvis_range_cm'] > 5, 'Missing landing compression'
    idle_lean = idle['head'].translation.y-idle['pelvis'].translation.y
    assert max(p['head'].translation.y-p['pelvis'].translation.y for p in endpoints['A_HitReact_Front']) > idle_lean+.05
    assert min(p['head'].translation.y-p['pelvis'].translation.y for p in endpoints['A_HitReact_Back']) < idle_lean-.05
    run_start_error = pose_error(endpoints['A_RunStart'][-1], sprint)
    run_stop_error = pose_error(endpoints['A_RunStop'][0], sprint)
    # Re-solving the unchanged rig's legs removes the older retargeter's
    # submillimeter ankle attachment error. Match its delivered sprint pose
    # within 0.5 mm / 0.1 degree rather than retaining that detached offset.
    assert run_start_error['translation_cm'] < .05 and run_start_error['rotation_deg'] < .1, run_start_error
    assert run_stop_error['translation_cm'] < .05 and run_stop_error['rotation_deg'] < .1, run_stop_error
    report['validation'] = {'status': 'passed', 'reference_bones': len(expected_ref),
                            'formal_mesh_unchanged': True, 'existing_source_actions_unchanged': True,
                            'sprint_start_endpoint_error': run_start_error,
                            'sprint_stop_endpoint_error': run_stop_error, 'clips': result,
                            'limits': 'Source numerical validation. Runtime/camera acceptance still requires UE capture.'}
    (SOURCE/'pending_actions.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('PENDING SOURCE VALIDATION', json.dumps(report['validation']), flush=True)


def preview():
    """Source-only pose sheet on the actual formal rider; never saves a .blend."""
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPendingActions.blend'))
    scene = bpy.context.scene
    rig, mesh = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier']
    poses = []
    phases = [.34, .50, .28, .22, .22, .65, .40]
    for name, u in zip(author.PROFILES, phases):
        poses.append((name, author.sample_action(rig, bpy.data.actions[name], u)))
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    for i, (name, pose) in enumerate(poses):
        actor = rig.copy()
        actor.data = rig.data
        actor.animation_data_clear()
        scene.collection.objects.link(actor)
        actor.location.x = i*1.25
        for bone in actor.data.bones:
            basis = pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ rest[bone.name] if bone.parent else rest[bone.name]
            actor.pose.bones[bone.name].matrix_basis = basis.inverted() @ pose[bone.name]
        model = mesh.copy()
        model.data = mesh.data
        model.parent = actor
        scene.collection.objects.link(model)
        for mod in model.modifiers:
            if mod.type == 'ARMATURE':
                mod.object = actor
        curve = bpy.data.curves.new(name, 'FONT')
        curve.body = name.replace('A_', '').replace('HitReact_', 'Hit ')
        curve.align_x = 'CENTER'
        curve.size = .11
        text = bpy.data.objects.new(name, curve)
        scene.collection.objects.link(text)
        text.location = (i*1.25, -.35, -.18)
        text.rotation_euler = (math.pi/2, 0, 0)
    rig.hide_render = True
    mesh.hide_render = True
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.studiolight_rotate_z = .4
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.background_type = 'WORLD'
    scene.world.color = (.12, .12, .12)
    bpy.ops.object.camera_add(location=(4.8, -13., 4.2))
    camera = bpy.context.object
    camera.rotation_euler = (Vector((3.75, 0., .85))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 9.2
    scene.camera = camera
    scene.render.resolution_x = 2520
    scene.render.resolution_y = 840
    scene.render.resolution_percentage = 100
    folder = SOURCE.parents[1]/'Saved/PendingActions'
    folder.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(folder/'source-pose-sheet.png')
    bpy.ops.render.render(write_still=True)
    print('SOURCE PREVIEW', scene.render.filepath, flush=True)


def collar_diagnostics():
    """Compare actually bound skin and shirt; do not mutate either mesh.

    Signed nearest-shirt distance is a local diagnostic, not a general closed
    mesh containment test (the shirt has a real open neckline and sleeves).
    Candidates are restricted to skin that is measurably beneath the shirt in
    the production idle, avoiding naturally exposed neck/arm vertices.
    """
    blend_hash = hashlib.sha256((SOURCE/'Blender/CourierPendingActions.blend').read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPendingActions.blend'))
    rig, mesh = bpy.data.objects['CourierRig'], bpy.data.objects['SK_Courier']
    if 'A_Idle' not in bpy.data.actions:
        with bpy.data.libraries.load(str(SOURCE/'Blender/Mesh2MotionProbe.blend'), link=False) as (src, dst):
            dst.actions = ['A_Idle']
    rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
    skin_materials = {'M_PortraitSkinBody', 'M_PortraitSkinHead'}
    skin = {i for p in mesh.data.polygons if mesh.data.materials[p.material_index].name in skin_materials for i in p.vertices
            if 1.30 < mesh.data.vertices[i].co.z < 1.62 and abs(mesh.data.vertices[i].co.x) < .30}
    cloth = [list(p.vertices) for p in mesh.data.polygons if mesh.data.materials[p.material_index].name == 'M_WorkClothes'
             and min(mesh.data.vertices[i].co.z for i in p.vertices) > 1.10]
    cloth_ids = {i for poly in cloth for i in poly}
    skin_faces = [list(p.vertices) for p in mesh.data.polygons if mesh.data.materials[p.material_index].name in skin_materials
                  and min(mesh.data.vertices[i].co.z for i in p.vertices) > 1.20]
    skin_surface_ids = {i for poly in skin_faces for i in poly}
    ids = sorted(skin | cloth_ids | skin_surface_ids)

    def surface(pose):
        deform = {n: pose[n] @ rest[n].inverted() for n in rest}
        points = {}
        for i in ids:
            vertex = mesh.data.vertices[i]
            points[i] = sum(((deform[mesh.vertex_groups[g.group].name] @ vertex.co)*g.weight for g in vertex.groups), Vector())
        order = sorted(cloth_ids)
        lookup = {v: i for i, v in enumerate(order)}
        tree = BVHTree.FromPolygons([points[i] for i in order], [[lookup[i] for i in p] for p in cloth])
        order = sorted(skin_surface_ids)
        lookup = {v: i for i, v in enumerate(order)}
        skin_tree = BVHTree.FromPolygons([points[i] for i in order], [[lookup[i] for i in p] for p in skin_faces])
        return points, tree, skin_tree

    idle = author.sample_action(rig, bpy.data.actions['A_Idle'], .35)
    original = author.sample_action(rig, bpy.data.actions['A_JumpLoop'], .50)
    baseline_points, baseline_tree, baseline_skin = surface(idle)
    covered = []
    for i in skin:
        nearest, normal, _, distance = baseline_tree.find_nearest(baseline_points[i])
        signed = (baseline_points[i]-nearest).dot(normal)
        if -.025 < signed < -.001 and distance < .03:
            covered.append(i)

    def local_rotations(pose):
        return {b.name: (pose[b.parent.name].to_quaternion().inverted() @ pose[b.name].to_quaternion() if b.parent else pose[b.name].to_quaternion()) for b in rig.data.bones}
    baseline_rotations = local_rotations(idle)
    original_rotations = local_rotations(original)

    def with_local_baseline(names):
        pose = author.copy_pose(original)
        for bone in rig.data.bones:
            name = bone.name
            q = baseline_rotations[name] if name in names else original_rotations[name]
            if bone.parent:
                q = pose[bone.parent.name].to_quaternion() @ q
            matrix = q.to_matrix().to_4x4()
            matrix.translation = (original[name].translation if name in ('root', 'pelvis') else
                                  pose[bone.parent.name] @ rest[bone.parent.name].inverted() @ bone.head_local)
            pose[name] = matrix
        return pose

    jobs = {'production_idle': idle, 'jump_loop': original,
            'retarget_idle': author.sample_action(rig, bpy.data.actions['A_M2M_Idle'], 0.)}
    for name in ('spine', 'chest', 'neck', 'head', 'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r'):
        jobs['restore_local_'+name] = with_local_baseline({name})
    jobs['restore_both_clavicles'] = with_local_baseline({'clavicle_l', 'clavicle_r'})
    jobs['restore_chest_neck'] = with_local_baseline({'chest', 'neck'})
    jobs['restore_pelvis_chest_neck'] = with_local_baseline({'pelvis', 'chest', 'neck'})
    jobs['restore_upperbody'] = with_local_baseline({'spine', 'chest', 'neck', 'head', 'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r'})
    jobs['restore_neck_clavicles'] = with_local_baseline({'neck', 'clavicle_l', 'clavicle_r'})
    jobs['restore_chest_neck_clavicles'] = with_local_baseline({'chest', 'neck', 'clavicle_l', 'clavicle_r'})
    jobs['restore_chest_neck_clavicles_head'] = with_local_baseline({'chest', 'neck', 'head', 'clavicle_l', 'clavicle_r'})
    source_idle = author.sample_action(rig, bpy.data.actions['A_M2M_Idle'], 0.)
    jobs['calibrated_motion_local_offsets'] = author.calibrate_pose(rig, rest, original, source_idle, idle)
    for factor in (0., .25, .5, .75):
        jobs['calibrated_collar_envelope_'+str(factor)] = author.collar_envelope(rig, rest,
            author.copy_pose(jobs['calibrated_motion_local_offsets']), idle, factor)
    for u in (.0, .25, .5, .75):
        jobs['existing_sprint_'+str(u)] = author.sample_action(rig, bpy.data.actions['A_M2M_Run'], u)
    for name, u in (('A_JumpStart', .67), ('A_Land', .28), ('A_HitReact_Front', .22), ('A_HitReact_Back', .22), ('A_RunStart', .75), ('A_RunStop', .48)):
        jobs['delivered_'+name] = author.sample_action(rig, bpy.data.actions[name], u)
    all_frames = '--collar-all' in sys.argv
    if all_frames:
        jobs = {'production_idle': idle}
        for name, profile in author.PROFILES.items():
            count = round(profile['duration_s']*author.FPS)
            for frame in range(count+1):
                jobs[name+'@'+str(frame)] = author.sample_action(rig, bpy.data.actions[name], frame/count)
        for frame in range(26):
            jobs['existing_sprint@'+str(frame)] = author.sample_action(rig, bpy.data.actions['A_M2M_Run'], frame/25)
    cameras = {'front': Vector((.8, -5.5, 1.5)), 'side_l': Vector((5.5, -.35, 1.5)), 'side_r': Vector((-5.5, -.35, 1.5))}
    def visible_to(tree, point, camera):
        delta = point-camera
        hit, normal, index, distance = tree.ray_cast(camera, delta.normalized(), delta.length+.003)
        return distance is None or distance >= delta.length-.001
    baseline_hidden = {name: [i for i in skin if not visible_to(baseline_tree, baseline_points[i], cam)
                             and visible_to(baseline_skin, baseline_points[i], cam)] for name, cam in cameras.items()}
    results = {}
    for name, pose in jobs.items():
        points, tree, skin_tree = surface(pose)
        poking = []
        for i in covered:
            nearest, normal, _, distance = tree.find_nearest(points[i])
            signed = (points[i]-nearest).dot(normal)
            if signed > .001 and distance < .08:
                poking.append((signed, i))
        poking.sort(reverse=True)
        results[name] = {'covered_idle_skin_vertices_over_1mm_outside_nearest_shirt': len(poking),
                         'max_signed_distance_cm': poking[0][0]*100 if poking else 0.,
                         'worst_vertex_samples': [{'index': i, 'signed_distance_cm': d*100,
                             'rest_xyz': list(mesh.data.vertices[i].co), 'posed_xyz': list(points[i]),
                             'weights': {mesh.vertex_groups[g.group].name: g.weight for g in mesh.data.vertices[i].groups}} for d, i in poking[:10]]}
        results[name]['newly_uncovered_baseline_clothed_skin'] = {label: [i for i in candidates if visible_to(tree, points[i], cameras[label])
            and visible_to(skin_tree, points[i], cameras[label])] for label, candidates in baseline_hidden.items()}
    rotation_changes = {name: angle(original_rotations[name], baseline_rotations[name]) for name in original_rotations}
    report = {'method': 'Signed nearest-shirt distances of skin vertices covered in actual production Idle; open neckline is explicitly excluded by baseline selection.',
              'candidate_skin_vertices': len(skin), 'baseline_covered_vertices': len(covered), 'comparisons': results,
              'local_rotation_difference_from_production_idle_deg': rotation_changes,
              'source_blend_sha256': blend_hash,
              'unchanged_mesh_signature': author.mesh_signature(mesh)}
    if all_frames:
        summary = {}
        for label in list(author.PROFILES)+['existing_sprint']:
            rows = [v for key, v in results.items() if key.startswith(label+'@')]
            summary[label] = {'frames': len(rows), 'max_newly_visible_collar_candidates':
                {view: max(len(row['newly_uncovered_baseline_clothed_skin'][view]) for row in rows) for view in cameras},
                'max_signed_nearest_shirt_distance_cm': max(row['max_signed_distance_cm'] for row in rows)}
        report['all_baked_frame_summary'] = summary
    out = SOURCE.parents[1]/('Saved/PendingActions/collar-validation.json' if all_frames else 'Saved/PendingActions/collar-diagnostics.json')
    out.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('COLLAR DIAGNOSTICS', json.dumps(summary if all_frames else {name: {k: ({side: len(ids) for side, ids in v.items()} if k == 'newly_uncovered_baseline_clothed_skin' else v) for k, v in r.items() if k != 'worst_vertex_samples'} for name, r in results.items()}), flush=True)


def preview_angles():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierPendingActions.blend'))
    scene = bpy.context.scene
    rig = bpy.data.objects['CourierRig']
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_cavity = True
    scene.display.shading.show_shadows = True
    scene.display.shading.background_type = 'WORLD'
    scene.world.color = (.15, .15, .15)
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 2.4
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = 840, 1200
    scene.render.resolution_percentage = 100
    folder = SOURCE.parents[1]/'Saved/PendingActions/SourceAngles'
    folder.mkdir(parents=True, exist_ok=True)
    for name, u in zip(author.PROFILES, (.34, .5, .28, .22, .22, .75, .48)):
        author.sample_action(rig, bpy.data.actions[name], u)
        for angle_name, position in (('front', (.45, -5.5, 1.5)), ('side', (-5.5, -.35, 1.5))):
            camera.location = position
            camera.rotation_euler = (Vector((0, 0, .93))-camera.location).to_track_quat('-Z', 'Y').to_euler()
            scene.render.filepath = str(folder/(name+'-'+angle_name+'.png'))
            bpy.ops.render.render(write_still=True)
    print('SOURCE ANGLE PREVIEWS', str(folder), flush=True)


if __name__ == '__main__':
    if '--collar' in sys.argv or '--collar-all' in sys.argv:
        collar_diagnostics()
    elif '--preview' in sys.argv:
        preview()
    elif '--preview-angles' in sys.argv:
        preview_angles()
    else:
        validate()
