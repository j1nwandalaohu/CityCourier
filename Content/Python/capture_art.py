"""Capture actual PIE frames of the art map using isolated test saves."""
import json
import time
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
out=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Art-PIE';out.mkdir(parents=True,exist_ok=True)
levels.load_level('/Game/CityCourier/Maps/L_CourierArtPreview')
if hasattr(unreal,'EditorPerformanceSettings'):
    unreal.get_default_object(unreal.EditorPerformanceSettings).set_editor_property('throttle_cpu_when_not_foreground',False)
levels.editor_request_begin_play()
start=time.monotonic();stage=0;step=0;task=None;results=[];checks={}
shots=[('street',(-2700,-12200,210),(-300,-11100,650)),
       ('shop',(-4750,-12100,165),(-4333,-10900,205)),
       ('rider',(1050,-11830,145),(800,-11550,110)),
       ('portrait',(925,-11700,170),(800,-11550,158)),
       ('riding',(1220,-11900,170),(950,-11550,105)),('market',(12600,12100,240),(13800,13300,450))]

def finish(error=None):
    (out/'results.json').write_text(json.dumps(dict(results=results,checks=checks,error=error),indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick_impl(delta):
    global stage,step,task,ready,pc,rider,bike,camera,drive_start,walk_start
    try:
        if time.monotonic()-start>720:raise RuntimeError('Art PIE capture timeout')
        world=editor.get_game_world()
        if not world:return
        if stage==0:
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():return
            rider=pc.get_controlled_pawn();bike=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.ScooterPawn)[0]
            rider.disable_input(pc);pc.set_ignore_look_input(True);pc.set_ignore_move_input(True)
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 30')
            unreal.SystemLibrary.execute_console_command(world,'r.ScreenPercentage 100')
            unreal.SystemLibrary.execute_console_command(world,'ShowFlag.Game 1')
            for command in ['sg.ViewDistanceQuality 3','sg.ShadowQuality 3','sg.GlobalIlluminationQuality 3','sg.ReflectionQuality 3','sg.TextureQuality 3','sg.EffectsQuality 3','sg.FoliageQuality 3','sg.PostProcessQuality 3','r.Streaming.PoolSize 2000']:
                unreal.SystemLibrary.execute_console_command(world,command)
            # Use a pre-authored camera; editor actor spawning cannot target PIE.
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if 'CC_Art_InspectionCamera' in [str(t) for t in a.tags])
            camera.camera_component.set_field_of_view(65)
            pc.set_view_target_with_blend(camera,0)
            stage=1
        elif stage==1:
            name,pos,target=shots[step]
            camera.camera_component.set_field_of_view(42 if name=='portrait' else 65)
            lens=camera.camera_component.get_editor_property('post_process_settings')
            lens.set_editor_property('override_depth_of_field_focal_distance',True)
            lens.set_editor_property('depth_of_field_focal_distance',(unreal.Vector(*pos)-unreal.Vector(*target)).length())
            lens.set_editor_property('override_depth_of_field_fstop',True)
            lens.set_editor_property('depth_of_field_fstop',4.0 if name=='portrait' else 16.0)
            camera.camera_component.set_editor_property('post_process_settings',lens)
            camera.camera_component.set_editor_property('post_process_blend_weight',1.0)
            if name=='riding':
                if not bike.try_mount(rider):raise RuntimeError('Art rider could not mount original gameplay scooter')
                bike.disable_input(pc);bike.set_drive_input(0,0,True);pc.set_view_target_with_blend(camera,0)
            camera.set_actor_location(unreal.Vector(*pos),False,True)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*pos),unreal.Vector(*target)),False)
            ready=time.monotonic()+6;stage=2
        elif stage==2 and time.monotonic()>ready:
            task=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(shots[step][0]+'.png')))
            stage=3
        elif stage==3 and task.is_task_done():
            def xyz(v):return [v.x,v.y,v.z]
            bones={name:xyz(rider.mesh.get_socket_location(name)) for name in ['pelvis','hand_l','hand_r','foot_l','foot_r']}
            if shots[step][0]=='riding':
                nodes={c.get_name():c for c in bike.get_components_by_class(unreal.SceneComponent)}
                for bone,anchor,tolerance in [('pelvis','RiderSeat',10),('hand_l','LeftGrip',12),('hand_r','RightGrip',12)]:
                    target=nodes[anchor].get_world_location()
                    if bone=='pelvis':target+=unreal.Vector(0,0,10) # Hip joint is above the seated skin surface.
                    error=(rider.mesh.get_socket_location(bone)-target).length()
                    checks[bone+'_anchor_error_cm']=error
                    if error>tolerance:raise RuntimeError(f'{bone} misses {anchor} by {error:.1f}cm')
            results.append(dict(shot=shots[step][0],rider_class=rider.get_class().get_name(),bike_class=bike.get_class().get_name(),bones=bones,bike=xyz(bike.get_actor_location())))
            step+=1
            if step==len(shots):
                drive_start=bike.get_actor_location();bike.set_drive_input(1,0,False);ready=time.monotonic()+2;stage=4
            else:stage=1
        elif stage==4 and time.monotonic()>ready:
            checks['ride_distance_cm']=(bike.get_actor_location()-drive_start).length()
            if checks['ride_distance_cm']<100:raise RuntimeError('Art scooter did not move')
            bike.set_drive_input(0,0,True);ready=time.monotonic()+2;stage=5
        elif stage==5 and time.monotonic()>ready:
            checks['dismounted']=bike.try_dismount()
            if not checks['dismounted']:raise RuntimeError('Art rider could not dismount')
            rider.disable_input(pc);walk_start=rider.get_actor_location();ready=time.monotonic()+1.5;stage=6
        elif stage==6:
            rider.add_movement_input(unreal.Vector(1,0,0),1,True)
            if time.monotonic()>ready:
                checks['walk_distance_cm']=(rider.get_actor_location()-walk_start).length()
                checks['walk_animation_speed']=rider.mesh.get_anim_instance().get_editor_property('ground_speed')
                if checks['walk_distance_cm']<100 or checks['walk_animation_speed']<8:raise RuntimeError('Walking animation did not follow gameplay movement')
                finish()
    except Exception as error:
        unreal.log_error(str(error));finish(str(error))
busy=False
def tick(delta):
    global busy
    if busy:return
    busy=True
    try:tick_impl(delta)
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)
