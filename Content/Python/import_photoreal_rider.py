"""Import the portrait courier, preserving the production skeleton and animations.

Run using Scripts/Run-EditorScript.ps1. No save-all call is used. A private
preflight import must match the existing mesh rig before production reimport.
The only existing package saved is Characters/SK_Courier.
"""
import copy
import json
import os
import re
from pathlib import Path
import sys

ART = '/Game/CityCourier/Art'
PORTRAIT = ART + '/Portrait'
CHARACTERS = ART + '/Characters'
BASELINE = PORTRAIT + '/Baseline/SK_CourierBefore'
KINDS = {'skin_head', 'skin_body', 'mouth', 'sclera', 'iris', 'pupil', 'cornea', 'hair', 'tearline'}
ORIGINAL_REPORT_KEYS = ('protected_files', 'mesh_rest', 'skeleton_rest', 'skeleton_metadata')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def mesh_only_requested(arguments, environment):
    return '--mesh-only' in arguments or environment.get('CITYCOURIER_PORTRAIT_MESH_ONLY') == '1'


def exclusive_report(path, contents):
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open('xb') as stream:
            stream.write(contents)
    except FileExistsError:
        pass


def migrate_persistent_reports(root):
    """Migrate ignored legacy logs once; tracked source reports are authoritative."""
    source, saved = root/'AssetsSource/CityCourier', root/'Saved/Validation/Portrait'
    baseline, current = source/'portrait_baseline.json', source/'portrait_import_validation.json'
    for target, legacy in [(baseline, saved/'import.original.json'), (current, saved/'import.json')]:
        if not target.is_file() and legacy.is_file():
            exclusive_report(target, legacy.read_bytes())
    return baseline, current


def archive_first_report(output, contents, source=None):
    """Create the first validated report archive without ever replacing it."""
    if source is not None:
        path = source/'portrait_baseline.json'
        exclusive_report(path, contents)
        contents = path.read_bytes()
    exclusive_report(output/'import.original.json', contents)


def save_success_report(output, report, source=None):
    require('validation' in report, 'Cannot archive an unvalidated portrait import')
    contents = (json.dumps(report, indent=2, sort_keys=True)+'\n').encode('utf-8')
    archive_first_report(output, contents, source=source)
    (output/'import.json').write_bytes(contents)
    if source is not None:
        (source/'portrait_import_validation.json').write_bytes(contents)


def apply_run_snapshot(report, live):
    require(all(key in live for key in ORIGINAL_REPORT_KEYS), 'Incomplete live import protection snapshot')
    result = copy.deepcopy(report)
    for key in ORIGINAL_REPORT_KEYS:
        result['run_'+key] = copy.deepcopy(live[key])
    return result


def mesh_only_report(previous, fbx_hash, catalog_hash):
    require(all(key in previous for key in ORIGINAL_REPORT_KEYS + ('textures', 'source_materials_sha256', 'validation')),
            'Mesh-only import requires an existing successful portrait import report')
    require(previous.get('baseline_asset', BASELINE) == BASELINE, 'Original baseline asset reference changed')
    result = copy.deepcopy(previous)
    result['baseline_asset'] = BASELINE
    result['source_fbx_sha256'] = fbx_hash
    result['source_catalog_sha256'] = catalog_hash
    result['mode'] = 'mesh-only'
    result.pop('validation', None)
    return result


def normal_flip(definition):
    space = definition.get('normal_space', 'OpenGL')
    require(space in ('OpenGL', 'DirectX'), 'Unknown normal map space: ' + str(space))
    return space == 'OpenGL'


def single_channel_mask(channel):
    require(channel in 'RGBA' and len(channel) == 1, 'Expected one RGBA mask channel')
    # ComponentMask defaults to RG. Every flag must be explicit, including
    # disabled channels, or scalar blend masks become colored vectors.
    return {component.lower(): component == channel for component in 'RGBA'}


