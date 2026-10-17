"""Import original retained FBX sources at manifest paths, assign materials and rig graph."""
import csv
import json
import pathlib
import hashlib
import unreal
root=pathlib.Path(unreal.Paths.project_dir())
catalog=json.loads((root/'AssetsSource/CityCourier/art-catalog.json').read_text(encoding='utf-8'))
with (root/'docs/art/asset-manifest.csv').open(encoding='utf-8-sig',newline='') as f:
    manifest={row['asset_id']:row for row in csv.DictReader(f)}
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools=unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
colors={
    'Amber':(.92,.47,.045),'Teal':(.035,.20,.21),'Fabric':(.055,.075,.10),
    'Skin':(.52,.30,.19),'Hair':(.025,.018,.015),'White':(.76,.77,.70),
    'Rubber':(.018,.021,.025),'Steel':(.32,.36,.38),'Paint':(.055,.18,.19),
    'Glass':(.055,.10,.13),'Lamp':(.95,.68,.30),'RedLamp':(.6,.025,.012),
    'Plaster':(.48,.40,.30),'Concrete':(.24,.27,.26),'Brick':(.33,.12,.065),
    'Tile':(.26,.32,.29),'Wood':(.25,.115,.042),'Canvas':(.57,.12,.065),
    'Leaf':(.07,.19,.065),'WarmWindow':(.78,.48,.19),
    'SignTeal':(.025,.12,.13),'SignRed':(.30,.035,.025),'SignCream':(.84,.68,.40)}
mats={}
for name,color in colors.items():
    path='/Game/CityCourier/Art/Materials/M_'+name
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_'+name,'/Game/CityCourier/Art/Materials',unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
    base=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-400,0)
    base.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    if name in ['Plaster','Concrete','Brick','Tile','Fabric','Canvas','Wood']:
        noise=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionNoise,-600,-180)
        noise.set_editor_property('scale',.05 if name in ['Plaster','Concrete','Brick','Tile'] else 1.5)
        noise.set_editor_property('quality',1)
        noise.set_editor_property('levels',2)
        noise.set_editor_property('output_min',.8)
        noise.set_editor_property('output_max',1.05)
        varied=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionMultiply,-180,0)
        unreal.MaterialEditingLibrary.connect_material_expressions(base,'',varied,'A')
        unreal.MaterialEditingLibrary.connect_material_expressions(noise,'',varied,'B')
        unreal.MaterialEditingLibrary.connect_material_property(varied,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for prop,value,y in [(unreal.MaterialProperty.MP_ROUGHNESS,.28 if name in ['Steel','Paint','Glass'] else .7,120),
                         (unreal.MaterialProperty.MP_METALLIC,.8 if name=='Steel' else .3 if name in ['Glass','Paint'] else 0,240)]:
        scalar=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-400,y)
        scalar.set_editor_property('r',value)
        unreal.MaterialEditingLibrary.connect_material_property(scalar,'',prop)
    if name in ['Lamp','RedLamp','WarmWindow','SignCream']:
        glow=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionMultiply,-180,-100)
        glow.set_editor_property('const_b',.12 if name=='Lamp' else .05 if name=='RedLamp' else .008 if name=='WarmWindow' else .002)
        unreal.MaterialEditingLibrary.connect_material_expressions(base,'',glow,'A')
        unreal.MaterialEditingLibrary.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    unreal.MaterialEditingLibrary.recompile_material(m)
    assets.save_loaded_asset(m)
    mats[name]=m

skeleton=None
for entry in catalog['assets']:
    row=manifest[entry['asset_id']]
    assert row['is_placeholder']=='false'
    path=row['ue_path'];folder,name=path.rsplit('/',1)
    fingerprint=hashlib.sha256((root/row['source_path']).read_bytes()).hexdigest()
    existing=unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if not existing or assets.get_metadata_tag(existing,'CityCourierSourceHash')!=fingerprint:
        opts=unreal.FbxImportUI()
        opts.set_editor_property('automated_import_should_detect_type',False)
        opts.set_editor_property('import_materials',False)
        opts.set_editor_property('import_textures',False)
        if entry['kind']=='static':
            opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
            opts.set_editor_property('import_as_skeletal',False)
            opts.static_mesh_import_data.set_editor_property('combine_meshes',True)
            opts.static_mesh_import_data.set_editor_property('auto_generate_collision',True)
        elif entry['kind']=='skeletal':
            opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
            opts.set_editor_property('import_as_skeletal',True)
            opts.set_editor_property('import_animations',False)
            opts.set_editor_property('create_physics_asset',False)
        else:
            assert skeleton
            opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_ANIMATION)
            opts.set_editor_property('import_mesh',False)
            opts.set_editor_property('import_animations',True)
            opts.set_editor_property('skeleton',skeleton)
        task=unreal.AssetImportTask()
        task.set_editor_property('filename',str(root/row['source_path']))
        task.set_editor_property('destination_path',folder)
        task.set_editor_property('destination_name',name)
        task.set_editor_property('automated',True)
        task.set_editor_property('replace_existing',True)
        task.set_editor_property('save',True)
        task.set_editor_property('options',opts)
        task.set_editor_property('factory',unreal.FbxFactory())
        tools.import_asset_tasks([task])
        imported=list(task.get_editor_property('imported_object_paths'))
        if entry['kind']=='animation' and not assets.does_asset_exist(path):
            animations=[p for p in imported if isinstance(unreal.load_asset(p),unreal.AnimSequence)]
            assert len(animations)==1,(name,imported)
            assert assets.rename_asset(animations[0],path)
    asset=unreal.load_asset(path)
    assert asset,'Missing imported asset '+path
    if entry['kind']=='skeletal':
        skeleton=asset.get_editor_property('skeleton')
        assert skeleton,'Imported skeletal mesh has no skeleton'
        assets.save_loaded_asset(skeleton)
        slots=asset.get_editor_property('materials')
        for i,slot in enumerate(slots):
            slot_name=str(slot.get_editor_property('material_slot_name')).split('.')[0]
            if slot_name in mats:slot.set_editor_property('material_interface',mats[slot_name])
            slots[i]=slot
        asset.set_editor_property('materials',slots)
    if entry['kind']=='static':
        for i,slot in enumerate(asset.get_editor_property('static_materials')):
            slot_name=str(slot.get_editor_property('material_slot_name')).split('.')[0]
            if slot_name in mats:asset.set_material(i,mats[slot_name])
    assets.save_loaded_asset(asset)
    assets.set_metadata_tag(asset,'CityCourierSourceHash',fingerprint)
    assets.save_loaded_asset(asset)
    unreal.log('CityCourier imported '+name)

bp_path='/Game/CityCourier/Blueprints/ABP_Courier'
if assets.does_asset_exist(bp_path):
    bp=unreal.load_asset(bp_path)
else:
    factory=unreal.AnimBlueprintFactory()
    factory.set_editor_property('parent_class',unreal.CourierAnimInstance)
    factory.set_editor_property('target_skeleton',skeleton)
    bp=tools.create_asset('ABP_Courier','/Game/CityCourier/Blueprints',unreal.AnimBlueprint,factory)
bp.set_editor_property('target_skeleton',skeleton)
assert unreal.CourierArtBuilder.build_animation_blueprint(bp),'Animation graph construction failed'
assets.save_loaded_asset(bp)
for name in ['BP_CourierCharacter','BP_Scooter']:
    bp=unreal.load_asset('/Game/CityCourier/Blueprints/'+name)
    if bp:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_loaded_asset(bp)
unreal.log('CityCourier original meshes and animation graph imported')
