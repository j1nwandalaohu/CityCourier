"""Import real combat clips and promote the new jog/sprint to production."""
import hashlib,json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
source=root/'AssetsSource/CityCourier'
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/CityCourier/Art/Characters'
protected=[root/'Content/CityCourier/Art/Characters'/name for name in ['SK_Courier.uasset','SK_Courier_Skeleton.uasset','A_Ride.uasset']]
before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
skeleton=unreal.load_asset(folder+'/SK_Courier_Skeleton')
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
names=['A_Jog','A_Sprint','A_LightPunch_L','A_LightPunch_R','A_HeavyPunch_R']
for name in names:
    path=source/'Exports/ProductionActions'/f'{name}.fbx'
    assert path.is_file(), 'Generate production FBX first: '+str(path)
    options=unreal.FbxImportUI()
    options.automated_import_should_detect_type=False
    options.import_mesh=False;options.import_as_skeletal=True;options.import_animations=True
    options.import_materials=False;options.import_textures=False;options.skeleton=skeleton
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate',False)
    options.anim_sequence_import_data.set_editor_property('custom_sample_rate',60 if 'Punch' in name else 30)
    task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True;task.options=options;task.factory=unreal.FbxFactory()
    tools.import_asset_tasks([task])
    clip=unreal.load_asset(folder+'/'+name)
    assert isinstance(clip,unreal.AnimSequence) and clip.get_editor_property('skeleton')==skeleton
    clip.set_editor_property('enable_root_motion',False)
    assert assets.save_loaded_asset(clip)
bp=unreal.load_asset('/Game/CityCourier/Blueprints/ABP_Courier')
assert unreal.CourierArtBuilder.build_animation_blueprint(bp)
clips=[unreal.load_asset(folder+'/'+name) for name in ['A_IdleRelaxed','A_Jog','A_Sprint','A_Ride']]
assert all(clips)
unreal.get_default_object(bp.generated_class()).set_editor_property('clips',clips)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp)
for p in protected:
    assert before[str(p)]==hashlib.sha256(p.read_bytes()).hexdigest(),'Protected rider/rig/ride changed: '+str(p)
unreal.log('PRODUCTION ACTION IMPORT COMPLETE '+json.dumps({name:unreal.load_asset(folder+'/'+name).get_play_length() for name in names}))
