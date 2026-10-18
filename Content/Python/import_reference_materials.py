"""Import authored CC0 glTF PBR channels without discarding opacity or packed ARM."""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from polish_materials import node,scalar,color,link,output,multiply,finish

def apply(source,art,import_file):
    result={};cache={}
    assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    def sample(mat,filename,kind):
        key=(filename,kind)
        if key not in cache:
            tex=import_file(source/filename,art+'/Textures','T_PH_'+Path(filename).stem+'_'+kind)
            tex.set_editor_property('srgb',kind=='color')
            tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if kind=='normal' else unreal.TextureCompressionSettings.TC_DEFAULT)
            if kind=='normal':tex.set_editor_property('flip_green_channel',True)
            assets.save_loaded_asset(tex);cache[key]=tex
        return node(mat,unreal.MaterialExpressionTextureSample,texture=cache[key],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if kind=='color' else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    def channel(mat,tex,name):
        mask=node(mat,unreal.MaterialExpressionComponentMask,r=name=='R',g=name=='G',b=name=='B',a=False)
        link(tex,mask);return mask
    for d in json.loads((source/'reference_materials.json').read_text()):
        folder=art+'/Materials';path=folder+'/'+d['name']
        mat=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(d['name'],folder,unreal.Material,unreal.MaterialFactoryNew())
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
        unreal.MaterialEditingLibrary.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NANITE)
        mat.set_editor_property('two_sided',d['double_sided'])
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if d['alpha']!='OPAQUE' else unreal.BlendMode.BLEND_OPAQUE)
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE if d['foliage'] else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        base=color(mat,d['color'])
        if d['color_map']:base=multiply(mat,base,sample(mat,d['color_map'],'color'))
        output(base,unreal.MaterialProperty.MP_BASE_COLOR)
        if d['normal_map']:output(sample(mat,d['normal_map'],'normal'),unreal.MaterialProperty.MP_NORMAL)
        if d['arm_map']:
            arm=sample(mat,d['arm_map'],'linear')
            output(multiply(mat,channel(mat,arm,'G'),scalar(mat,d['roughness'])),unreal.MaterialProperty.MP_ROUGHNESS)
            output(multiply(mat,channel(mat,arm,'B'),scalar(mat,d['metallic'])),unreal.MaterialProperty.MP_METALLIC)
        else:
            output(scalar(mat,d['roughness']),unreal.MaterialProperty.MP_ROUGHNESS)
            output(scalar(mat,d['metallic']),unreal.MaterialProperty.MP_METALLIC)
        # Poly Haven's glTF may omit occlusionTexture even though ARM still
        # contains authored ambient occlusion in its red channel.
        ao=d['ao_map'] or d['arm_map']
        if ao:output(channel(mat,sample(mat,ao,'linear'),'R'),unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        if d['alpha']!='OPAQUE':
            if not d['opacity_map']:raise RuntimeError('Missing source opacity for '+d['name'])
            output(channel(mat,sample(mat,d['opacity_map'],'linear'),'R'),unreal.MaterialProperty.MP_OPACITY_MASK)
        if d['foliage']:output(multiply(mat,base,scalar(mat,.45)),unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        finish(mat);result[d['name']]=mat
    return result

if __name__=='__main__':
    def import_texture(file,folder,name):
        task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=folder;task.destination_name=name
        task.automated=True;task.replace_existing=True;task.save=True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex=unreal.load_asset(folder+'/'+name)
        if tex is None:raise RuntimeError('Missing imported reference texture '+name)
        return tex
    apply(Path(unreal.Paths.project_dir())/'AssetsSource/CityCourier','/Game/CityCourier/Art',import_texture)
