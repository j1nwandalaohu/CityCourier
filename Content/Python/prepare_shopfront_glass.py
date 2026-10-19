"""Create real transparent, collision-backed street display glazing."""
import unreal

path = '/Game/CityCourier/Art/Buildings/Materials/M_ShopGlass'
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
m = unreal.load_asset(path) if assets.does_asset_exist(path) else None
if not m:
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ShopGlass',path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
assert m
e = unreal.MaterialEditingLibrary
e.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided',True)
m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE)
color = e.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
color.set_editor_property('constant',unreal.LinearColor(.8,.9,.95,1))
e.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
for prop,value in ((unreal.MaterialProperty.MP_OPACITY,.06),(unreal.MaterialProperty.MP_ROUGHNESS,.08),
                   (unreal.MaterialProperty.MP_SPECULAR,.2)):
    node = e.create_material_expression(m,unreal.MaterialExpressionConstant)
    node.set_editor_property('r',value)
    e.connect_material_property(node,'',prop)
e.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
e.recompile_material(m)
assert m.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT
assert assets.save_loaded_asset(m)
unreal.log('SHOP GLASS: translucent opacity 0.06, instanced usage, native blocking cube geometry')
