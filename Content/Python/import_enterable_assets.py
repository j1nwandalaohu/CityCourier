"""Import only the new hollow-building kit; preserve all existing art assets."""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/CityCourier/Art/Buildings'


def import_assets():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
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
            material_path = '/Game/CityCourier/Art/Materials/' + name
            assert assets.does_asset_exist(material_path), f'Missing retained project material {name} on {path.stem}'
            mesh.set_material(i, unreal.load_asset(material_path))
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
