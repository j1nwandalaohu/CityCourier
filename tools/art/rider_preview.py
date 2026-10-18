"""Rebuild a standalone rider inspection scene without overwriting production exports."""
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_rider
import art_common
from art_common import ROOT,material,cylinder

OUT=ROOT/'Saved/PolishRider';OUT.mkdir(parents=True,exist_ok=True)
if '--export' in sys.argv:art_common.EXPORT=OUT/'Exports'
else:build_rider.export=lambda *args:None
build_rider.save=lambda *args:None
build_rider.build()
scene=bpy.context.scene;rig=bpy.data.objects['CourierRig'];mesh=bpy.data.objects['SK_Courier']
scene.render.engine='CYCLES';scene.cycles.samples=32
scene.render.resolution_x=1100;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.world.color=(.17,.17,.17)
scene.view_settings.view_transform='AgX'
for name,loc,power,size in [('Key',(-3,-4,5),500,4),('Fill',(3,-2,2),180,3),('Rim',(0,3,4),600,3)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=loc
    obj.rotation_euler=(Vector((0,0,1))-obj.location).to_track_quat('-Z','Y').to_euler()
camdata=bpy.data.cameras.new('InspectionCamera');cam=bpy.data.objects.new('InspectionCamera',camdata)
scene.collection.objects.link(cam);scene.camera=cam;camdata.type='ORTHO'
def view(name,action,loc,target,scale):
    rig.animation_data.action=bpy.data.actions[action];scene.frame_set(1)
    cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();camdata.ortho_scale=scale
    scene.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
view('idle','A_Idle',(2,-4,2.4),(0,0,.94),2.0)
view('face','A_Idle',(1,-3,1.9),(0,-.04,1.63),.42)
view('hands','A_Ride',(1,-1.5,1.3),(0,-.46,.86),.9)
view('ride','A_Ride',(2,-3,2),(0,-.15,.74),1.65)
report={'vertices':len(mesh.data.vertices),'polygons':len(mesh.data.polygons),'bones':len(rig.data.bones),
        'clips':[a.name for a in bpy.data.actions],
        'ride_wrists':{s:list(rig.pose.bones['hand_'+s].matrix.translation) for s in ['l','r']},
        'ride_ankles':{s:list(rig.pose.bones['foot_'+s].matrix.translation) for s in ['l','r']}}
(OUT/'inspection.json').write_text(json.dumps(report,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'rider-inspection.blend'))
print(json.dumps(report),flush=True)
