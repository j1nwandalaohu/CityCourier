"""Actual PIE A/B poses and sampled motion frames under fixed lab lighting."""
import json,time,os
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/CharacterProbe';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.load_level('/Game/CityCourier/Maps/L_CharacterProbe');levels.editor_request_begin_play()
start=time.monotonic();stage=0;index=0;task=None;busy=False;results=[];traffic_managers=[]
validation=json.loads((root/'AssetsSource/CityCourier/motion_probe_validation.json').read_text())
jobs=[{'name':'idle-comparison','base':'A_Idle','new':'A_M2M_Idle','time':.7},
      {'name':'walk-comparison','base':'A_Walk','new':'A_M2M_Walk','time':.4},
      {'name':'jog-comparison','base':'A_Walk','new':'A_M2M_Jog','time':.25},
      {'name':'run-comparison','base':'A_Run','new':'A_M2M_Run','time':.19}]
for lighting in ('neutral','dusk'):
    for person in ('MakeHuman','CharMorph'):jobs.append({'name':person.lower()+'-'+lighting,'person':person,'lighting':lighting})
for mode,base,new,speed in [('jog','A_Walk','A_M2M_Jog',420),('run','A_Run','A_M2M_Run',700)]:
    for frame in range(30):jobs.append({'name':f'{mode}-frames/frame-{frame:03}','base':base,'new':new,'time':frame/15,'speed':speed,'video':True})
if os.environ.get('CC_PROBE_STILLS_ONLY')=='1':jobs=jobs[:8]

