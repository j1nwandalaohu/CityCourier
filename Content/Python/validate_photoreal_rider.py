"""Photoreal courier contract checks; run in UE, or with --self-test in CPython."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ART = '/Game/CityCourier/Art'
PORTRAIT = ART + '/Portrait'
MESH = ART + '/Characters/SK_Courier'
SKELETON = ART + '/Characters/SK_Courier_Skeleton'
BASELINE = PORTRAIT + '/Baseline/SK_CourierBefore'


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def file_hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def transform_record(transform):
    t, q, s = transform.translation, transform.rotation, transform.scale3d
    return {'translation': [t.x, t.y, t.z], 'rotation': [q.x, q.y, q.z, q.w],
            'scale': [s.x, s.y, s.z]}


def skeleton_rest(unreal, skeleton):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    check(unreal.AnimPoseExtensions.is_valid(pose), 'Cannot read shared skeleton rest pose')
    names = list(unreal.AnimPoseExtensions.get_bone_names(pose))
    result = {}
    for name in names:
        record = transform_record(unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL))
        result[str(name)] = record
    return result


def skeleton_metadata(skeleton, mesh):
    """Public socket APIs; slots/retarget policy are protected by package hashes.

    Skeleton's internal slot, bone tree and socket arrays are protected in
    UE5.6 Python. Mesh APIs enumerate both mesh and shared skeleton sockets.
    """
    sockets = []
    for index in range(mesh.num_sockets()):
        socket = mesh.get_socket_by_index(index)
        sockets.append({'name': str(socket.get_editor_property('socket_name')),
                        'bone': str(socket.get_editor_property('bone_name')),
                        'transform': transform_record(socket.get_socket_local_transform()),
                        'always_animated': socket.get_editor_property('force_always_animated')})
    return {'sockets': sockets,
            'compatible_skeletons': [str(item) for item in skeleton.get_editor_property('compatible_skeletons')]}


def mesh_rest(unreal, mesh):
    component = unreal.new_object(unreal.SkeletalMeshComponent)
    component.set_skeletal_mesh_asset(mesh)
    result = {}
    for index in range(component.get_num_bones()):
        name = component.get_bone_name(index)
        record = transform_record(component.get_ref_pose_transform(index))
        record['parent'] = str(component.get_parent_bone(name))
        result[str(name)] = record
    check(result, 'Mesh has no bones: ' + mesh.get_path_name())
    return result


def compare_rest(before, after, label, translation_tolerance=.002, angular_tolerance=.00002):
    check(set(before) == set(after), label + ': bone roster changed; missing=' +
          str(sorted(set(before) - set(after))) + '; added=' + str(sorted(set(after) - set(before))))
    for name, a in before.items():
        b = after[name]
        if 'parent' in a:
            check(a['parent'] == b.get('parent'), label + ': parent changed: ' + name)
        for key, tolerance in [('translation', translation_tolerance), ('scale', .00002)]:
            check(max(abs(x-y) for x, y in zip(a[key], b[key])) <= tolerance,
                  label + ': ' + key + ' changed: ' + name)
        # q and -q represent the same rotation.
        qa, qb = a['rotation'], b['rotation']
        distance = min(max(abs(x-y) for x, y in zip(qa, qb)),
                       max(abs(x+y) for x, y in zip(qa, qb)))
        check(distance <= angular_tolerance, label + ': rotation changed: ' + name)


def protected_files(root):
    """Hash every existing animation/Blueprint and the original material package."""
    result = {}
    for folder in ['Content/CityCourier/Art/Characters', 'Content/CityCourier/Blueprints']:
        for path in sorted((root/folder).rglob('*.uasset')):
            if path.stem != 'SK_Courier':
                result[str(path.relative_to(root))] = file_hash(path)
    skin = root/'Content/CityCourier/Art/Materials/M_Skin.uasset'
    check(skin.is_file(), 'Original M_Skin is missing')
    result[str(skin.relative_to(root))] = file_hash(skin)
    return result


def input_graph(unreal, material, prop):
    """Follow actual output connections rather than counting unconnected nodes."""
    editor = unreal.MaterialEditingLibrary
    pending = [editor.get_material_property_input_node(material, prop)]
    visited = {}
    while pending:
        node = pending.pop()
        if node is None or node.get_path_name() in visited:
            continue
        visited[node.get_path_name()] = node
        pending.extend(editor.get_inputs_for_material_expression(material, node))
    return list(visited.values())


def strict_mask_channel(flags):
    enabled = [channel.upper() for channel in 'rgba' if flags.get(channel, False)]
    check(len(enabled) == 1, 'ComponentMask must select exactly one scalar channel: ' + str(flags))
    return enabled[0]


def report_file_paths(root):
    source, saved = root/'AssetsSource/CityCourier', root/'Saved/Validation/Portrait'
    baseline, current = source/'portrait_baseline.json', source/'portrait_import_validation.json'
    return (baseline if baseline.is_file() else saved/'import.original.json',
            current if current.is_file() else saved/'import.json')


def protection_record(report, key):
    """New imports protect live before/after state; old reports retain compatibility."""
    return report.get('run_'+key, report[key])


def material_masks(unreal, material, prop):
    result = []
    for node in input_graph(unreal, material, prop):
        if isinstance(node, unreal.MaterialExpressionComponentMask):
            flags = {channel: node.get_editor_property(channel) for channel in 'rgba'}
            result.append((node, strict_mask_channel(flags)))
    return result


def validate(unreal, report=None):
    root = Path(unreal.Paths.project_dir()).resolve()
    archive, current_report = report_file_paths(root)
    if report is None:
        check(archive.is_file(), 'Persistent original portrait baseline report is missing')
        report = json.loads(current_report.read_text(encoding='utf-8'))
    if archive.is_file():
        original = json.loads(archive.read_text(encoding='utf-8'))
        for key in ('protected_files', 'mesh_rest', 'skeleton_rest', 'skeleton_metadata'):
            check(report.get(key) == original.get(key), 'First-import protection record was overwritten: ' + key)
    mesh, skeleton = unreal.load_asset(MESH), unreal.load_asset(SKELETON)
    check(mesh and skeleton, 'Production mesh/skeleton is missing')
    check(mesh.get_editor_property('skeleton') == skeleton, 'Courier uses a different skeleton')
    compare_rest(protection_record(report, 'skeleton_rest'), skeleton_rest(unreal, skeleton), 'Shared skeleton')
    check(protection_record(report, 'skeleton_metadata') == skeleton_metadata(skeleton, mesh), 'Shared skeleton socket metadata changed during import')
    compare_rest(protection_record(report, 'mesh_rest'), mesh_rest(unreal, mesh), 'Production mesh')
    check(protected_files(root) == protection_record(report, 'protected_files'),
          'A live protected package was changed, created or removed during import')
    baseline = unreal.load_asset(BASELINE)
    check(baseline and baseline.get_editor_property('skeleton') == skeleton, 'Baseline is missing or changed rig')
    compare_rest(report['mesh_rest'], mesh_rest(unreal, baseline), 'Preserved baseline rig')
    check(any(slot.material_interface and slot.material_interface.get_path_name().startswith(ART+'/Materials/M_Skin.')
              for slot in baseline.get_editor_property('materials')), 'Baseline no longer references original M_Skin')
    profile = unreal.load_asset(PORTRAIT+'/SSP_CourierSkin')
    check(isinstance(profile, unreal.SubsurfaceProfile), 'No actual skin SubsurfaceProfile asset')
    settings = profile.get_editor_property('settings')
    check(settings.get_editor_property('enable_burley'), 'Skin is not using Burley scattering')
    check(.05 <= settings.get_editor_property('mean_free_path_distance') <= .25,
          'Skin scattering distance is outside the bounded human skin range')
    check(settings.get_editor_property('world_unit_scale') == 1.0, 'Skin profile units must be cm')
    check(settings.get_editor_property('roughness0') < settings.get_editor_property('roughness1'),
          'Dual skin specular lobes are missing')
    definitions = json.loads((root/'AssetsSource/CityCourier/portrait_materials.json').read_text(encoding='utf-8'))['materials']
    check(file_hash(root/'AssetsSource/CityCourier/portrait_materials.json') == report['source_materials_sha256'],
          'Material contract changed since import')
    check(file_hash(root/'AssetsSource/CityCourier/Exports/Portrait/SK_Courier.fbx') == report['source_fbx_sha256'],
          'FBX source changed since import')
    if 'source_catalog_sha256' in report:
        check(file_hash(root/'AssetsSource/CityCourier/portrait_rider.json') == report['source_catalog_sha256'],
              'Portrait source catalog changed since import')
    actual = {}
    for slot in mesh.get_editor_property('materials'):
        name = str(slot.get_editor_property('material_slot_name'))
        check(slot.material_interface, 'Unassigned material: ' + name)
        expected = ART+'/Materials/'+name
        check(slot.material_interface.get_path_name().split('.')[0] == expected, 'Wrong material on ' + name)
        actual[name] = slot.material_interface.get_path_name()
    check(set(definitions) <= set(actual), 'Portrait materials absent from imported mesh')
    for name, definition in definitions.items():
        material = unreal.load_asset(ART+'/Materials/'+name)
        check(material, 'Missing portrait material ' + name)
        # Inspect actual stored booleans; UMaterialExpressionComponentMask's
        # RG defaults otherwise turn R/B blend weights into vector masks.
        base_masks = material_masks(unreal, material, unreal.MaterialProperty.MP_BASE_COLOR)
        rough_masks = material_masks(unreal, material, unreal.MaterialProperty.MP_ROUGHNESS)
        check(all(channel == 'R' for node, channel in rough_masks), name + ': roughness mask must be scalar R')
        if definition['kind'].startswith('skin_'):
            check(material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_SUBSURFACE_PROFILE,
                  name + ': incorrect skin shading model')
            check(material.get_editor_property('subsurface_profile') == profile, name + ': missing skin profile')
        check(set(report['textures'].get(name, {})) == set(definition.get('textures', {})),
              name + ': imported texture records are incomplete')
        for key, record in report['textures'].get(name, {}).items():
            texture = unreal.load_asset(record['asset'])
            # GetUsedTextures reads compiled render resources, which are empty
            # in Python commandlets. Check each saved output graph below.
            check(texture, name + ': missing map ' + key)
            check(texture.get_editor_property('srgb') == (key in ('base_color', 'neck_base_color')),
                  name + ': incorrect texture color space for ' + key)
            if key == 'normal':
                check(texture.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_NORMALMAP,
                      name + ': normal map is not using normal compression')
                check(texture.get_editor_property('flip_green_channel') == record['flip_green'],
                      name + ': incorrect normal handedness')
            check(file_hash(root/'AssetsSource/CityCourier'/record['source']) == record['sha256'],
                  name + ': source texture changed since import')
            prop = unreal.MaterialProperty.MP_NORMAL if key == 'normal' else unreal.MaterialProperty.MP_ROUGHNESS if key == 'roughness' else unreal.MaterialProperty.MP_BASE_COLOR
            graph = input_graph(unreal, material, prop)
            samples = [node for node in graph if isinstance(node, unreal.MaterialExpressionTextureSample)
                       and node.get_editor_property('texture') == texture]
            check(samples, name + ': map does not reach its correct output: ' + key)
            for sample in samples:
                coords = [node for node in unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, sample)
                          if isinstance(node, unreal.MaterialExpressionTextureCoordinate)]
                check(len(coords) == 1, name + ': missing explicit UV channel on ' + key)
                uv = coords[0]
                check(uv.get_editor_property('coordinate_index') == (1 if key == 'neck_base_color' else 0),
                      name + ': wrong UV channel on ' + key)
                scale = float(definition.get('normal_uv_scale', 1)) if key == 'normal' else 1.0
                check(abs(uv.get_editor_property('u_tiling')-scale) < .0001 and
                      abs(uv.get_editor_property('v_tiling')-scale) < .0001,
                      name + ': wrong UV tiling on ' + key)
        for prop in [unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_ROUGHNESS]:
            check(unreal.MaterialEditingLibrary.get_material_property_input_node(material, prop),
                  name + ': disconnected ' + str(prop))
        if 'normal' in definition.get('textures', {}):
            check(unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_NORMAL),
                  name + ': disconnected normal output')
        if definition['kind'] == 'hair':
            check(material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_HAIR,
                  name + ': missing physical hair shader')
            check(len(base_masks) == 1 and base_masks[0][1] == 'G', name + ': root-tip mask must select only V/G')
        if definition['kind'] == 'skin_head':
            check(any(isinstance(node, unreal.MaterialExpressionVertexColor)
                      for node in input_graph(unreal, material, unreal.MaterialProperty.MP_BASE_COLOR)),
                  name + ': neck/scalp vertex masks are missing')
            check(sorted(channel for node, channel in base_masks) == ['B', 'R'],
                  name + ': neck/scalp masks must select scalar R and scalar B')
        if definition['kind'] == 'sclera' and definition.get('alpha_mask'):
            check(material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_MASKED,
                  name + ': missing sclera alpha mask')
        if definition['kind'] in ('cornea', 'tearline'):
            check(material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT,
                  name + ': surface is not transparent')
            refraction = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_REFRACTION)
            check(isinstance(refraction, unreal.MaterialExpressionConstant) and abs(refraction.get_editor_property('r')-1.0) < .00001,
                  name + ': screen-space refraction must have zero distortion (IOR output 1.0)')
            specular = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_SPECULAR)
            ior = float(definition.get('ior', 1.336 if definition['kind'] == 'tearline' else 1.376))
            expected = ((ior-1)/(ior+1))**2/.08
            check(isinstance(specular, unreal.MaterialExpressionConstant) and abs(specular.get_editor_property('r')-expected) < .00001,
                  name + ': transparent eye coat lost its physical Fresnel reflectance')
    return {'materials': actual, 'bone_count': len(protection_record(report, 'skeleton_rest')),
            'protected_packages': len(protection_record(report, 'protected_files')), 'profile': profile.get_path_name()}


def self_test():
    import copy
    import unittest

    class ContractTests(unittest.TestCase):
        def setUp(self):
            path = Path(__file__).with_name('import_photoreal_rider.py')
            self.assertTrue(path.is_file(), 'Portrait importer is missing')
            spec = importlib.util.spec_from_file_location('portrait_import', path)
            self.module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(self.module)

        def test_texture_path_must_stay_in_source(self):
            root = Path(__file__).parent.resolve()
            with self.assertRaises(RuntimeError):
                self.module.source_texture(root, '../outside.png')

        def test_channel_selection_explicitly_clears_all_other_component_defaults(self):
            for requested in 'RGBA':
                expected = {channel.lower(): channel == requested for channel in 'RGBA'}
                self.assertEqual(self.module.single_channel_mask(requested), expected)
                self.assertEqual(strict_mask_channel(expected), requested)
            for bad in [{'r': True, 'g': True, 'b': False, 'a': False},
                        {'r': True, 'g': True, 'b': True, 'a': False}]:
                with self.assertRaises(RuntimeError):
                    strict_mask_channel(bad)

        def test_transparent_eye_coat_preserves_fresnel_without_screen_space_distortion(self):
            specular, refraction = self.module.eye_coat_optics({'ior': 1.376})
            self.assertAlmostEqual(specular*.08, ((1.376-1)/(1.376+1))**2)
            self.assertEqual(refraction, 1.0)

        def test_open_gl_normal_requires_green_flip(self):
            self.assertTrue(self.module.normal_flip({'normal_space': 'OpenGL'}))
            self.assertFalse(self.module.normal_flip({'normal_space': 'DirectX'}))
            with self.assertRaises(RuntimeError):
                self.module.normal_flip({'normal_space': 'unknown'})

        def test_rest_comparison_rejects_new_bone_or_rotation(self):
            a = {'root': {'translation': [0, 0, 0], 'rotation': [0, 0, 0, 1], 'scale': [1, 1, 1], 'parent': 'None'}}
            compare_rest(a, a, 'unchanged')
            b = copy.deepcopy(a)
            b['root']['rotation'] = [0, 0, 0, -1]
            compare_rest(a, b, 'equivalent quaternion')
            b['extra'] = b['root']
            with self.assertRaises(RuntimeError):
                compare_rest(a, b, 'new bone')
            b = copy.deepcopy(a)
            b['root']['rotation'] = [0, .02, 0, .9998]
            with self.assertRaises(RuntimeError):
                compare_rest(a, b, 'changed rotation')

        def test_metadata_requires_complete_skin_maps(self):
            with self.assertRaises(RuntimeError):
                self.module.validate_definitions({'materials': {'M_PortraitSkinHead': {'kind': 'skin_head', 'textures': {}}}})

        def test_mesh_only_revision_preserves_original_protection_records(self):
            original = {'protected_files': {'skeleton': 'initial-hash'}, 'mesh_rest': {'root': 'rest'},
                        'skeleton_rest': {'root': 'reference'}, 'skeleton_metadata': {'sockets': []},
                        'textures': {'skin': {'normal': 'original-normal'}}, 'source_materials_sha256': 'material-hash',
                        'source_fbx_sha256': 'old-fbx', 'validation': {'bone_count': 51}}
            snapshot = copy.deepcopy(original)
            updated = self.module.mesh_only_report(original, 'new-fbx', 'new-catalog')
            self.assertEqual(original, snapshot)
            for key in ['protected_files', 'mesh_rest', 'skeleton_rest', 'skeleton_metadata', 'textures', 'source_materials_sha256']:
                self.assertEqual(updated[key], original[key])
            self.assertEqual(updated['source_fbx_sha256'], 'new-fbx')
            self.assertEqual(updated['source_catalog_sha256'], 'new-catalog')
            self.assertEqual(updated['baseline_asset'], BASELINE)
            with self.assertRaises(RuntimeError):
                self.module.mesh_only_report({'validation': {}}, 'fbx', 'catalog')

        def test_mesh_only_mode_can_be_selected_by_script_argument_or_environment(self):
            self.assertTrue(self.module.mesh_only_requested(['--mesh-only'], {}))
            self.assertTrue(self.module.mesh_only_requested([], {'CITYCOURIER_PORTRAIT_MESH_ONLY': '1'}))
            self.assertFalse(self.module.mesh_only_requested([], {}))

        def run_generic_import_boundary(self, incomplete=False, scope='all', source_only=False, production=False):
            import os
            import runpy
            import tempfile
            from types import SimpleNamespace
            from unittest.mock import patch
            calls = []
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source = root/'AssetsSource/CityCourier'
                source.mkdir(parents=True)
                (source/'materials.json').write_text('[]')
                (source/'portrait_materials.json').write_text('{}')
                (source/'portrait_rider.json').write_text('{}')
                if production:
                    (source/'production_actions.json').write_text('{}')
                (source/'Exports/Portrait').mkdir(parents=True)
                (source/'Exports/Portrait/SK_Courier.fbx').write_bytes(b'fixture')
                if source_only:
                    (source/'portrait_baseline.json').write_text('{}')
                    if not incomplete:
                        (source/'portrait_import_validation.json').write_text('{}')
                else:
                    reports = root/'Saved/Validation/Portrait'
                    reports.mkdir(parents=True)
                    (reports/'import.original.json').write_text('{}')
                    if not incomplete:
                        (reports/'import.json').write_text('{}')
                installed = {BASELINE, MESH, SKELETON}
                assets = SimpleNamespace(does_asset_exist=lambda path: path in installed)
                def legacy_mutation():
                    raise RuntimeError('Legacy asset mutation reached')
                unreal_fixture = SimpleNamespace(Paths=SimpleNamespace(project_dir=lambda: str(root)),
                    EditorAssetSubsystem=object, get_editor_subsystem=lambda cls: assets,
                    AssetToolsHelpers=SimpleNamespace(get_asset_tools=lambda: object()),
                    MaterialExpressionConstant=object, AssetImportTask=legacy_mutation,
                    log=lambda message: calls.append(('log', message)))
                class PortraitFixture:
                    def __init__(self, unreal):
                        pass
                    def run(self, **options):
                        calls.append(('portrait', options))
                importer_fixture = SimpleNamespace(PortraitImporter=PortraitFixture)
                if hasattr(self.module, 'migrate_persistent_reports'):
                    importer_fixture.migrate_persistent_reports = self.module.migrate_persistent_reports
                execute_path = runpy.run_path
                def execute_production(path, **options):
                    self.assertEqual(Path(path), root/'Content/Python/import_production_actions.py')
                    if incomplete:
                        raise RuntimeError('Production assets mutated before installation preflight')
                    calls.append(('production_actions', {}))
                    return {}
                with patch.dict(sys.modules, {'unreal': unreal_fixture,
                                               'import_photoreal_rider': importer_fixture}), \
                     patch.dict(os.environ, {'CITYCOURIER_ART_IMPORT_SCOPE': scope}), \
                     patch.object(runpy, 'run_path', side_effect=execute_production):
                    execute_path(str(Path(__file__).with_name('import_art.py')), run_name='__main__')
            return calls

        def test_generic_import_routes_installed_portrait_before_any_legacy_mutation(self):
            for scope in ('all', 'characters'):
                with self.subTest(scope=scope):
                    calls = self.run_generic_import_boundary(scope=scope)
                    self.assertEqual([options for name, options in calls if name == 'portrait'], [{'mesh_only': True}])

        def test_generic_import_fails_closed_for_incomplete_portrait_installation(self):
            with self.assertRaisesRegex(RuntimeError, 'Incomplete portrait installation'):
                self.run_generic_import_boundary(incomplete=True)

        def test_promoted_actions_import_after_preflight_before_portrait_snapshot(self):
            for scope in ('all', 'characters'):
                calls = self.run_generic_import_boundary(source_only=True, production=True, scope=scope)
                writes = [name for name, options in calls if name != 'log']
                self.assertEqual(writes, ['production_actions', 'portrait'])
            with self.assertRaisesRegex(RuntimeError, 'Incomplete portrait installation'):
                self.run_generic_import_boundary(source_only=True, production=True, incomplete=True)

        def test_fresh_checkout_routes_with_only_source_reports_and_no_saved_directory(self):
            calls = self.run_generic_import_boundary(source_only=True)
            self.assertEqual([options for name, options in calls if name == 'portrait'], [{'mesh_only': True}])
            with self.assertRaisesRegex(RuntimeError, 'Incomplete portrait installation'):
                self.run_generic_import_boundary(source_only=True, incomplete=True)

        def test_legacy_report_migration_preserves_exact_original_bytes(self):
            import tempfile
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                saved = root/'Saved/Validation/Portrait'
                saved.mkdir(parents=True)
                original = b'{ "original": true }\n'
                latest = b'{ "validation": {"passed": true} }\n'
                (saved/'import.original.json').write_bytes(original)
                (saved/'import.json').write_bytes(latest)
                baseline, current = self.module.migrate_persistent_reports(root)
                self.assertEqual(baseline.read_bytes(), original)
                self.assertEqual(current.read_bytes(), latest)
                (saved/'import.original.json').write_bytes(b'changed legacy archive')
                self.module.migrate_persistent_reports(root)
                self.assertEqual(baseline.read_bytes(), original)

        def test_live_feature_changes_become_run_reference_without_rewriting_first_audit(self):
            original = {'protected_files': {'ABP': 'initial'}, 'mesh_rest': {'root': 'old'},
                        'skeleton_rest': {'root': 'old'}, 'skeleton_metadata': {'sockets': []}}
            live = {'protected_files': {'ABP': 'legitimate-new-action', 'NewClip': 'new'}, 'mesh_rest': {'root': 'current'},
                    'skeleton_rest': {'root': 'current'}, 'skeleton_metadata': {'sockets': ['new-socket']}}
            updated = self.module.apply_run_snapshot(original, live)
            self.assertEqual(updated['protected_files'], {'ABP': 'initial'})
            self.assertEqual(protection_record(updated, 'protected_files'), live['protected_files'])
            self.assertEqual(protection_record(updated, 'skeleton_metadata'), live['skeleton_metadata'])
            self.assertEqual(protection_record(original, 'protected_files'), original['protected_files'])

        def test_successful_full_import_archives_first_report_exactly_once(self):
            import tempfile
            with tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                first = {'protected_files': {'skeleton': 'first'}, 'validation': {'passed': True}}
                second = {'protected_files': {'skeleton': 'first'}, 'validation': {'passed': True}, 'source_fbx_sha256': 'new'}
                self.module.save_success_report(output, first)
                archived = (output/'import.original.json').read_bytes()
                self.assertEqual(json.loads(archived), first)
                self.module.save_success_report(output, second)
                self.assertEqual((output/'import.original.json').read_bytes(), archived)
                self.assertEqual(json.loads((output/'import.json').read_text()), second)

        def test_success_reports_also_write_persistent_source_records(self):
            import tempfile
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                saved, source = root/'Saved/Validation/Portrait', root/'AssetsSource/CityCourier'
                first = {'validation': {'passed': True}, 'protected_files': {'ABP': 'first'}}
                self.module.save_success_report(saved, first, source=source)
                baseline = source/'portrait_baseline.json'
                exact = baseline.read_bytes()
                second = {**first, 'source_fbx_sha256': 'next'}
                self.module.save_success_report(saved, second, source=source)
                self.assertEqual(baseline.read_bytes(), exact)
                self.assertEqual((source/'portrait_import_validation.json').read_bytes(), (saved/'import.json').read_bytes())

    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests))
    check(result.wasSuccessful(), 'Portrait self-tests failed')


if __name__ == '__main__':
    if '--self-test' in sys.argv:
        self_test()
    else:
        import unreal
        result = validate(unreal)
        unreal.log('PHOTOREAL COURIER VALIDATION PASSED ' + json.dumps(result, sort_keys=True))
