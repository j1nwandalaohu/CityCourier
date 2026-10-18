"""Encode actual UE pose-sample frames, with captions, using Blender's FFmpeg."""
from pathlib import Path
import bpy
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'docs/art/character-probe';OUT.mkdir(parents=True,exist_ok=True)
for name in ('jog','run'):
    files=sorted((ROOT/'Saved/Validation/CharacterProbe'/(name+'-frames')).glob('frame-*.png'))
    if len(files)!=30:raise RuntimeError('Expected 30 captured UE frames for '+name)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
    scene.render.fps=15;scene.frame_start=1;scene.frame_end=len(files)
    editor=scene.sequence_editor_create()
    sequence=editor.strips.new_image('Actual UE frames',filepath=str(files[0]),channel=1,frame_start=1)
    for file in files[1:]:sequence.elements.append(file.name)
    sequence.frame_final_duration=len(files)
    text=editor.strips.new_effect('Comparison labels',type='TEXT',channel=2,frame_start=1,frame_end=len(files)+1)
    text.text='BASELINE (LEFT)   |   MESH2MOTION (RIGHT)';text.font_size=26;text.location=(.5,.94)
    text.color=(1,1,1,1);text.use_shadow=True
    scene.render.image_settings.file_format='FFMPEG';scene.render.ffmpeg.format='MPEG4';scene.render.ffmpeg.codec='H264'
    scene.render.ffmpeg.constant_rate_factor='HIGH';scene.render.ffmpeg.audio_codec='NONE'
    # Captures already contain UE tone mapping; avoid applying a second filmic transform.
    scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
    scene.render.filepath=str(OUT/(name+'.mp4'));bpy.ops.render.render(animation=True)
    print('ENCODED',scene.render.filepath,flush=True)