def finish(error=None):
    (out/'results.json').write_text(json.dumps({'shots':results,'error':error,'render':'Epic 1280x720, actual PIE pose samples','motion_frames_fps':15,'order':'baseline left, retarget right','ambient_managers_disabled':len(traffic_managers)},indent=2)+'\n',encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play();unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick_impl(delta):
    global stage,index,task,ready,world,pc,parts,camera,active_job,light_positions,traffic_managers
    try:
        if time.monotonic()-start>850:raise RuntimeError('Character probe capture timeout')
        world=editor.get_game_world()
        if not world:return
        if stage==0:
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():return
            # The merged gameplay mode starts ambient traffic in every map.
            # Disable only this PIE world's population for a controlled lab.
            manager_class=getattr(unreal,'AmbientTrafficManager',None)
            traffic_managers=list(unreal.GameplayStatics.get_all_actors_of_class(world,manager_class)) if manager_class else []
            for manager in traffic_managers:manager.set_actor_tick_enabled(False)
            vehicle_class=getattr(unreal,'AmbientVehicle',None)
            if vehicle_class:
                for vehicle in unreal.GameplayStatics.get_all_actors_of_class(world,vehicle_class):vehicle.set_population_active(False)
            pawn=pc.get_controlled_pawn();pawn.disable_input(pc);pawn.set_actor_hidden_in_game(True)
            pawn.set_actor_enable_collision(False);pawn.set_actor_location(unreal.Vector(2000,2000,150),False,True)
            pc.set_ignore_look_input(True);pc.set_ignore_move_input(True)
            parts={str(tag).removeprefix('CC_Probe_'):a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) for tag in a.tags if str(tag).startswith('CC_Probe_')}
            camera=parts['Camera'];pc.set_view_target_with_blend(camera,0)
            light_positions={tag:parts[tag].get_actor_location() for tag in ('Key','Fill','Rim')}
            for command in ['t.MaxFPS 30','ShowFlag.Game 1','ShowFlag.HUD 0','r.ScreenPercentage 100','r.Streaming.PoolSize 2200']+[f'sg.{name}Quality 3' for name in ['ViewDistance','Shadow','GlobalIllumination','Reflection','Texture','Effects','Foliage','PostProcess']]:
                unreal.SystemLibrary.execute_console_command(world,command)
            stage=1
        elif stage==1:
            job=jobs[index];active_job=job
            portrait='person' in job
            offset=0 if portrait else -job.get('speed',0)*job['time']
            for tag in ('Before','After'):parts[tag].set_actor_hidden_in_game(portrait)
            for tag in ('MakeHuman','CharMorph'):parts[tag].set_actor_hidden_in_game(not portrait or tag!=job.get('person'))
            dusk=job.get('lighting')=='dusk'
            for tag,intensity in [('Key',12),('Fill',4),('Rim',10)]:
                c=parts[tag].light_component;c.set_editor_property('intensity',intensity*(.65 if dusk and tag=='Fill' else 1))
                c.set_editor_property('use_temperature',True);c.set_editor_property('temperature',4400 if dusk and tag=='Key' else 6500)
                p=light_positions[tag];parts[tag].set_actor_location(unreal.Vector(p.x,p.y+offset,p.z),False,True)
            if portrait:
                pos=unreal.Vector(90,-220,163);target=unreal.Vector(0,0,157);fov=23
            else:
                pos=unreal.Vector(0,-450,175);target=unreal.Vector(0,0,100);fov=48
                seconds=job['time'];speed=job.get('speed',0);offset=-speed*seconds
                pos.y+=offset;target.y+=offset
                # Camera looks along +Y; its screen-right points toward -X.
                for tag,name,x in [('Before',job['base'],65),('After',job['new'],-65)]:
                    c=parts[tag].get_component_by_class(unreal.SkeletalMeshComponent)
                    path='/Game/CityCourier/Art/'+('Characters/' if tag=='Before' else 'Experiments/Mesh2Motion/')+name
                    animation=unreal.load_asset(path)
                    if not animation:raise RuntimeError('Missing probe clip '+path)
                    c.play_animation(animation,True);c.set_play_rate(0)
                    natural=validation[name]['estimated_natural_speed_cm_s']
                    rate=speed/natural if tag=='After' and speed and natural>1 else 1
                    position=seconds*rate%animation.get_play_length();c.set_position(position,False)
                    parts[tag].set_actor_location(unreal.Vector(x,offset,2 if tag=='Before' else 0),False,True)
                    if tag=='After':job['matched_play_rate']=rate
            camera.camera_component.set_field_of_view(fov);camera.set_actor_location(pos,False,True)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,target),False)
            ready=time.monotonic()+(.15 if job.get('video') else 2.5);stage=2
        elif stage==2 and time.monotonic()>ready:
            file=out/(active_job['name']+'.png');file.parent.mkdir(parents=True,exist_ok=True)
            task=unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(file));stage=3
        elif stage==3 and task.is_task_done():
            if any(m.get_active_vehicle_count() or m.get_active_pedestrian_count() for m in traffic_managers):
                raise RuntimeError('Ambient population entered the character lab')
            if 'new' in active_job:
                after=parts['After'];c=after.get_component_by_class(unreal.SkeletalMeshComponent)
                error=(c.get_socket_location('root')-after.get_actor_location()).length()
                if error>.1:raise RuntimeError('UE probe root drift '+str(error)+'cm')
                active_job['ue_root_error_cm']=error
                active_job['ue_position_s']=c.get_position()
                active_job['ue_anim_instance']=c.get_anim_instance().get_class().get_name()
                active_job['ue_head_world']=[getattr(c.get_socket_location('head'),a) for a in ('x','y','z')]
                active_job['ue_left_foot_world']=[getattr(c.get_socket_location('foot_l'),a) for a in ('x','y','z')]
                sequence=unreal.load_asset('/Game/CityCourier/Art/Experiments/Mesh2Motion/'+active_job['new'])
                options=unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED,optional_skeletal_mesh=c.get_skeletal_mesh_asset())
                pose=sequence.get_anim_pose_at_time(c.get_position(),options)
                pose_error=max((c.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation-pose.get_bone_pose(bone,unreal.AnimPoseSpaces.WORLD).translation).length() for bone in ('head','hand_l','hand_r','foot_l','foot_r'))
                if pose_error>.1:raise RuntimeError('Rendered UE pose disagrees with animation by '+str(pose_error)+'cm')
                active_job['ue_pose_error_cm']=pose_error
            results.append(active_job);index+=1
            if index==len(jobs):finish()
            else:stage=1
    except Exception as error:
        unreal.log_error(str(error));finish(str(error))

def tick(delta):
    global busy
    if busy:return
    busy=True
    try:tick_impl(delta)
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)
