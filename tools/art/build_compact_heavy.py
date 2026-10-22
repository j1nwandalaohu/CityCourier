"""Reuse the retained library cross, retiming keys without authoring new poses."""
import hashlib
import json
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_common import SOURCE, export

bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierCombat.blend'))
rig = bpy.data.objects['CourierRig']
source = bpy.data.actions['A_LightPunch_R']
old = bpy.data.actions.get('A_HeavyPunch_R')
if old:
    old.name = 'Reference_Melee_Hook'
action = source.copy()
action.name = 'A_HeavyPunch_R'
action.use_fake_user = True
first = float(source.frame_range[0])
fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
assert abs(fps-60) < .01

def retime(frame):
    t = (frame-first)/fps
    return 1+fps*(t*.40/.18 if t <= .18 else .40+(t-.18)*(.95-.40)/(.55-.18))

for curve in action.fcurves:
    for key in curve.keyframe_points:
        key.co.x = retime(key.co.x)
        key.interpolation = 'LINEAR'
    curve.update()
rig.animation_data.action = action
bpy.context.scene.frame_start = 1
bpy.context.scene.frame_end = 58
bpy.context.scene.frame_set(1)
export('A_HeavyPunch_R', [rig], 'ProductionActions', True)
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Blender/CourierCompactHeavy.blend'))
fbx = SOURCE/'Exports/ProductionActions/A_HeavyPunch_R.fbx'
report = {
    'source_library': 'Mesh2Motion', 'source_action': 'Punch_Cross',
    'license': 'CC0-1.0', 'source_clip': 'A_LightPunch_R',
    'pose_authoring': 'unchanged library poses; piecewise time retiming only',
    'duration_s': .95, 'contact_seconds': .40,
    'fbx_sha256': hashlib.sha256(fbx.read_bytes()).hexdigest(),
    'runtime_import': 'pending',
}
(SOURCE/'compact_heavy.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
production_path = SOURCE/'production_actions.json'
production = json.loads(production_path.read_text(encoding='utf-8'))
production['clips']['A_HeavyPunch_R'] = {
    **production['clips']['A_LightPunch_R'], 'duration_s': .95, 'samples': 58,
    'source_clip': 'A_LightPunch_R', 'contact_seconds': .40,
}
production['contact_alignment']['A_HeavyPunch_R'] = {
    **production['contact_alignment']['A_LightPunch_R'], 'duration': .95,
    'contact': .40, 'samples': 58,
}
production_path.write_text(json.dumps(production, indent=2)+'\n', encoding='utf-8')
print('COMPACT_HEAVY_EXPORTED '+json.dumps(report), flush=True)
