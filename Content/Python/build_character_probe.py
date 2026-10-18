"""A separate lighting/animation lab. Existing gameplay and art maps are untouched."""
import hashlib,json,sys
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Content/Python'))
from polish_materials import node,scalar,color,link,output,finish
ART='/Game/CityCourier/Art';LAB=ART+'/Experiments/CharacterComparison'
source=root/'AssetsSource/CityCourier'
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools=unreal.AssetToolsHelpers.get_asset_tools()

def import_file(file,folder,name,options=None):
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True
    if options:task.options=options
    tools.import_asset_tasks([task]);obj=unreal.load_asset(folder+'/'+name)
    if not obj:raise RuntimeError('Missing import '+name)
    return obj

catalog=json.loads((source/'character_comparison_materials.json').read_text())
cache={};materials={}
for name,d in catalog['materials'].items():
    folder=LAB+'/Materials';path=folder+'/'+name
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
    samples={}
    for channel,filename in d['textures'].items():
        file=source/filename
        if hashlib.sha256(file.read_bytes()).hexdigest()!=d['texture_sha256'][channel]:raise RuntimeError('Comparison texture checksum: '+filename)
        key=(filename,channel)
        if key not in cache:
            tex=import_file(file,LAB+'/Textures','T_'+file.stem)
            tex.set_editor_property('srgb',channel=='base_color')
            if channel=='normal':
                tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property('flip_green_channel',True)
            assets.save_loaded_asset(tex);cache[key]=tex
        samples[channel]=node(m,unreal.MaterialExpressionTextureSample,texture=cache[key],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='base_color' else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    output(samples.get('base_color') or color(m,d['base_color']),unreal.MaterialProperty.MP_BASE_COLOR)
    output(samples.get('roughness') or scalar(m,d['roughness']),unreal.MaterialProperty.MP_ROUGHNESS)
    output(scalar(m,d['metallic']),unreal.MaterialProperty.MP_METALLIC)
    if 'normal' in samples:output(samples['normal'],unreal.MaterialProperty.MP_NORMAL)
    if d['subsurface']['weight']>0:
        # Use the same shading family as the existing MakeHuman rider. Imported
        # texture/geometry quality is compared under identical lab lighting.
        m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_SUBSURFACE)
        output(color(m,(.45,.16,.09)),unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        output(scalar(m,.15),unreal.MaterialProperty.MP_OPACITY)
        output(scalar(m,.28),unreal.MaterialProperty.MP_SPECULAR)
    if d['alpha']['mode']=='masked':
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property('two_sided',True);m.set_editor_property('opacity_mask_clip_value',d['alpha']['clip_threshold'])
        if not unreal.MaterialEditingLibrary.connect_material_property(samples['base_color'],'A',unreal.MaterialProperty.MP_OPACITY_MASK):raise RuntimeError('Sclera mask connection failed')
    finish(m);materials[name]=m

for name in ('SM_MakeHumanBust','SM_CharMorphBust'):
    options=unreal.FbxImportUI();options.automated_import_should_detect_type=False;options.import_materials=False;options.import_textures=False
    options.import_as_skeletal=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=False
    obj=import_file(source/'Exports/CharacterComparison'/(name+'.fbx'),LAB,name,options)
    for i,slot in enumerate(obj.get_editor_property('static_materials')):
        material_name=str(slot.get_editor_property('imported_material_slot_name'))
        material=materials.get(material_name) or unreal.load_asset(ART+'/Materials/'+material_name)
        if not material:raise RuntimeError('Missing material '+material_name)
        obj.set_material(i,material)
    assets.save_loaded_asset(obj)

map_path='/Game/CityCourier/Maps/L_CharacterProbe'
if assets.does_asset_exist(map_path):levels.load_level(map_path)
else:levels.new_level(map_path)
for actor in list(actors.get_all_level_actors()):
    if any(str(t).startswith('CC_Probe_') for t in actor.tags):actors.destroy_actor(actor)
def spawn(cls,tag,pos,yaw=0):
    a=actors.spawn_actor_from_class(cls,unreal.Vector(*pos));a.tags=[unreal.Name('CC_Probe_'+tag)]
    a.set_actor_label('CC_Probe_'+tag);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);return a
