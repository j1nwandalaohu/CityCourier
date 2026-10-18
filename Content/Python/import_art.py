"""Import reproducible art sources and wire visual subclasses into existing blueprints.

Run in the merged project. No gameplay rules or original city map are rewritten.
"""
import csv
import json
import os
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
SOURCE=ROOT/'AssetsSource/CityCourier'
ART='/Game/CityCourier/Art'
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools=unreal.AssetToolsHelpers.get_asset_tools()
materials={}
scope=os.environ.get('CITYCOURIER_ART_IMPORT_SCOPE','all')
if scope not in ('all','characters'):raise RuntimeError('Invalid CITYCOURIER_ART_IMPORT_SCOPE')

def import_file(file,folder,name,options=None):
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True
    if options:task.options=options
    tools.import_asset_tasks([task])
    result=unreal.load_asset(folder+'/'+name)
    if result is None:raise RuntimeError('Import did not produce '+folder+'/'+name+'; '+str(task.imported_object_paths))
    return result

def constant(mat,value,kind=unreal.MaterialExpressionConstant):
    node=unreal.MaterialEditingLibrary.create_material_expression(mat,kind)
    if kind==unreal.MaterialExpressionConstant:node.set_editor_property('r',value)
    else:node.set_editor_property('constant',unreal.LinearColor(*value,1))
    return node

