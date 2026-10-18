"""Import experimental clips against the existing skeleton without replacing it."""
import hashlib,json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve();source=root/'AssetsSource/CityCourier'
folder='/Game/CityCourier/Art/Experiments/Mesh2Motion'
skeleton_path='/Game/CityCourier/Art/Characters/SK_Courier_Skeleton'
package=root/'Content/CityCourier/Art/Characters/SK_Courier_Skeleton.uasset'
before=hashlib.sha256(package.read_bytes()).hexdigest()
skeleton=unreal.load_asset(skeleton_path)
if not skeleton:raise RuntimeError('Production courier skeleton is missing')
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
results={}
for name,definition in json.loads((source/'motion_probe.json').read_text())['clips'].items():
    options=unreal.FbxImportUI();options.automated_import_should_detect_type=False
    options.import_materials=False;options.import_textures=False;options.import_mesh=False
    options.import_as_skeletal=True;options.import_animations=True;options.skeleton=skeleton
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate',True)
    task=unreal.AssetImportTask();task.filename=str(source/'Exports/MotionProbe'/(name+'.fbx'))
    task.destination_path=folder;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True;task.options=options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    clip=unreal.load_asset(folder+'/'+name)
    if not isinstance(clip,unreal.AnimSequence):raise RuntimeError('No animation imported: '+name)
    if clip.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Wrong skeleton: '+name)
    duration=clip.get_play_length()
    if abs(duration-definition['duration_s'])>1/30+.0001:raise RuntimeError('Animation duration drift: '+name)
    results[name]={'asset':clip.get_path_name(),'duration_s':duration,'skeleton':skeleton_path}
if before!=hashlib.sha256(package.read_bytes()).hexdigest():raise RuntimeError('Probe changed production skeleton package')
out=root/'Saved/Validation/CharacterProbe';out.mkdir(parents=True,exist_ok=True)
(out/'import.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
unreal.log('MESH2MOTION PROBE IMPORT COMPLETE '+json.dumps(results))
