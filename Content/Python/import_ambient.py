"""Import retained ambient sources using the independent original citizen rig."""
import json,pathlib,unreal
root=pathlib.Path(unreal.Paths.project_dir())
entries=json.loads((root/'AssetsSource/CityCourier/Ambient/catalog.json').read_text())
if globals().get('CITIZEN_ONLY',False):
    entries=[entry for entry in entries if entry['kind']=='skeletal']
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem);tools=unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
skeleton=unreal.load_asset('/Game/CityCourier/Art/Ambient/Citizen/SK_Courier_Skeleton')
assert skeleton, 'The retained independent citizen skeleton is required'
for name,color in [('CitizenCloth',(.12,.20,.31)),('CarPaint',(.24,.32,.38))]:
    if globals().get('CITIZEN_ONLY',False) and name!='CitizenCloth':continue
    path='/Game/CityCourier/Art/Materials/M_'+name
    material=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_'+name,'/Game/CityCourier/Art/Materials',unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    base=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector,-200,0)
    base.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant,-200,100)
    rough.set_editor_property('r',.75 if name=='CitizenCloth' else .3)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.set_material_usage(material,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    unreal.MaterialEditingLibrary.recompile_material(material);assets.save_loaded_asset(material)
for entry in entries:
    folder,name=entry['ue_path'].rsplit('/',1)
    if entry['kind']=='skeletal' and assets.does_asset_exist(entry['ue_path']):
        existing=unreal.load_asset(entry['ue_path'])
        # FBX reimport keeps the previous rig even if options name another one.
        # Recreate only this generated mesh when migrating from the courier rig.
        if existing.get_editor_property('skeleton')!=skeleton:
            assert assets.delete_asset(entry['ue_path'])
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(root/entry['source_path']))
    for key,value in [('destination_path',folder),('destination_name',name),('automated',True),('replace_existing',True),('save',True)]:task.set_editor_property(key,value)
    if entry['kind']!='audio':
        opts=unreal.FbxImportUI();opts.set_editor_property('automated_import_should_detect_type',False)
        opts.set_editor_property('import_materials',False);opts.set_editor_property('import_textures',False)
        skeletal=entry['kind']=='skeletal'
        opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
        opts.set_editor_property('import_as_skeletal',skeletal);opts.set_editor_property('import_animations',False)
        if skeletal:opts.set_editor_property('skeleton',skeleton);opts.set_editor_property('create_physics_asset',False)
        else:opts.static_mesh_import_data.set_editor_property('combine_meshes',True)
        task.set_editor_property('options',opts);task.set_editor_property('factory',unreal.FbxFactory())
    else:task.set_editor_property('factory',unreal.SoundFactory())
    tools.import_asset_tasks([task]);asset=unreal.load_asset(entry['ue_path']);assert asset
    if entry['kind']=='audio':
        asset.set_editor_property('looping',name in ['S_City','S_Motor'])
        asset.set_editor_property('virtualization_mode',unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    elif entry['kind']=='skeletal':
        slots=list(asset.get_editor_property('materials'))
        for i,slot in enumerate(slots):
            mat=unreal.load_asset('/Game/CityCourier/Art/Materials/M_'+str(slot.get_editor_property('material_slot_name')))
            assert mat;slot.set_editor_property('material_interface',mat);slots[i]=slot
        asset.set_editor_property('materials',slots)
        assert asset.get_editor_property('skeleton')==skeleton
    else:
        for i,slot in enumerate(asset.get_editor_property('static_materials')):
            mat=unreal.load_asset('/Game/CityCourier/Art/Materials/M_'+str(slot.get_editor_property('material_slot_name')))
            assert mat;asset.set_material(i,mat)
    assets.save_loaded_asset(asset)
assets.save_loaded_asset(skeleton)
for name,parent in [('BP_AmbientVehicle',unreal.AmbientVehicle),('BP_AmbientPedestrian',unreal.AmbientPedestrian)]:
    path='/Game/CityCourier/Blueprints/'+name
    if not assets.does_asset_exist(path):
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',parent)
        bp=tools.create_asset(name,'/Game/CityCourier/Blueprints',unreal.Blueprint,factory)
    else:bp=unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);assets.save_loaded_asset(bp)
unreal.log('CityCourier original ambient meshes and seven sound sources imported')
