"""Native UE surface graphs for CC0 PBR scans and original roasted skin."""
from pathlib import Path
import unreal
from import_interior_finish import (SOURCE,DEST,E,_node,_scalar,_color,_output,_link,
    _mask,_mul,_mix,_world_uv,_noise,_unary,_append,_hash,_binary,_sub)


def apply_surface(importer, material, definition):
    maps=definition.get('pbr_maps',{})
    uv,position=_world_uv(material,definition.get('tile_cm',100))
    authored=definition.get('uv_mode')=='authored'
    if authored:uv=_node(material,unreal.MaterialExpressionTextureCoordinate)
    sources=[]
    def sample(channel):
        file=SOURCE/maps[channel]
        key=(maps[channel],channel)
        if key not in importer.textures:
            normal=channel=='normal'
            texture=importer._import_file(file,DEST+'/Textures','T_RI_'+file.stem)
            texture.set_editor_property('srgb',channel=='color')
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP
                if normal else unreal.TextureCompressionSettings.TC_DEFAULT)
            texture.set_editor_property('flip_green_channel',normal)
            importer._save(texture);importer.textures[key]=texture
            importer.report['textures'].append(dict(asset=texture.get_path_name(),source=maps[channel],
                source_sha256=_hash(file),srgb=channel=='color',flip_green_channel=normal))
        texture=importer.textures[key]
        sampler=(unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='normal' else
                 unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='color' else
                 unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        node=_node(material,unreal.MaterialExpressionTextureSample,texture=texture,sampler_type=sampler)
        _link(uv,node,'UVs');sources.append(texture.get_path_name())
        return node
    if definition.get('procedural_skin'):
        material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        patches=_noise(material,position,.035)
        pores=_noise(material,position,7)
        base=_mix(material,_color(material,[.12,.025,.005]),
                  _color(material,[.23,.065,.014]),patches)
        base=_mul(material,base,_mix(material,_scalar(material,.96),_scalar(material,1.02),pores))
        spots=_noise(material,position,.4)
        base=_mul(material,base,_mix(material,_scalar(material,.88),_scalar(material,1.05),spots))
        rough=_mix(material,_scalar(material,.40),_scalar(material,.53),pores)
        nx=_mul(material,_sub(material,pores,_scalar(material,.5)),_scalar(material,.06))
        ny=_mul(material,_sub(material,_noise(material,position,9.3),_scalar(material,.5)),_scalar(material,.06))
        _output(_unary(material,unreal.MaterialExpressionNormalize,
            _append(material,_append(material,nx,ny),_scalar(material,1))),unreal.MaterialProperty.MP_NORMAL)
        _output(_scalar(material,.35),unreal.MaterialProperty.MP_SPECULAR)
        metal=_scalar(material,0)
    else:
        base=sample('color')
        base=_mul(material,base,_color(material,definition.get('color_tint',definition.get('color',[1,1,1]))
              if authored or 'color_tint' in definition else [1,1,1]))
        if 'arm' in maps:
            arm=sample('arm')
            rough=_mul(material,_mask(material,arm,'g'),_scalar(material,definition['roughness']))
            metal=_mul(material,_mask(material,arm,'b'),_scalar(material,definition['metallic']))
            # glTF metallicRoughnessTexture only defines G/B. Food scans use a
            # grayscale roughness image; its red channel is not occlusion.
            if '_arm_' in Path(maps['arm']).stem:
                _output(_mask(material,arm,'r'),unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        else:
            rough=sample('rough') if 'rough' in maps else _scalar(material,definition['roughness'])
            metal=sample('metal') if 'metal' in maps else _scalar(material,definition['metallic'])
        if definition.get('rough_min'):
            rough=_binary(material,unreal.MaterialExpressionMax,rough,_scalar(material,definition['rough_min']))
        if 'normal' in maps:
            strength=definition.get('normal_strength',1)
            normal=_unary(material,unreal.MaterialExpressionNormalize,
                          _mul(material,sample('normal'),_color(material,[strength,strength,1])))
            _output(normal,unreal.MaterialProperty.MP_NORMAL)
        if definition.get('paint_soffit'):
            normal=_node(material,unreal.MaterialExpressionVertexNormalWS)
            up=_unary(material,unreal.MaterialExpressionClamp,_mask(material,normal,'b'))
            base=_mix(material,_color(material,[.66,.67,.63]),base,up)
            rough=_mix(material,_scalar(material,.8),rough,up)
    _output(base,unreal.MaterialProperty.MP_BASE_COLOR)
    _output(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    _output(metal,unreal.MaterialProperty.MP_METALLIC)
    E.layout_material_expressions(material);E.recompile_material(material)
    statistics=E.get_statistics(material)
    instructions=int(statistics.get_editor_property('num_pixel_shader_instructions'))
    if instructions<=0:raise RuntimeError('PBR shader compilation failed: '+definition['name'])
    importer._save(material);importer.materials[definition['name']]=material
    importer.report['materials'].append(dict(name=definition['name'],kind=definition['kind'],
        asset=material.get_path_name(),instanced_static_meshes=True,nanite_usage=True,
        pixel_shader_instructions=instructions,uv_mode='authored' if authored else 'world_planar',
        world_planar_tile_cm=definition.get('tile_cm'),pbr_maps=maps,textures=sorted(set(sources)),
        normal_strength=definition.get('normal_strength'),procedural_skin=bool(definition.get('procedural_skin'))))
    unreal.log('REALISTIC SURFACE READY: '+definition['name'])