def eye_coat_optics(definition, default_ior=1.376):
    ior = float(definition.get('ior', default_ior))
    require(1.0 < ior <= 2.0, 'Invalid physical eye coat IOR')
    f0 = ((ior-1.0)/(ior+1.0))**2
    # Thin cornea/tear meshes already enclose the physical iris surface.
    # UE screen-space refraction has no matching thickness and copies the
    # scene into the eye. Keep physical Fresnel through Specular, no offset.
    return f0/.08, 1.0


def source_texture(source, relative):
    path = (source/relative).resolve()
    require(path.is_relative_to(source.resolve()), 'Texture escapes AssetsSource/CityCourier: ' + str(relative))
    require(path.is_file(), 'Missing portrait texture: ' + str(path))
    return path


def validate_definitions(metadata):
    definitions = metadata.get('materials', {})
    require(definitions, 'Empty portrait material contract')
    for name, definition in definitions.items():
        require(re.fullmatch(r'M_Portrait[A-Za-z0-9_]+', name), 'Invalid portrait material name: ' + name)
        kind = definition.get('kind')
        require(kind in KINDS, 'Unknown portrait surface kind: ' + str(kind))
        textures = definition.get('textures', {})
        require(set(textures) <= {'base_color', 'normal', 'roughness', 'neck_base_color'},
                'Unknown map key on ' + name)
        if kind in ('skin_head', 'mouth'):
            require({'base_color', 'normal', 'roughness'} <= set(textures), name + ' needs albedo, normal and roughness maps')
        if kind == 'skin_body':
            require({'base_color', 'normal'} <= set(textures), name + ' needs body albedo and fine normal maps')
        if kind == 'skin_head':
            require('neck_base_color' in textures, 'Head needs UV1 body albedo for its neck transition')
        if kind in ('sclera', 'iris'):
            require('base_color' in textures, name + ' needs its source eye texture')
        normal_flip(definition)
        require(.1 <= float(definition.get('normal_uv_scale', 1)) <= 32, 'Invalid normal tiling on ' + name)
    return definitions