floor_mat=unreal.load_asset(LAB+'/M_LabFloor') or tools.create_asset('M_LabFloor',LAB,unreal.Material,unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(floor_mat)
output(color(floor_mat,(.13,.13,.13)),unreal.MaterialProperty.MP_BASE_COLOR);output(scalar(floor_mat,.8),unreal.MaterialProperty.MP_ROUGHNESS);finish(floor_mat)
floor=spawn(unreal.StaticMeshActor,'Floor',(0,0,-10));floor.set_actor_scale3d(unreal.Vector(60,60,.2))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));floor.static_mesh_component.set_material(0,floor_mat)
grid_mat=unreal.load_asset(LAB+'/M_LabGrid') or tools.create_asset('M_LabGrid',LAB,unreal.Material,unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(grid_mat)
output(color(grid_mat,(.045,.045,.045)),unreal.MaterialProperty.MP_BASE_COLOR)
output(scalar(grid_mat,.8),unreal.MaterialProperty.MP_ROUGHNESS);finish(grid_mat)
# One-metre markers remain fixed in world space, making foot sliding visible.
for index in range(19):
    a=spawn(unreal.StaticMeshActor,'GridY'+str(index),(0,200-index*100,.025))
    a.set_actor_scale3d(unreal.Vector(6,.012,.0005))
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,grid_mat)
    a.static_mesh_component.set_collision_profile_name('NoCollision')
for index in range(7):
    a=spawn(unreal.StaticMeshActor,'GridX'+str(index),(-300+index*100,-700,.025))
    a.set_actor_scale3d(unreal.Vector(.012,18,.0005))
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,grid_mat)
    a.static_mesh_component.set_collision_profile_name('NoCollision')
for tag,x in [('Before',65),('After',-65)]:
    a=spawn(unreal.SkeletalMeshActor,tag,(x,0,2 if tag=='Before' else 0),180)
    c=a.get_component_by_class(unreal.SkeletalMeshComponent);c.set_skeletal_mesh_asset(unreal.load_asset(ART+'/Characters/SK_Courier'))
    c.set_collision_profile_name('NoCollision');c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
for tag,name in [('MakeHuman','SM_MakeHumanBust'),('CharMorph','SM_CharMorphBust')]:
    a=spawn(unreal.StaticMeshActor,tag,(0,0,0),180)
    a.static_mesh_component.set_static_mesh(unreal.load_asset(LAB+'/'+name));a.static_mesh_component.set_collision_profile_name('NoCollision')
camera=spawn(unreal.CameraActor,'Camera',(250,-350,180));camera.camera_component.set_field_of_view(48)
for tag,pos,target,power,size in [('Key',(-180,-260,340),(0,0,140),12,170),('Fill',(230,-130,230),(0,0,140),4,220),('Rim',(0,190,300),(0,0,140),10,150)]:
    a=spawn(unreal.RectLight,tag,pos);a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*pos),unreal.Vector(*target)),False)
    c=a.light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity',power);c.set_editor_property('attenuation_radius',8000)
    c.set_editor_property('source_width',size);c.set_editor_property('source_height',size)
sky=spawn(unreal.SkyLight,'Sky',(0,0,0));sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property('cubemap',unreal.load_asset(ART+'/Textures/T_EveningPanorama'))
sky.light_component.set_editor_property('intensity',.4)
pp=spawn(unreal.PostProcessVolume,'Exposure',(0,0,0));pp.set_editor_property('unbound',True);settings=pp.get_editor_property('settings')
for n in ('min','max'):
    settings.set_editor_property('override_auto_exposure_'+n+'_brightness',True)
    settings.set_editor_property('auto_exposure_'+n+'_brightness',1.)
settings.set_editor_property('override_bloom_intensity',True);settings.set_editor_property('bloom_intensity',.05)
pp.set_editor_property('settings',settings)
if not levels.save_current_level():raise RuntimeError('Failed to save probe map')
unreal.log('CHARACTER PROBE LAB READY '+map_path)
