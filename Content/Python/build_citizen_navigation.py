"""Build only owned outdoor navigation bounds after the editor target is compiled.

Run explicitly with Run-EditorScript.ps1 after an authorized Editor build.
Does not invoke the compiler, rebuild city actors, start PIE, or package the game.
"""
import unreal

builder = getattr(unreal.CourierArtBuilder, 'build_citizen_navigation', None)
if builder is None:
    raise RuntimeError('Citizen navigation interface is not compiled. Wait for an explicitly requested Editor build.')
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
for path in ('/Game/CityCourier/Maps/L_CourierCity', '/Game/CityCourier/Maps/L_CourierArtPreview'):
    if not assets.does_asset_exist(path):
        continue
    if not levels.load_level(path):
        raise RuntimeError('Cannot load '+path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not builder(world):
        raise RuntimeError('Cannot build citizen navigation for '+path)
    # Native builder finishes all navigation tile jobs before returning.
    if not levels.save_current_level():
        raise RuntimeError('Cannot save '+path)
unreal.log('CITIZEN_NAVIGATION_GENERATION_COMPLETE')