class PortraitImporter:
    def __init__(self, unreal):
        self.u = unreal
        self.root = Path(unreal.Paths.project_dir()).resolve()
        self.source = self.root/'AssetsSource/CityCourier'
        self.assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        self.tools = unreal.AssetToolsHelpers.get_asset_tools()
        self.editor = unreal.MaterialEditingLibrary
        self.texture_records = {}
        self.material_names = set()
        self.mesh_only = False

    def save(self, asset):
        path = asset.get_path_name().split('.')[0]
        require(not self.mesh_only or path == CHARACTERS+'/SK_Courier',
                'Mesh-only import cannot save a surface or baseline asset: '+path)
        require(path.startswith(PORTRAIT+'/') or path == CHARACTERS+'/SK_Courier' or
                path in {ART+'/Materials/'+name for name in self.material_names},
                'Refusing to save protected package ' + path)
        require(self.assets.save_loaded_asset(asset, only_if_is_dirty=False), 'Could not save ' + path)

    def create(self, name, folder, cls, factory):
        path = folder+'/'+name
        asset = self.u.load_asset(path) if self.assets.does_asset_exist(path) else self.tools.create_asset(name, folder, cls, factory)
        require(isinstance(asset, cls), 'Wrong/missing asset class at ' + path)
        return asset

    def node(self, mat, cls, **properties):
        result = self.editor.create_material_expression(mat, cls)
        for key, value in properties.items():
            result.set_editor_property(key, value)
        return result

    def scalar(self, mat, value):
        return self.node(mat, self.u.MaterialExpressionConstant, r=float(value))

    def color(self, mat, rgb):
        return self.node(mat, self.u.MaterialExpressionConstant3Vector, constant=self.u.LinearColor(*rgb, 1))

    def link(self, a, b, pin='', channel=''):
        require(self.editor.connect_material_expressions(a, channel, b, pin), 'Failed graph connection: '+b.get_name()+'.'+pin)

    def output(self, node, prop, channel=''):
        require(self.editor.connect_material_property(node, channel, prop), 'Failed material output '+str(prop))

    def multiply(self, mat, a, b):
        n = self.node(mat, self.u.MaterialExpressionMultiply)
        self.link(a, n, 'A'); self.link(b, n, 'B')
        return n

    def mix(self, mat, a, b, alpha):
        n = self.node(mat, self.u.MaterialExpressionLinearInterpolate)
        self.link(a, n, 'A'); self.link(b, n, 'B'); self.link(alpha, n, 'Alpha')
        return n

    def channel(self, mat, source, channel):
        n = self.node(mat, self.u.MaterialExpressionComponentMask, **single_channel_mask(channel))
        self.link(source, n)
        return n

    def profile(self):
        u = self.u
        profile = self.create('SSP_CourierSkin', PORTRAIT, u.SubsurfaceProfile, u.SubsurfaceProfileFactory())
        settings = profile.get_editor_property('settings')
        # Engine SubsurfaceProfile.h defines cm units. 1.2 mm red mean free
        # path keeps pores and nasolabial detail while softening ears/nose.
        values = {'enable_burley': True, 'enable_mean_free_path': True,
                  'surface_albedo': u.LinearColor(.48, .235, .16, 1),
                  'mean_free_path_color': u.LinearColor(1, .18, .095, 1),
                  'mean_free_path_distance': .12, 'world_unit_scale': 1.0,
                  'tint': u.LinearColor(1, 1, 1, 1),
                  'boundary_color_bleed': u.LinearColor(.8, .32, .22, 1),
                  'roughness0': .70, 'roughness1': 1.30, 'lobe_mix': .78,
                  'ior': 1.4, 'normal_scale': .08,
                  'extinction_scale': 1.0, 'scattering_distribution': .8,
                  'transmission_tint_color': u.LinearColor(1, .72, .58, 1)}
        for key, value in values.items():
            settings.set_editor_property(key, value)
        profile.set_editor_property('settings', settings)
        self.save(profile)
        return profile

    def texture(self, mat, name, key, relative, definition):
        u = self.u
        path = source_texture(self.source, relative)
        asset_name = 'T_'+name.removeprefix('M_')+'_'+key
        task = u.AssetImportTask()
        task.filename = str(path)
        task.destination_path = PORTRAIT+'/Textures'
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = False
        self.tools.import_asset_tasks([task])
        texture = u.load_asset(task.destination_path+'/'+asset_name)
        require(isinstance(texture, u.Texture2D), 'Texture import failed: '+str(path))
        normal = key == 'normal'
        linear = normal or key == 'roughness'
        texture.set_editor_property('srgb', not linear)
        texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP if normal else
                                    u.TextureCompressionSettings.TC_GRAYSCALE if key == 'roughness' else u.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property('flip_green_channel', normal and normal_flip(definition))
        self.save(texture)
        from validate_photoreal_rider import file_hash
        self.texture_records.setdefault(name, {})[key] = {'asset': texture.get_path_name().split('.')[0],
            'source': relative, 'sha256': file_hash(path), 'flip_green': normal and normal_flip(definition)}
        sample = self.node(mat, u.MaterialExpressionTextureSample, texture=texture,
            sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
            u.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE if key == 'roughness' else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        scale = float(definition.get('normal_uv_scale', 1)) if normal else 1
        uv = self.node(mat, u.MaterialExpressionTextureCoordinate,
                       coordinate_index=1 if key == 'neck_base_color' else 0,
                       u_tiling=scale, v_tiling=scale)
        self.link(uv, sample, 'UVs')
        return sample

    def material(self, name, definition, profile):
        u, E, P = self.u, self.editor, self.u.MaterialProperty
        kind = definition['kind']
        mat = self.create(name, ART+'/Materials', u.Material, u.MaterialFactoryNew())
        E.delete_all_material_expressions(mat)
        E.set_material_usage(mat, u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        mat.set_editor_property('blend_mode', u.BlendMode.BLEND_OPAQUE)
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
        mat.set_editor_property('two_sided', False)
        mat.set_editor_property('tangent_space_normal', True)
        samples = {key: self.texture(mat, name, key, relative, definition)
                   for key, relative in definition.get('textures', {}).items()}
        defaults = {'pupil': (.0015, .001, .0007), 'hair': (.006, .004, .002),
                    'cornea': (.99, .99, .99), 'tearline': (.35, .18, .14)}
        base = samples.get('base_color') or self.color(mat, definition.get('color', defaults.get(kind, (.4, .22, .16))))
        rough_defaults = {'skin_head': .46, 'skin_body': .50, 'mouth': .38, 'sclera': .28,
                          'iris': .36, 'pupil': .25, 'hair': .46, 'cornea': .018, 'tearline': .04}
        rough = self.channel(mat, samples['roughness'], 'R') if 'roughness' in samples else self.scalar(mat, definition.get('roughness', rough_defaults[kind]))
        # Clamp measured maps into a plausible range without flattening detail.
        if kind.startswith('skin_'):
            clamp = self.node(mat, u.MaterialExpressionClamp, min_default=.30, max_default=.65)
            self.link(rough, clamp); rough = clamp
            mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
            mat.set_editor_property('subsurface_profile', profile)
            self.output(self.scalar(mat, .82), P.MP_OPACITY)
            self.output(self.scalar(mat, .35), P.MP_SPECULAR)
            if kind == 'skin_head':
                vc = self.node(mat, u.MaterialExpressionVertexColor)
                neck = self.channel(mat, vc, 'R')
                base = self.mix(mat, base, samples['neck_base_color'], neck)
                scalp = self.multiply(mat, self.channel(mat, vc, 'B'), self.scalar(mat, .96))
                base = self.mix(mat, base, self.color(mat, (.004, .003, .002)), scalp)
        else:
            # Specular=F0/.08; IOR 1.376 cornea / 1.336 tear film have
            # approximately 2.5% / 2.1% normal-incidence reflectance.
            coat_specular = eye_coat_optics(definition, 1.336 if kind == 'tearline' else 1.376)[0] if kind in ('cornea', 'tearline') else .32
            self.output(self.scalar(mat, coat_specular), P.MP_SPECULAR)
        if kind == 'hair':
            mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_HAIR)
            mat.set_editor_property('two_sided', True)
            uv = self.node(mat, u.MaterialExpressionTextureCoordinate)
            tip = self.channel(mat, uv, 'G')
            base = self.mix(mat, base, self.multiply(mat, base, self.scalar(mat, 1.65)), tip)
            # Hair uses Normal as strand tangent (ShadingModelsMaterial.ush).
            # UV V is root->tip; tangent-space Y follows skeletal deformation.
            self.output(self.color(mat, (0, 1, 0)), P.MP_NORMAL)
            # UE5.6 hides the CustomData0 enum from Python. Keep the
            # engine's standard hair backlighting default.
        if kind == 'sclera' and definition.get('alpha_mask', False):
            mat.set_editor_property('blend_mode', u.BlendMode.BLEND_MASKED)
            mat.set_editor_property('opacity_mask_clip_value', .333)
            self.output(samples['base_color'], P.MP_OPACITY_MASK, 'A')
        if kind in ('cornea', 'tearline'):
            mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
            mat.set_editor_property('translucency_lighting_mode', u.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
            mat.set_editor_property('refraction_method', u.RefractionMode.RM_INDEX_OF_REFRACTION)
            self.output(self.scalar(mat, definition.get('opacity', .035 if kind == 'cornea' else .22)), P.MP_OPACITY)
            self.output(self.scalar(mat, 1.0), P.MP_REFRACTION)
        if 'normal' in samples:
            self.output(samples['normal'], P.MP_NORMAL, 'RGB')
        self.output(base, P.MP_BASE_COLOR, 'RGB' if base in samples.values() else '')
        self.output(rough, P.MP_ROUGHNESS)
        self.output(self.scalar(mat, 0), P.MP_METALLIC)
        E.layout_material_expressions(mat)
        E.recompile_material(mat)
        self.save(mat)
        return mat

    def import_mesh(self, folder, name, skeleton=None):
        u = self.u
        options = u.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_as_skeletal = True
        options.import_mesh = True
        options.import_animations = False
        options.import_materials = False
        options.import_textures = False
        options.create_physics_asset = False
        options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
        if skeleton:
            options.skeleton = skeleton
        data = options.skeletal_mesh_import_data
        for key, value in {'update_skeleton_reference_pose': False, 'use_t0_as_ref_pose': False,
                           'import_morph_targets': False, 'preserve_smoothing_groups': True,
                           'reorder_material_to_fbx_order': True,
                           'vertex_color_import_option': u.VertexColorImportOption.REPLACE,
                           'normal_import_method': u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS}.items():
            data.set_editor_property(key, value)
        task = u.AssetImportTask()
        task.filename = str(self.source/'Exports/Portrait/SK_Courier.fbx')
        task.destination_path, task.destination_name = folder, name
        task.automated, task.replace_existing, task.save = True, True, False
        task.options, task.factory = options, u.FbxFactory()
        self.tools.import_asset_tasks([task])
        require(all(path.startswith(folder+'/') for path in task.imported_object_paths),
                'FBX importer created assets outside its destination: '+str(task.imported_object_paths))
        mesh = u.load_asset(folder+'/'+name)
        require(isinstance(mesh, u.SkeletalMesh), 'FBX did not import a skeletal mesh: '+name)
        return mesh

    def assign_materials(self, mesh, materials):
        slots = list(mesh.get_editor_property('materials'))
        seen = set()
        for slot in slots:
            name = str(slot.get_editor_property('imported_material_slot_name'))
            if name not in materials and not self.assets.does_asset_exist(ART+'/Materials/'+name):
                name = str(slot.get_editor_property('material_slot_name'))
            material = materials.get(name) or self.u.load_asset(ART+'/Materials/'+name)
            require(material, 'Unknown FBX material slot '+name)
            slot.set_editor_property('material_slot_name', name)
            slot.set_editor_property('material_interface', material)
            seen.add(name)
        require(set(materials) <= seen, 'FBX missing portrait materials '+str(set(materials)-seen))
        mesh.set_editor_property('materials', slots)

    def cleanup_failed_materials(self):
        """Delete only this pass's seven abandoned material prototypes, if unused."""
        result = {}
        for suffix in ('SkinBody', 'SkinHead', 'Mouth', 'Iris', 'Sclera', 'Pupil', 'Cornea'):
            path = PORTRAIT+'/Materials/M_Portrait'+suffix
            if not self.assets.does_asset_exist(path):
                continue
            users = list(self.assets.find_package_referencers_for_asset(path, load_assets_to_confirm=True))
            if users:
                result[path] = {'retained_referencers': [str(item) for item in users]}
                continue
            require(self.assets.delete_asset(path), 'Cannot remove unreferenced failed-run material '+path)
            result[path] = {'deleted': True}
        return result

    def run_mesh_only(self, definitions, skeleton, original, live):
        from validate_photoreal_rider import (compare_rest, file_hash, mesh_rest, protected_files,
                                              skeleton_rest, validate)
        u = self.u
        self.mesh_only = True
        output = self.root/'Saved/Validation/Portrait'
        initial_path, report_path = migrate_persistent_reports(self.root)
        require(report_path.is_file(), 'Mesh-only import needs the first successful import.json')
        require(initial_path.is_file(), 'Mesh-only import needs the persistent original portrait_baseline.json')
        initial = json.loads(initial_path.read_text(encoding='utf-8'))
        previous = json.loads(report_path.read_text(encoding='utf-8'))
        for key in ORIGINAL_REPORT_KEYS:
            require(previous.get(key) == initial.get(key), 'Original protection record changed: '+key)
        require(self.assets.does_asset_exist(BASELINE), 'Original courier baseline is missing')
        report = mesh_only_report(previous, file_hash(self.source/'Exports/Portrait/SK_Courier.fbx'),
                                  file_hash(self.source/'portrait_rider.json'))
        report = apply_run_snapshot(report, live)
        # Validate the current protected state and reused shaders before any FBX
        # import. Only source revision hashes are updated; all original checks
        # and texture fingerprints remain those of the first validated import.
        validate(u, report)
        self.skin_profile = u.load_asset(PORTRAIT+'/SSP_CourierSkin')
        require(isinstance(self.skin_profile, u.SubsurfaceProfile), 'Existing skin profile is missing')
        materials = {name: u.load_asset(ART+'/Materials/'+name) for name in definitions}
        require(all(isinstance(material, u.Material) for material in materials.values()),
                'Mesh-only import requires all existing portrait materials')
        surface_paths = {PORTRAIT+'/SSP_CourierSkin'} | {ART+'/Materials/'+name for name in definitions}
        surface_paths |= {record['asset'] for maps in report['textures'].values() for record in maps.values()}
        surface_files = {str(Path('Content')/(path.removeprefix('/Game/')+'.uasset')):
                         file_hash(self.root/'Content'/(path.removeprefix('/Game/')+'.uasset'))
                         for path in sorted(surface_paths)}
        u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
        preflight = self.import_mesh(PORTRAIT+'/Preflight', 'SK_CourierPreflight')
        compare_rest(live['mesh_rest'], mesh_rest(u, preflight), 'Mesh-only FBX preflight rig')
        compare_rest(live['skeleton_rest'], skeleton_rest(u, skeleton), 'Shared skeleton after mesh-only preflight')
        self.assign_materials(preflight, materials)
        require(protected_files(self.root) == live['protected_files'], 'Preflight changed a protected package')
        mesh = self.import_mesh(CHARACTERS, 'SK_Courier', skeleton)
        require(mesh.get_editor_property('skeleton') == skeleton, 'Mesh-only reimport changed the shared skeleton binding')
        compare_rest(live['mesh_rest'], mesh_rest(u, mesh), 'Mesh-only production rig')
        compare_rest(live['skeleton_rest'], skeleton_rest(u, skeleton), 'Shared skeleton after mesh-only reimport')
        self.assign_materials(mesh, materials)
        self.save(mesh)
        output.mkdir(parents=True, exist_ok=True)
        (output/'import.pending.json').write_text(json.dumps(report, indent=2, sort_keys=True)+'\n', encoding='utf-8')
        result = validate(u, report)
        for relative, expected in surface_files.items():
            require(file_hash(self.root/relative) == expected, 'Mesh-only reimport changed a reused surface asset: '+relative)
        report['reused_surface_files'] = surface_files
        report['cleanup'] = self.cleanup_failed_materials()
        report['run_protected_files_after'] = protected_files(self.root)
        require(report['run_protected_files_after'] == live['protected_files'], 'Cleanup changed a protected package')
        report['validation'] = result
        save_success_report(output, report, source=self.source)
        u.log('PHOTOREAL COURIER MESH-ONLY IMPORT COMPLETE '+json.dumps(result, sort_keys=True))

    def run(self, mesh_only=False):
        from validate_photoreal_rider import (BASELINE, SKELETON, compare_rest, file_hash, mesh_rest,
                                              protected_files, skeleton_metadata, skeleton_rest, validate)
        u = self.u
        definitions = validate_definitions(json.loads((self.source/'portrait_materials.json').read_text(encoding='utf-8')))
        self.material_names = set(definitions)
        require((self.source/'Exports/Portrait/SK_Courier.fbx').is_file(), 'Portrait FBX is missing')
        for definition in definitions.values():
            for relative in definition.get('textures', {}).values():
                source_texture(self.source, relative)
        skeleton = u.load_asset(SKELETON)
        original = u.load_asset(CHARACTERS+'/SK_Courier')
        require(skeleton and original and original.get_editor_property('skeleton') == skeleton,
                'Original courier mesh and shared skeleton must exist')
        live = {'protected_files': protected_files(self.root), 'skeleton_rest': skeleton_rest(u, skeleton),
                'skeleton_metadata': skeleton_metadata(skeleton, original), 'mesh_rest': mesh_rest(u, original)}
        if mesh_only:
            return self.run_mesh_only(definitions, skeleton, original, live)
        report = {**copy.deepcopy(live),
                  'source_fbx_sha256': file_hash(self.source/'Exports/Portrait/SK_Courier.fbx'),
                  'source_materials_sha256': file_hash(self.source/'portrait_materials.json'),
                  'baseline_asset': BASELINE}
        output = self.root/'Saved/Validation/Portrait'
        initial_path, previous_path = migrate_persistent_reports(self.root)
        require(initial_path.is_file() or not previous_path.is_file(),
                'Existing portrait import is missing its persistent original baseline report')
        if initial_path.is_file():
            initial = json.loads(initial_path.read_text(encoding='utf-8'))
            for key in ORIGINAL_REPORT_KEYS:
                report[key] = copy.deepcopy(initial[key])
        report = apply_run_snapshot(report, live)
        catalog = self.source/'portrait_rider.json'
        if catalog.is_file():
            report['source_catalog_sha256'] = file_hash(catalog)
        # Duplicate exactly once before any production replacement, leaving the
        # original M_Skin and all clothes references untouched for comparison.
        if not self.assets.does_asset_exist(BASELINE):
            baseline = self.assets.duplicate_asset(CHARACTERS+'/SK_Courier', BASELINE)
            require(baseline, 'Could not create original courier baseline')
            self.save(baseline)
        u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
        preflight = self.import_mesh(PORTRAIT+'/Preflight', 'SK_CourierPreflight')
        compare_rest(live['mesh_rest'], mesh_rest(u, preflight), 'FBX preflight rig')
        compare_rest(live['skeleton_rest'], skeleton_rest(u, skeleton), 'Shared skeleton after preflight')
        self.skin_profile = self.profile()
        materials = {name: self.material(name, definition, self.skin_profile) for name, definition in definitions.items()}
        self.assign_materials(preflight, materials)
        # Only after a full rig/material preflight may the production mesh be
        # reimported. save=False keeps the importer from saving shared packages.
        mesh = self.import_mesh(CHARACTERS, 'SK_Courier', skeleton)
        require(mesh.get_editor_property('skeleton') == skeleton, 'Production mesh lost shared skeleton')
        compare_rest(live['mesh_rest'], mesh_rest(u, mesh), 'Production mesh rig')
        compare_rest(live['skeleton_rest'], skeleton_rest(u, skeleton), 'Shared skeleton after reimport')
        self.assign_materials(mesh, materials)
        self.save(mesh)
        report['textures'] = self.texture_records
        output = self.root/'Saved/Validation/Portrait'
        output.mkdir(parents=True, exist_ok=True)
        (output/'import.pending.json').write_text(json.dumps(report, indent=2, sort_keys=True)+'\n', encoding='utf-8')
        result = validate(u, report)
        report['run_protected_files_after'] = protected_files(self.root)
        report['validation'] = result
        save_success_report(output, report, source=self.source)
        u.log('PHOTOREAL COURIER IMPORT COMPLETE '+json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    import unreal
    sys.path.insert(0, str(Path(unreal.Paths.project_dir())/'Content/Python'))
    PortraitImporter(unreal).run(mesh_only=mesh_only_requested(sys.argv[1:], os.environ))
