"""Import the independent interior finish kit without touching retained assets.

Reads the Blender manifest and palette, writes only Art/Interiors packages and
two explicit validation reports. UE 5.6 commandlets require
``-AllowCommandletRendering`` to validate compiled shader statistics; an editor
Python run also works. No current level is modified or saved.

Fresh palette (PowerShell, from the project root; use your installed UE path)::

    $env:CITYCOURIER_INTERIOR_IMPORT_SCOPE='all'
    $interiorProject=(Resolve-Path ./CityCourier.uproject).Path
    $interiorImporter=(Resolve-Path ./Content/Python/import_interior_finish.py).Path
    & D:/sdk/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe $interiorProject -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput -AllowCommandletRendering -run=pythonscript "-script=$interiorImporter"

Verified existing palette, meshes only (no renderer needed)::

    $env:CITYCOURIER_INTERIOR_IMPORT_SCOPE='meshes'
    ./scripts/Run-EditorScript.ps1 -Script Content/Python/import_interior_finish.py -EngineRoot D:/sdk/UE_5.6
"""
import hashlib
import json
import math
import os
import re
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'AssetsSource/CityCourier'
SOURCE_KIT = SOURCE / 'InteriorArt'
DEST = '/Game/CityCourier/Art/Interiors'
E = unreal.MaterialEditingLibrary


def _hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def protected_snapshot():
    """Include all retained art, both city maps, building data and runtime code."""
    paths = set()
    for folder in ['Content/CityCourier/Art', 'Content/CityCourier/Maps',
                   'Content/CityCourier/Data', 'Content/CityCourier/Blueprints', 'Source']:
        for path in (ROOT / folder).rglob('*'):
            if path.is_file() and not path.is_relative_to(ROOT/'Content/CityCourier/Art/Interiors'):
                paths.add(path)
    for name in ['tools/interiors/layout.py',
                 'AssetsSource/CityCourier/Interiors/inventory.json',
                 'AssetsSource/CityCourier/Interiors/asset-manifest.json',
                 'AssetsSource/CityCourier/materials.json']:
        path = ROOT / name
        if path.is_file():
            paths.add(path)
    return {path.relative_to(ROOT).as_posix(): _hash(path) for path in sorted(paths)}


def _node(material, cls, **properties):
    result = E.create_material_expression(material, cls)
    if not result:
        raise RuntimeError('Could not create ' + cls.__name__)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def _scalar(material, value):
    return _node(material, unreal.MaterialExpressionConstant, r=float(value))


def _color(material, value):
    return _node(material, unreal.MaterialExpressionConstant3Vector,
                 constant=unreal.LinearColor(*value, 1))


def _link(source, target, pin='', output=''):
    if not E.connect_material_expressions(source, output, target, pin):
        raise RuntimeError('Material graph connection failed: '+target.get_name()+'.'+pin)


def _output(source, prop):
    if not E.connect_material_property(source, '', prop):
        raise RuntimeError('Material output connection failed: ' + str(prop))


def _unary(material, cls, value, **properties):
    result = _node(material, cls, **properties)
    _link(value, result)
    return result


def _binary(material, cls, a, b):
    result = _node(material, cls)
    _link(a, result, 'A')
    _link(b, result, 'B')
    return result


def _mul(material, a, b):
    return _binary(material, unreal.MaterialExpressionMultiply, a, b)


def _add(material, a, b):
    return _binary(material, unreal.MaterialExpressionAdd, a, b)


def _sub(material, a, b):
    return _binary(material, unreal.MaterialExpressionSubtract, a, b)


def _mix(material, a, b, alpha):
    result = _node(material, unreal.MaterialExpressionLinearInterpolate)
    for source, pin in [(a, 'A'), (b, 'B'), (alpha, 'Alpha')]:
        _link(source, result, pin)
    return result


def _mask(material, source, channel):
    # UE defaults R/G on some mask nodes. Set ALL four channels explicitly.
    result = _node(material, unreal.MaterialExpressionComponentMask,
                   r=False, g=False, b=False, a=False)
    result.set_editor_property(channel, True)
    _link(source, result)
    return result


def _append(material, a, b):
    return _binary(material, unreal.MaterialExpressionAppendVector, a, b)


