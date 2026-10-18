"""PBR surface detail shared by asset import and the art-map rebuild."""
import json
from pathlib import Path
import unreal

ART='/Game/CityCourier/Art'
E=unreal.MaterialEditingLibrary
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

def node(m,cls,**props):
    n=E.create_material_expression(m,cls)
    for key,value in props.items():n.set_editor_property(key,value)
    return n

def scalar(m,v):return node(m,unreal.MaterialExpressionConstant,r=v)
def color(m,v):return node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*v,1))
def link(a,b,pin='',output=''):
    if not E.connect_material_expressions(a,output,b,pin):raise RuntimeError(f'Material connection failed: {b.get_name()}.{pin}')
def output(n,prop):
    if not E.connect_material_property(n,'',prop):raise RuntimeError('Material output failed: '+str(prop))
def multiply(m,a,b):
    n=node(m,unreal.MaterialExpressionMultiply);link(a,n,'A');link(b,n,'B');return n
def add(m,a,b):
    n=node(m,unreal.MaterialExpressionAdd);link(a,n,'A');link(b,n,'B');return n
def mix(m,a,b,alpha):
    n=node(m,unreal.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(alpha,n,'Alpha');return n
def texture(m,name,normal=False,linear=False,uv=None):
    tex=unreal.load_asset(ART+'/Textures/T_'+name)
    if not tex:raise RuntimeError('Missing surface texture '+name)
    n=node(m,unreal.MaterialExpressionTextureSample,texture=tex,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    if uv:link(uv,n,'UVs')
    return n

def macro(m,scale=.003):
    position=node(m,unreal.MaterialExpressionWorldPosition)
    p=multiply(m,position,scalar(m,scale))
    noise=node(m,unreal.MaterialExpressionNoise,scale=1.0,quality=1,levels=2,output_min=0.0,output_max=1.0)
    link(p,noise)
    clamped=node(m,unreal.MaterialExpressionClamp);link(noise,clamped)
    return clamped

def world_uv(m,centimeters):
    position=node(m,unreal.MaterialExpressionWorldPosition)
    mask=node(m,unreal.MaterialExpressionComponentMask,r=True,g=True);link(position,mask)
    return multiply(m,mask,scalar(m,1/centimeters))

def finish(m):E.recompile_material(m);assets.save_loaded_asset(m)

def apply():
    definitions=json.loads((Path(unreal.Paths.project_dir())/'AssetsSource/CityCourier/materials.json').read_text())
    scans={'M_Plaster':(.70,.68,.62),'M_Concrete':(.65,.64,.59),'M_Brick':(.70,.64,.55),'M_Asphalt':(.48,.48,.46),'M_Pavers':(.67,.67,.63)}
    detail={'M_Rubber','M_Seat','M_CourierGold','M_ScooterPaint','M_Helmet','M_PaintedMetal','M_Aluminum','M_Skin','M_WorkClothes','M_WindowGlass','M_WindowWarm','M_Leaf'}
    for d in definitions:
        name=d['name']
        if name not in scans and name not in detail:continue
        m=unreal.load_asset(ART+'/Materials/'+name)
        if not m:continue
        E.delete_all_material_expressions(m)
        tex={}
        for f in d['textures']:
            normal='_nor_gl_' in f or '_normal.' in f;rough='_rough_' in f
            tex['normal' if normal else 'rough' if rough else 'color']=texture(m,Path(f).stem,normal,rough)
        base=tex.get('color') or color(m,d['color'])
        rough=tex.get('rough') or scalar(m,d['roughness'])
        metallic=d['metallic']
        if name in scans:
            variation=mix(m,scalar(m,.90),scalar(m,1),macro(m,.002))
            base=multiply(m,multiply(m,base,color(m,scans[name])),variation)
            rough=mix(m,rough,scalar(m,.94),multiply(m,macro(m,.014),scalar(m,.08)))
        if name in {'M_Rubber','M_Seat','M_CourierGold','M_ScooterPaint','M_Helmet','M_PaintedMetal','M_Aluminum'}:
            variation=macro(m,.16 if name in {'M_Rubber','M_Seat'} else .035)
            base=multiply(m,base,mix(m,scalar(m,.94),scalar(m,1),variation))
            rough=mix(m,scalar(m,max(.18,d['roughness']-.04)),scalar(m,min(.95,d['roughness']+.05)),variation)
            if name in {'M_ScooterPaint','M_Helmet','M_CourierGold'}:metallic=0
        if name=='M_Skin':
            m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_SUBSURFACE)
            output(color(m,(.45,.16,.09)),unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
            output(scalar(m,.15),unreal.MaterialProperty.MP_OPACITY)
            output(scalar(m,.28),unreal.MaterialProperty.MP_SPECULAR)
            rough=mix(m,scalar(m,.43),scalar(m,.52),macro(m,1.8))
        if name=='M_WorkClothes':
            output(scalar(m,.18),unreal.MaterialProperty.MP_SPECULAR)
            rough=scalar(m,.88)
        if name=='M_WindowGlass':
            base=color(m,(.012,.022,.027));metallic=0
            rough=mix(m,scalar(m,.10),scalar(m,.14),macro(m,.025))
        if name=='M_WindowWarm':
            base=color(m,(.22,.16,.085));metallic=0;rough=scalar(m,.55)
        if name=='M_Leaf':
            m.set_editor_property('two_sided',True)
            m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
            output(color(m,(.07,.12,.018)),unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
            base=multiply(m,base,mix(m,color(m,(.45,.65,.45)),color(m,(1.25,1.05,.50)),macro(m,.11)))
        output(base,unreal.MaterialProperty.MP_BASE_COLOR)
        output(rough,unreal.MaterialProperty.MP_ROUGHNESS)
        output(scalar(m,metallic),unreal.MaterialProperty.MP_METALLIC)
        if 'normal' in tex:
            normal=tex['normal']
            if name=='M_WorkClothes':
                flattened=multiply(m,normal,color(m,(.4,.4,1)))
                normal=node(m,unreal.MaterialExpressionNormalize);link(flattened,normal)
            output(normal,unreal.MaterialProperty.MP_NORMAL)
        if d['emission']:output(multiply(m,base,scalar(m,d['emission'])),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        finish(m)

def ground(name,scan,scale,tint,damp=False):
    tools=unreal.AssetToolsHelpers.get_asset_tools();path=ART+'/Materials/'+name
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,ART+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m);uv=world_uv(m,scale)
    base=texture(m,scan+'_diff_2k',uv=uv);rough=texture(m,scan+'_rough_2k',linear=True,uv=uv)
    variation=macro(m,.004)
    base=multiply(m,multiply(m,base,color(m,tint)),mix(m,scalar(m,.88),scalar(m,1),variation))
    if damp:
        wet=multiply(m,variation,scalar(m,.2))
        base=mix(m,base,multiply(m,base,scalar(m,.75)),wet)
        rough=mix(m,rough,scalar(m,.35),wet)
    output(base,unreal.MaterialProperty.MP_BASE_COLOR);output(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    output(texture(m,scan+'_nor_gl_2k',normal=True,uv=uv),unreal.MaterialProperty.MP_NORMAL)
    finish(m);return m

def sky_material():
    path=ART+'/Materials/M_EveningPanorama';tools=unreal.AssetToolsHelpers.get_asset_tools()
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_EveningPanorama',ART+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True);m.set_editor_property('is_sky',True)
    sample=node(m,unreal.MaterialExpressionTextureSample,texture=unreal.load_asset(ART+'/Textures/T_EveningPanorama'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    position=node(m,unreal.MaterialExpressionWorldPosition);camera=node(m,unreal.MaterialExpressionCameraPositionWS)
    delta=node(m,unreal.MaterialExpressionSubtract);link(position,delta,'A');link(camera,delta,'B')
    normal=node(m,unreal.MaterialExpressionNormalize);link(delta,normal);link(normal,sample,'UVs')
    output(multiply(m,sample,scalar(m,.65)),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m);return m
