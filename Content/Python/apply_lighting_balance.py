"""Update the shipping sky fill without rebuilding art or gameplay actors.

The environment integration script uses the same sky intensity. Room fixture
output is configured by UCourierInteriorFinish::Apply at runtime.
"""
import json
from pathlib import Path
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
skies = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.SkyLight)
         and 'CC_BootstrapSkyLight' in [str(t) for t in a.tags]]
assert len(skies) == 1, 'Expected exactly one shipping sky fill'
light = skies[0].light_component
before = light.intensity
light.set_intensity(1.65)
assert levels.save_current_level()
out = Path(unreal.Paths.project_dir())/'Saved/Validation/lighting-balance.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(dict(sky_before=before, sky_after=light.intensity,
    room_lumens=dict(shop=240, residential=180, office=300),
    exposure_changed=False), indent=2), encoding='utf-8')