for definition in json.loads((SOURCE/'materials.json').read_text(encoding='utf-8')):
    name=definition['name']
    if not name.startswith('M_'):continue
    folder=ART+'/Materials';path=folder+'/'+name
    mat=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    unreal.MaterialEditingLibrary.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    unreal.MaterialEditingLibrary.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NANITE)
    if name=='M_Leaf':mat.set_editor_property('two_sided',True)
    properties=[('color',unreal.MaterialProperty.MP_BASE_COLOR),('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('metallic',unreal.MaterialProperty.MP_METALLIC)]
    for key,prop in properties:
        node=constant(mat,definition[key],unreal.MaterialExpressionConstant3Vector if key=='color' else unreal.MaterialExpressionConstant)
        unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
    if definition['emission']:
        value=[c*definition['emission'] for c in definition['color']]
        unreal.MaterialEditingLibrary.connect_material_property(constant(mat,value,unreal.MaterialExpressionConstant3Vector),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    for filename in definition['textures']:
        matches=list((SOURCE/'External').rglob(filename))
        if len(matches)!=1:raise RuntimeError('Ambiguous or missing source texture: '+filename)
        texture=import_file(matches[0],ART+'/Textures','T_'+Path(filename).stem)
        normal='_nor_gl_' in filename or '_normal.' in filename;rough='_rough_' in filename
        texture.set_editor_property('srgb',not(normal or rough))
        if normal:
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property('flip_green_channel',True)
        assets.save_loaded_asset(texture)
        node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
        node.set_editor_property('texture',texture)
        node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if rough else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        prop=unreal.MaterialProperty.MP_NORMAL if normal else unreal.MaterialProperty.MP_ROUGHNESS if rough else unreal.MaterialProperty.MP_BASE_COLOR
        unreal.MaterialEditingLibrary.connect_material_property(node,'RGB',prop)
    unreal.MaterialEditingLibrary.recompile_material(mat);assets.save_loaded_asset(mat);materials[name]=mat

sky=import_file(SOURCE/'External/PolyHaven/sunset_jhbcentral_1k.hdr',ART+'/Textures','T_EveningSky')
import_file(SOURCE/'External/PolyHaven/industrial_sunset_02_puresky_2k.hdr',ART+'/Textures','T_EveningPanorama')
sys.path.insert(0,str(ROOT/'Content/Python'))
import polish_materials
polish_materials.apply()
import import_reference_materials
materials.update(import_reference_materials.apply(SOURCE,ART,import_file))

def fbx_options(skeletal=False,animation=False,skeleton=None):
    o=unreal.FbxImportUI();o.automated_import_should_detect_type=False
    o.import_materials=False;o.import_textures=False;o.import_as_skeletal=skeletal
    o.import_mesh=not animation;o.import_animations=animation
    o.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH
    if skeleton:o.skeleton=skeleton
    if skeletal:o.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
    if not skeletal:
        o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False
        o.static_mesh_import_data.generate_lightmap_u_vs=False
    return o

for file in ([] if scope=='characters' else sorted((SOURCE/'Exports').glob('*/*.fbx'))):
    if file.parent.name in ('Characters','MotionProbe','CharacterComparison'):continue
    mesh=import_file(file,ART+'/'+file.parent.name,file.stem,fbx_options())
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        name=str(slot.get_editor_property('imported_material_slot_name'))
        if name not in materials:name=str(slot.get_editor_property('material_slot_name'))
        if name not in materials:raise RuntimeError('Missing static material '+name+' on '+file.stem)
        mesh.set_material(i,materials[name])
    if file.parent.name in ('Architecture','ReferenceProps'):
        settings=mesh.get_editor_property('nanite_settings');settings.enabled=True;mesh.set_editor_property('nanite_settings',settings)
    assets.save_loaded_asset(mesh)

# The legacy FBX path merges added joints on skeletal reimport. Interchange in
# UE 5.6 can update the mesh while leaving its shared skeleton's bone tree stale.
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
existing_skeleton=unreal.load_asset(ART+'/Characters/SK_Courier_Skeleton') if assets.does_asset_exist(ART+'/Characters/SK_Courier_Skeleton') else None
character=import_file(SOURCE/'Exports/Characters/SK_Courier.fbx',ART+'/Characters','SK_Courier',fbx_options(True,skeleton=existing_skeleton))
slots=list(character.get_editor_property('materials'))
for slot in slots:
    name=str(slot.get_editor_property('material_slot_name'))
    if name not in materials:raise RuntimeError('Missing rider material '+name)
    slot.material_interface=materials[name]
character.set_editor_property('materials',slots);assets.save_loaded_asset(character)
for slot in character.get_editor_property('materials'):
    if not slot.material_interface.get_path_name().startswith(ART+'/Materials/'):
        raise RuntimeError('Rider material assignment did not persist: '+str(slot))
skeleton=character.get_editor_property('skeleton')
if not assets.save_loaded_asset(skeleton,only_if_is_dirty=False):raise RuntimeError('Cannot persist expanded courier skeleton')
clips=[]
for name in ['A_Idle','A_Walk','A_Run','A_Ride']:
    clips.append(import_file(SOURCE/f'Exports/Characters/{name}.fbx',ART+'/Characters',name,fbx_options(True,True,skeleton)))

abp_path='/Game/CityCourier/Blueprints/ABP_Courier'
if assets.does_asset_exist(abp_path):abp=unreal.load_asset(abp_path)
else:
    factory=unreal.AnimBlueprintFactory();factory.set_editor_property('parent_class',unreal.CourierAnimInstance);factory.set_editor_property('target_skeleton',skeleton)
    abp=tools.create_asset('ABP_Courier','/Game/CityCourier/Blueprints',unreal.AnimBlueprint,factory)
unreal.get_default_object(abp.generated_class()).set_editor_property('clips',clips)
unreal.BlueprintEditorLibrary.compile_blueprint(abp);assets.save_loaded_asset(abp)

bp=unreal.load_asset('/Game/CityCourier/Blueprints/BP_CourierCharacter')
unreal.BlueprintEditorLibrary.reparent_blueprint(bp,unreal.CourierArtCharacter)
defaults=unreal.get_default_object(bp.generated_class());component=defaults.get_editor_property('mesh')
component.set_skeletal_mesh_asset(character);component.set_anim_instance_class(abp.generated_class())
for part in defaults.get_editor_property('prototype_parts'):part.set_visibility(False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assets.save_loaded_asset(bp)
bp=unreal.load_asset('/Game/CityCourier/Blueprints/BP_Scooter')
unreal.BlueprintEditorLibrary.reparent_blueprint(bp,unreal.ScooterArtPawn)
defaults=unreal.get_default_object(bp.generated_class())
defaults.get_editor_property('body_art').set_static_mesh(unreal.load_asset(ART+'/Scooter/SM_ScooterBody'))
for prop in ['front_wheel_art','rear_wheel_art']:defaults.get_editor_property(prop).set_static_mesh(unreal.load_asset(ART+'/Scooter/SM_ScooterWheel'))
for part in defaults.get_editor_property('prototype_parts'):part.set_visibility(False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assets.save_loaded_asset(bp)
unreal.log('CityCourier ART IMPORT COMPLETE: meshes, four clips, animation blueprint and visual subclasses')
