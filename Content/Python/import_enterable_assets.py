"""Import only the new hollow-building kit; preserve all existing art assets."""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/CityCourier/Art/Buildings'

def kit_material(name):
    """Separate usage-enabled copies keep original art materials untouched."""
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    source = '/Game/CityCourier/Art/Materials/' + name
    target = DEST + '/Materials/' + name
    assert assets.does_asset_exist(source), 'Missing retained material: ' + source
    material = unreal.load_asset(target) if assets.does_asset_exist(target) else assets.duplicate_asset(source, target)
    assert material, 'Material duplication failed: ' + target
    unreal.MaterialEditingLibrary.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    assert material.get_editor_property('used_with_instanced_static_meshes')
    assert assets.save_loaded_asset(material)
    return material


def prepare_materials():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for name in ['M_Plaster', 'M_Concrete', 'M_PaintedMetal', 'M_Wood']:
        kit_material(name)
    for path in assets.list_assets(DEST, recursive=False):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        for index, slot in enumerate(mesh.get_editor_property('static_materials')):
            mesh.set_material(index, kit_material(str(slot.get_editor_property('imported_material_slot_name'))))
        assert assets.save_loaded_asset(mesh)


def import_assets():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name in ['M_Plaster', 'M_Concrete', 'M_PaintedMetal', 'M_Wood']:
        kit_material(name)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    files = sorted((ROOT/'AssetsSource/CityCourier/Exports/Buildings').glob('*.fbx'))
    assert len(files) == 12, 'Incomplete enterable-building kit'
    for path in files:
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_mesh = True
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        options.static_mesh_import_data.generate_lightmap_u_vs = False
        task = unreal.AssetImportTask()
        task.filename = str(path)
        task.destination_path = DEST
        task.destination_name = path.stem
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.options = options
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset(DEST+'/'+path.stem)
        assert isinstance(mesh, unreal.StaticMesh), 'Static mesh import failed: '+path.stem
        for i, slot in enumerate(mesh.get_editor_property('static_materials')):
            name = str(slot.get_editor_property('imported_material_slot_name'))
            mesh.set_material(i, kit_material(name))
        body = mesh.get_editor_property('body_setup')
        assert body, 'Mesh has no physical body setup: '+path.stem
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        if path.stem.startswith('SM_EnterableShopHouse'):
            settings = mesh.get_editor_property('nanite_settings')
            settings.enabled = True
            mesh.set_editor_property('nanite_settings', settings)
        assert assets.save_loaded_asset(mesh)
        unreal.log('ENTERABLE ASSET IMPORTED: ' + path.stem)


if __name__ == '__main__':
    import_assets()
