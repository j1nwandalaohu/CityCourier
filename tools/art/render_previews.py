"""Render actual Blender source geometry for inspection, not concept images."""
import bpy
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/Validation/Art-Source';OUT.mkdir(parents=True,exist_ok=True)
for name,position,target,ortho in [
    ('CourierRider',(2.6,-4.5,2.25),(0,0,.95),2.3),
    ('DeliveryScooter',(2.8,-3.5,1.8),(0,0,.7),2.4),
    ('SM_ShopHouseA',(18,-28,15),(0,-2,8),24)]:
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/'AssetsSource/CityCourier/Blender'/(name+'.blend')))
    if name=='DeliveryScooter':
        with bpy.data.libraries.load(str(ROOT/'AssetsSource/CityCourier/Blender/ScooterWheel.blend'),link=False) as (src,dst):dst.objects=['SM_ScooterWheel']
        wheel=dst.objects[0];bpy.context.collection.objects.link(wheel);wheel.location=(0,-.63,.265)
        other=wheel.copy();bpy.context.collection.objects.link(other);other.location=(0,.62,.265)
    bpy.ops.mesh.primitive_plane_add(size=200)
    floor=bpy.context.object;floor.name='PreviewFloor';m=bpy.data.materials.new('PreviewFloor');m.diffuse_color=(.16,.18,.18,1);floor.data.materials.append(m)
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=20;scene.cycles.use_denoising=True
    scene.world.color=(.28,.28,.28)
    bpy.ops.object.light_add(type='AREA',location=(-3,-4,6));bpy.context.object.data.energy=700;bpy.context.object.data.shape='DISK';bpy.context.object.data.size=5
    bpy.ops.object.light_add(type='SUN',location=(0,0,10));sun=bpy.context.object;sun.rotation_euler=(.55,-.3,-.5);sun.data.energy=2;sun.data.angle=.08
    bpy.ops.object.camera_add(location=position);camera=bpy.context.object;camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=ortho;scene.camera=camera
    scene.render.resolution_x=1000;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.render.filepath=str(OUT/(name+'.png'));scene.frame_set(1);bpy.ops.render.render(write_still=True)
