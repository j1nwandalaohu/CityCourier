"""Fail if the delivered exterior art remains exclusive to the preview map."""
import json
from pathlib import Path
import unreal

REQUIRED_MESHES = {
    'SM_ReferenceTree', 'SM_ReferenceCrate', 'SM_ReferenceAirconRusted',
    'SM_MarketStall', 'SM_MarketStallProduce', 'SM_MarketStallNoodles',
    'SM_DrainGrate', 'SM_Manhole', 'SM_WheelieBin',
}


def validate():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
    meshes, materials = set(), set()
    buildings = 0
    for actor in actors.get_all_level_actors():
        buildings += isinstance(actor, unreal.CourierBuildingActor)
        if actor.get_editor_property('hidden'):
            continue
        for c in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not c.is_visible() or not c.static_mesh:
                continue
            meshes.add(c.static_mesh.get_name())
            materials.update(m.get_name() for m in c.get_materials() if m)
    assert REQUIRED_MESHES <= meshes, 'Missing visible street art: ' + str(sorted(REQUIRED_MESHES-meshes))
    assert {'M_RoadWorld', 'M_PavementWorld', 'M_EveningPanorama'} <= materials, 'Missing street materials/sky'
    assert not {'M_GrayboxRoad', 'M_GrayboxSidewalk', 'M_GrayboxGround'} & materials
    assert buildings == 293, 'Canonical building coverage changed'
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
    dependencies = {str(p).rsplit('/', 1)[-1] for p in registry.get_dependencies(
        '/Game/CityCourier/Art/Interiors/DA_InteriorFinish', options)}
    manifest = json.loads((Path(unreal.Paths.project_dir())/'AssetsSource/CityCourier/InteriorArt/asset-manifest.json').read_text(encoding='utf-8'))
    expected = {a['name'] for a in manifest['assets']}
    assert expected <= dependencies, 'Unbound interior kit meshes: ' + str(sorted(expected-dependencies))
    unreal.log('ENVIRONMENT INTEGRATION PASS: all nine street mesh types, road/pavement/sky, 293 buildings')


if __name__ == '__main__':
    validate()