def _world_uv(material, centimeters):
    position = _node(material, unreal.MaterialExpressionWorldPosition)
    normal = _node(material, unreal.MaterialExpressionVertexNormalWS)
    absolute = _unary(material, unreal.MaterialExpressionAbs, normal)
    x, y, z = [_mask(material, position, c) for c in ['r', 'g', 'b']]
    nx, nz = [_mask(material, absolute, c) for c in ['r', 'b']]
    # X walls -> YZ, Y walls -> XZ, horizontal surfaces -> XY.
    u = _mix(material, x, y, nx)
    v = _mix(material, z, y, nz)
    return _mul(material, _append(material, u, v), _scalar(material, 1/centimeters)), position


def _noise(material, position, scale):
    coordinates = _mul(material, position, _scalar(material, scale))
    noise = _node(material, unreal.MaterialExpressionNoise, scale=1.0,
                  quality=1, levels=2, output_min=0.0, output_max=1.0)
    # Noise exposes a display name with its position origin; the unnamed first
    # pin is stable across UE's origin-space labels (as in polish_materials.py).
    _link(coordinates, noise)
    return _unary(material, unreal.MaterialExpressionClamp, noise)


class InteriorImporter:
    def __init__(self):
        self.assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        self.tools = unreal.AssetToolsHelpers.get_asset_tools()
        self.materials = {}
        self.textures = {}
        self.created = []
        self.report = {'schema_version': 1, 'asset_root': DEST,
                       'started_utc': datetime.now(timezone.utc).isoformat(),
                       'assets': [], 'materials': [], 'textures': []}

    def _save(self, asset):
        if not asset.get_path_name().startswith(DEST+'/'):
            raise RuntimeError('Refusing save outside the interior kit: '+asset.get_path_name())
        if not self.assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('Asset did not save: '+asset.get_path_name())
        if asset.get_path_name() not in self.created:
            self.created.append(asset.get_path_name())

    def _import_file(self, file, folder, name, options=None):
        if not folder.startswith(DEST+'/') and folder != DEST:
            raise RuntimeError('Import destination outside the interior kit')
        task = unreal.AssetImportTask()
        task.filename = str(file)
        task.destination_path = folder
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = False
        if options:
            task.options = options
        self.tools.import_asset_tasks([task])
        imported_paths = [str(path) for path in task.imported_object_paths]
        expected_path = folder+'/'+name+'.'+name
        # Fail before load_asset can silently return a previous saved package.
        # Every kit FBX and texture must freshly produce exactly one target.
        if imported_paths != [expected_path]:
            raise RuntimeError('Expected exactly the newly imported target '+expected_path+
                               '; actual import results='+repr(imported_paths))
        result = unreal.load_asset(folder+'/'+name)
        if not result:
            raise RuntimeError('Asset import produced no result: '+name)
        return result

    def _texture(self, scan, channel):
        key = (scan, channel)
        if key in self.textures:
            return self.textures[key]
        file = SOURCE/'External/PolyHaven'/f'{scan}_{channel}_2k.jpg'
        texture = self._import_file(file, DEST+'/Textures', 'T_IF_'+file.stem)
        normal = channel == 'nor_gl'
        texture.set_editor_property('srgb', channel == 'diff')
        texture.set_editor_property('compression_settings',
            unreal.TextureCompressionSettings.TC_NORMALMAP if normal else
            unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property('flip_green_channel', normal)
        self._save(texture)
        self.textures[key] = texture
        self.report['textures'].append({'asset': texture.get_path_name(),
            'source': file.relative_to(SOURCE).as_posix(), 'source_sha256': _hash(file),
            'srgb': bool(texture.get_editor_property('srgb')),
            'flip_green_channel': bool(texture.get_editor_property('flip_green_channel'))})
        return texture

    def _sample(self, material, scan, channel, uv):
        sampler = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel == 'nor_gl' else
                   unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if channel == 'rough' else
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        sample = _node(material, unreal.MaterialExpressionTextureSample,
                       texture=self._texture(scan, channel), sampler_type=sampler)
        _link(uv, sample, 'UVs')
        return sample

    def _material(self, definition):
        name = definition['name']
        path = DEST+'/Materials/'+name
        material = (unreal.load_asset(path) if self.assets.does_asset_exist(path) else
                    self.tools.create_asset(name, DEST+'/Materials', unreal.Material,
                                            unreal.MaterialFactoryNew()))
        if not isinstance(material, unreal.Material):
            raise RuntimeError('Expected an editable material at '+path)
        E.delete_all_material_expressions(material)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
        material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        E.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        E.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_NANITE)
        uv, position = _world_uv(material, float(definition.get('tile_cm', 200)))
        base = _color(material, definition['color'])
        rough = _scalar(material, definition['roughness'])
        micro = _noise(material, position, .8)
        if definition.get('scan'):
            scan = definition['scan']
            base = _mix(material, base, self._sample(material, scan, 'diff', uv), _scalar(material, .35))
            rough = _mix(material, rough, self._sample(material, scan, 'rough', uv), _scalar(material, .35))
            normal = self._sample(material, scan, 'nor_gl', uv)
            # Keep reused exterior scans subtle on maintained indoor plaster.
            normal = _unary(material, unreal.MaterialExpressionNormalize,
                            _mul(material, normal, _color(material, (.25,.25,1))))
            _output(normal, unreal.MaterialProperty.MP_NORMAL)
        kind = definition['kind']
        if kind == 'tile':
            frac = _unary(material, unreal.MaterialExpressionFrac, uv)
            one = _scalar(material, 1)
            u, v = [_mask(material, frac, c) for c in ['r', 'g']]
            du = _binary(material, unreal.MaterialExpressionMin, u, _sub(material, one, u))
            dv = _binary(material, unreal.MaterialExpressionMin, v, _sub(material, one, v))
            edge = _binary(material, unreal.MaterialExpressionMin, du, dv)
            # 0.003 UV on either side is a 3mm grout line at the 50cm tile scale.
            grout = _sub(material, one, _unary(material, unreal.MaterialExpressionClamp,
                         _mul(material, edge, _scalar(material, 1/.003))))
            cell = _unary(material, unreal.MaterialExpressionFloor, uv)
            cx, cy = [_mask(material, cell, c) for c in ['r', 'g']]
            hashed = _add(material, _mul(material, cx, _scalar(material, 12.9898)),
                          _mul(material, cy, _scalar(material, 78.233)))
            hashed = _unary(material, unreal.MaterialExpressionFrac,
                      _mul(material, _unary(material, unreal.MaterialExpressionSine, hashed),
                           _scalar(material, 43758.5453)))
            base = _mul(material, base, _mix(material, _scalar(material, .965),
                                            _scalar(material, 1.025), hashed))
            base = _mul(material, base, _mix(material, _scalar(material, .985), one, micro))
            base = _mix(material, base, _color(material, (.24,.245,.225)), grout)
            rough = _mix(material, rough, _scalar(material, .85), grout)
            if definition['key'] == 'tile_grey':
                # Existing slabs group top and soffit faces. Keep their lower
                # faces painted rather than repeating the floor finish overhead.
                up = _unary(material, unreal.MaterialExpressionClamp,
                            _mask(material, _node(material, unreal.MaterialExpressionVertexNormalWS), 'b'))
                base = _mix(material, _color(material, (.66,.67,.63)), base, up)
                rough = _mix(material, _scalar(material, .8), rough, up)
        elif kind == 'wood':
            u = _mask(material, uv, 'r')
            variation = _noise(material, position, .10)
            grain = _unary(material, unreal.MaterialExpressionSine,
                    _add(material, _mul(material, u, _scalar(material, 65)),
                         _mul(material, variation, _scalar(material, 1.3))))
            grain = _add(material, _mul(material, grain, _scalar(material, .5)), _scalar(material, .5))
            base = _mul(material, base, _mix(material, _scalar(material, .91), _scalar(material, 1.04), grain))
            rough = _mix(material, rough, _scalar(material, min(.98,definition['roughness']+.045)), variation)
        elif kind == 'fabric':
            threads = []
            for channel in ['r', 'g']:
                coordinate = _mul(material, _mask(material, uv, channel),
                                  _scalar(material, definition.get('tile_cm',200)*8))
                threads.append(_unary(material, unreal.MaterialExpressionAbs,
                                      _unary(material, unreal.MaterialExpressionSine, coordinate)))
            weave = _mul(material, threads[0], threads[1])
            base = _mul(material, base, _mix(material, _scalar(material, .94), _scalar(material, 1.02), weave))
            rough = _mix(material, rough, _scalar(material, .94), _mul(material, micro, _scalar(material, .2)))
            _output(_scalar(material, .17), unreal.MaterialProperty.MP_SPECULAR)
        elif kind == 'metal':
            rough = _mix(material, rough, _scalar(material, min(.95,definition['roughness']+.045)), micro)
        elif kind == 'glass':
            # Frosted, pale opaque glass remains supported by Nanite and avoids
            # making monitor faces transparent when sharing this kit material.
            base = _mix(material, base, _color(material, (.76,.79,.75)), _scalar(material, .35))
            rough = _scalar(material, max(.23, definition['roughness']))
            _output(_scalar(material, .5), unreal.MaterialProperty.MP_SPECULAR)
        else:
            base = _mul(material, base, _mix(material, _scalar(material, .985), _scalar(material, 1), micro))
        _output(base, unreal.MaterialProperty.MP_BASE_COLOR)
        _output(rough, unreal.MaterialProperty.MP_ROUGHNESS)
        _output(_scalar(material, definition['metallic']), unreal.MaterialProperty.MP_METALLIC)
        if definition.get('emission', 0):
            _output(_mul(material, base, _scalar(material, definition['emission'])),
                    unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        E.layout_material_expressions(material)
        E.recompile_material(material)
        statistics = E.get_statistics(material)
        pixel_instructions = int(statistics.get_editor_property('num_pixel_shader_instructions'))
        if pixel_instructions <= 0:
            raise RuntimeError('Material did not produce a compiled pixel shader: '+name)
        if not material.get_editor_property('used_with_instanced_static_meshes'):
            raise RuntimeError('Instanced mesh material usage did not persist: '+name)
        if not material.get_editor_property('used_with_nanite'):
            raise RuntimeError('Nanite material usage did not persist: '+name)
        self._save(material)
        self.materials[name] = material
        self.report['materials'].append({'name': name, 'kind': kind,
            'asset': material.get_path_name(), 'instanced_static_meshes': True, 'nanite_usage': True,
            'world_planar_tile_cm': definition.get('tile_cm',200),
            'pixel_shader_instructions': pixel_instructions,
            'component_masks_explicit': True, 'scan': definition.get('scan'),
            'glass_mode': 'opaque_frosted' if kind == 'glass' else None})
        unreal.log('INTERIOR MATERIAL READY: '+name)

    def _mesh(self, definition):
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_mesh = True
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        data = options.static_mesh_import_data
        data.combine_meshes = True
        data.auto_generate_collision = False
        data.generate_lightmap_u_vs = False
        data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
        # Blender's FBX object root carries the meters-to-centimeters scale.
        # Its pivot is already the canonical asset origin, so baking this root
        # preserves that origin while retaining the physical 100x scale.
        data.transform_vertex_to_absolute = True
        data.bake_pivot_in_vertex = False
        data.convert_scene = True
        data.convert_scene_unit = True
        data.force_front_x_axis = False
        data.import_uniform_scale = 1.0
        name = definition['name']
        mesh = self._import_file(SOURCE/definition['fbx'], DEST, name, options)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError('Static mesh import failed: '+name)
        raw_bounds = mesh.get_bounding_box()
        raw_min = [raw_bounds.min.x, raw_bounds.min.y, raw_bounds.min.z]
        raw_max = [raw_bounds.max.x, raw_bounds.max.y, raw_bounds.max.z]
        slots = []
        for index, slot in enumerate(mesh.get_editor_property('static_materials')):
            material_name = str(slot.get_editor_property('imported_material_slot_name'))
            if material_name not in definition['material_names'] or material_name not in self.materials:
                raise RuntimeError('Unrecognized material slot '+material_name+' on '+name)
            mesh.set_material(index, self.materials[material_name])
            slots.append(material_name)
        if set(slots) != set(definition['material_names']):
            raise RuntimeError('Source/imported material set mismatch: '+name)
        body = mesh.get_editor_property('body_setup')
        if not body:
            raise RuntimeError('Static mesh has no body setup: '+name)
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        instance = body.get_editor_property('default_instance')
        instance.set_editor_property('collision_profile_name', definition['collision'])
        instance.set_editor_property('collision_enabled',
            unreal.CollisionEnabled.QUERY_AND_PHYSICS if definition['collision']=='BlockAll' else
            unreal.CollisionEnabled.NO_COLLISION)
        body.set_editor_property('default_instance', instance)
        # Full-precision geometry preserves the physical room fitter contract;
        # material Nanite usage is ready if these small meshes are enabled later.
        bounds = mesh.get_bounding_box()
        lo = [bounds.min.x, bounds.min.y, bounds.min.z]
        hi = [bounds.max.x, bounds.max.y, bounds.max.z]
        error = max(abs(a-b) for a,b in zip(lo+hi,definition['minimum_cm']+definition['maximum_cm']))
        if error > .5:
            raise RuntimeError('Imported bounds differ by %.4fcm: %s; imported=%s/%s source=%s/%s' %
                (error,name,lo,hi,definition['minimum_cm'],definition['maximum_cm']))
        triangles = mesh.get_num_triangles(0)
        if triangles <= 0:
            raise RuntimeError('Mesh has no imported render triangles: '+name)
        self._save(mesh)
        reloaded = unreal.load_asset(DEST+'/'+name)
        for index, material_name in enumerate(slots):
            if reloaded.get_material(index).get_path_name() != self.materials[material_name].get_path_name():
                raise RuntimeError('Material assignment did not persist: '+name)
        self.report['assets'].append({'name':name, 'asset':mesh.get_path_name(),
            'minimum_cm':lo, 'maximum_cm':hi, 'source_minimum_cm':definition['minimum_cm'],
            'source_maximum_cm':definition['maximum_cm'], 'maximum_bounds_error_cm':error,
            'triangles':triangles, 'source_triangles':definition['triangles'],
            'raw_import_minimum_cm':raw_min, 'raw_import_maximum_cm':raw_max,
            'axis_normalization':'FBX export adapter; canonical authored axes retained',
            'collision_profile':str(instance.get_editor_property('collision_profile_name')),
            'collision_trace':'CTF_USE_COMPLEX_AS_SIMPLE', 'materials':slots,
            'source_fbx_sha256':_hash(SOURCE/definition['fbx']), 'pivot_preserved':True})
        unreal.log('INTERIOR MESH VALIDATED: '+name+' bounds error='+str(error)+' cm')

    def _reuse_palette(self, palette, previous):
        if previous.get('palette_sha256') != _hash(SOURCE_KIT/'palette.json'):
            raise RuntimeError('Cannot reuse shaders for a changed palette')
        records = {record['name']:record for record in previous.get('materials',[])}
        for definition in palette:
            name = definition['name']
            record = records.get(name,{})
            material = unreal.load_asset(DEST+'/Materials/'+name)
            if (not isinstance(material,unreal.Material) or
                record.get('pixel_shader_instructions',0)<=0 or
                not material.get_editor_property('used_with_instanced_static_meshes') or
                not material.get_editor_property('used_with_nanite')):
                raise RuntimeError('No previously compiled, saved palette material: '+name)
            self.materials[name] = material
            self.report['materials'].append(record)
        self.report['textures'] = previous.get('textures',[])
        self.report['reused_verified_palette'] = True

    def _preflight(self):
        manifest = json.loads((SOURCE_KIT/'asset-manifest.json').read_text(encoding='utf-8'))
        palette = json.loads((SOURCE_KIT/'palette.json').read_text(encoding='utf-8'))['materials']
        if manifest['schema_version'] != 1 or not manifest['assets']:
            raise RuntimeError('Unsupported or empty interior manifest')
        if manifest.get('asset_root',DEST) != DEST:
            raise RuntimeError('Manifest asset root violates destination boundary')
        names = [d['name'] for d in palette]
        if len(names) != len(set(names)):
            raise RuntimeError('Duplicate material definitions')
        for definition in palette:
            if not re.fullmatch(r'M_IF_[A-Za-z0-9_]+', definition['name']):
                raise RuntimeError('Invalid material name')
            if definition['kind'] not in {'plaster','tile','wood','fabric','metal','plain','glass','emissive'}:
                raise RuntimeError('Unsupported material kind')
            if len(definition['color']) != 3 or not all(math.isfinite(v) for v in definition['color']):
                raise RuntimeError('Invalid palette color')
            if float(definition.get('tile_cm',200)) <= 0:
                raise RuntimeError('Invalid material physical scale')
            if definition.get('scan'):
                if not re.fullmatch('[a-z0-9_]+', definition['scan']):
                    raise RuntimeError('Invalid scan identifier')
                for channel in ['diff','rough','nor_gl']:
                    if not (SOURCE/'External/PolyHaven'/f"{definition['scan']}_{channel}_2k.jpg").is_file():
                        raise RuntimeError('Missing retained CC0 scan '+definition['scan']+' '+channel)
        mesh_names = []
        for definition in manifest['assets']:
            mesh_names.append(definition['name'])
            if not re.fullmatch(r'SM_IF_[A-Za-z0-9_]+',definition['name']):
                raise RuntimeError('Invalid interior mesh name')
            file = (SOURCE/definition['fbx']).resolve()
            if not file.is_relative_to(SOURCE/'Exports/InteriorArt') or not file.is_file():
                raise RuntimeError('Missing or unauthorized FBX source: '+str(file))
            if file.stem != definition['name'] or definition['collision'] not in {'BlockAll','NoCollision'}:
                raise RuntimeError('Invalid mesh import definition')
            if set(definition['material_names'])-set(names):
                raise RuntimeError('FBX manifest references an undefined material')
            for key in ['minimum_cm','maximum_cm']:
                if len(definition[key]) != 3 or not all(math.isfinite(v) for v in definition[key]):
                    raise RuntimeError('Invalid source bounds')
        if len(mesh_names) != len(set(mesh_names)):
            raise RuntimeError('Duplicate mesh definitions')
        return manifest['assets'], palette

    def _write_report(self):
        content = json.dumps(self.report, ensure_ascii=False, indent=2)+'\n'
        for path in [SOURCE_KIT/'import-validation.json', ROOT/'Saved/Validation/InteriorArt/import-validation.json']:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(content,encoding='utf-8')

    def run(self):
        scope = os.environ.get('CITYCOURIER_INTERIOR_IMPORT_SCOPE','all')
        if scope not in {'all','meshes'}:
            raise RuntimeError('Invalid CITYCOURIER_INTERIOR_IMPORT_SCOPE: '+scope)
        command_line = unreal.SystemLibrary.get_command_line().lower()
        if scope == 'all' and '-run=pythonscript' in command_line and '-allowcommandletrendering' not in command_line:
            raise RuntimeError('Fresh interior palette shader validation requires a real renderer. '
                               'Run UnrealEditor-Cmd with -AllowCommandletRendering, or use an editor Python run. '
                               'The default Python commandlet uses Null RHI; no assets were changed.')
        definitions, palette = self._preflight()
        mesh_only = scope == 'meshes'
        previous = (json.loads((SOURCE_KIT/'import-validation.json').read_text(encoding='utf-8'))
                    if mesh_only else {})
        self.report['protected_before_sha256'] = protected_snapshot()
        self.report['manifest_sha256'] = _hash(SOURCE_KIT/'asset-manifest.json')
        self.report['palette_sha256'] = _hash(SOURCE_KIT/'palette.json')
        self.report['status'] = 'running'
        self._write_report()
        failure = None
        try:
            unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
            if mesh_only:
                self._reuse_palette(palette,previous)
            else:
                for definition in palette:
                    self._material(definition)
            for definition in definitions:
                self._mesh(definition)
        except Exception:
            failure = traceback.format_exc()
        after = protected_snapshot()
        before = self.report['protected_before_sha256']
        changed = [name for name in sorted(set(before)|set(after)) if before.get(name) != after.get(name)]
        self.report.update(protected_after_sha256=after, protected_changed=changed,
            protected_unchanged=not changed, saved_assets=self.created,
            finished_utc=datetime.now(timezone.utc).isoformat(),
            status='passed' if not failure and not changed else 'failed', error=failure)
        self._write_report()
        if failure or changed:
            raise RuntimeError('Interior import failed: '+(failure or '')+'; protected changes='+str(changed))
        unreal.log('INTERIOR FINISH IMPORT COMPLETE: '+str(len(self.report['assets']))+
                   ' meshes, '+str(len(self.materials))+' materials; all retained hashes unchanged')


if __name__ == '__main__':
    InteriorImporter().run()
