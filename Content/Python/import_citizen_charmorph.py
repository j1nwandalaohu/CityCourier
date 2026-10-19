"""Import the NPC as a new mesh/skeleton; reuse retained portrait materials only."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root/'AssetsSource/CityCourier/Ambient/CharMorph'
report = json.loads((source/'manifest.json').read_text())
fbx = source/'SK_CitizenCharMorph.fbx'
assert hashlib.sha256(fbx.read_bytes()).hexdigest() == report['fbx_sha256']
folder = '/Game/CityCourier/Art/Ambient/CharMorph'
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
protected = {p: hashlib.sha256(p.read_bytes()).hexdigest()
    for directory in ('Content/CityCourier/Art/Characters', 'Content/CityCourier/Art/Portrait',
                      'Content/CityCourier/Art/Ambient/Citizen')
    for p in (root/directory).rglob('*.uasset')}
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.import_as_skeletal = True
options.import_mesh = True
options.import_animations = False
options.import_materials = False
options.import_textures = False
options.create_physics_asset = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
existing = unreal.load_asset(folder+'/SK_CitizenCharMorph_Skeleton') if assets.does_asset_exist(folder+'/SK_CitizenCharMorph_Skeleton') else None
options.skeleton = existing
task = unreal.AssetImportTask()
task.filename = str(fbx)
task.destination_path = folder
task.destination_name = 'SK_CitizenCharMorph'
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
task.factory = unreal.FbxFactory()
tools.import_asset_tasks([task])
assert folder+'/SK_CitizenCharMorph.SK_CitizenCharMorph' in list(task.imported_object_paths)
mesh = unreal.load_asset(folder+'/SK_CitizenCharMorph')
assert isinstance(mesh, unreal.SkeletalMesh)
skeleton = mesh.get_editor_property('skeleton')
assert skeleton != unreal.load_asset('/Game/CityCourier/Art/Characters/SK_Courier_Skeleton')
assert skeleton.get_path_name().startswith(folder+'/'), 'Must use an independent NPC skeleton'
slots = list(mesh.get_editor_property('materials'))
for slot in slots:
    name = str(slot.get_editor_property('imported_material_slot_name'))
    material = unreal.load_asset('/Game/CityCourier/Art/Materials/'+name)
    assert material, ('Missing retained portrait/clothing material', name)
    slot.set_editor_property('material_interface', material)
mesh.set_editor_property('materials', slots)
assert not any(name in report['materials'] for name in ('M_Helmet', 'M_ReflectiveStrip'))
assert assets.save_loaded_asset(skeleton, only_if_is_dirty=False)
assert assets.save_loaded_asset(mesh, only_if_is_dirty=False)
assert all(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in protected.items())
report['ue_import'] = {'status': 'passed', 'mesh': mesh.get_path_name(),
    'skeleton': skeleton.get_path_name(), 'independent_from_player': True,
    'protected_existing_packages_unchanged': True, 'material_count': len(slots)}
(source/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
unreal.log('CITIZEN_CHARMORPH_IMPORT_PASSED')
