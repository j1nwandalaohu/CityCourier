"""Capture production poses using Enhanced Input, without replacing runtime clips."""
import json,time
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/ProductionActions';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
camera=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.CameraActor,unreal.Vector(1080,-11860,190))
camera.tags=[unreal.Name('ProductionActionCamera')]
levels.editor_request_begin_play()
start=time.monotonic();stage=0;busy=False;shots=[];sent=False;captured=False
def finish(error=None):
    (out/'results.json').write_text(json.dumps({'error':error,'shots':shots,'input':'Enhanced Input injection through production bindings'},indent=2))
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play();unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def capture(name):
    shots.append({'name':name,'action':str(anim.get_editor_property('current_action')),'action_elapsed':anim.get_editor_property('action_elapsed'),'speed':anim.get_editor_property('ground_speed'),'clips':[a.get_name() for a in anim.get_editor_property('clips')],
                  'fists':{s:str(rider.mesh.get_socket_location('PunchTrace_'+s)) for s in ['L','R']}})
    unreal.SystemLibrary.execute_console_command(world,'Shot filename='+str(out/(name+'.png'))+' nosuffix')
def inject(name,value=unreal.Vector(1,0,0)):
    inputs.inject_input_vector_for_action(actions[name],value,[],[])
def impl(delta):
    global world,pc,rider,anim,inputs,actions,stage,ready,sent,captured,camera
    if time.monotonic()-start>150:raise RuntimeError('Production action capture timeout')
    world=editor.get_game_world()
    if not world:return
    if stage==0:
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        if not pc or not pc.get_controlled_pawn():return
        rider=pc.get_controlled_pawn();anim=rider.mesh.get_anim_instance()
        candidates=[s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__')]
        assert len(candidates)==1, 'Expected one PIE input subsystem: '+str([s.get_path_name() for s in candidates])
        inputs=candidates[0]
        assert inputs
        actions={n:unreal.load_asset('/Game/CityCourier/Input/IA_'+n) for n in ['Move','Sprint','LightPunch','HeavyPunch']}
        camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if 'ProductionActionCamera' in [str(t) for t in a.tags])
        pc.set_control_rotation(unreal.Rotator());pc.set_view_target_with_blend(camera,0)
        # Use an unobstructed road fixture, away from the parked spawn scooter.
        rider.set_actor_location(unreal.Vector(-4000,-12000,110),False,True)
        for cmd in ['t.MaxFPS 60','r.ScreenPercentage 100','r.Streaming.PoolSize 1600']:unreal.SystemLibrary.execute_console_command(world,cmd)
        ready=time.monotonic()+5;stage=1
    pos=rider.get_actor_location();target=pos+unreal.Vector(0,0,25)
    camera.set_actor_location(pos+unreal.Vector(300,-330,110),False,True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),target),False)
    now=time.monotonic()
    if stage==1 and now>ready:ready=now+1;stage=2
    elif stage==2:
        inject('Move',unreal.Vector(0,1,0))
        if now>ready:
            assert 390<anim.get_editor_property('ground_speed')<450, 'Jog speed: '+str(anim.get_editor_property('ground_speed'))+' position: '+str(rider.get_actor_location())
            capture('jog');ready=now+1;stage=3
    elif stage==3:
        inject('Move',unreal.Vector(0,1,0));inject('Sprint')
        if now>ready:
            assert anim.get_editor_property('ground_speed')>650, 'Sprint speed: '+str(anim.get_editor_property('ground_speed'))
            capture('sprint');ready=now+1;stage=4
    elif stage==4 and now>ready:
        inject('LightPunch');ready=now+3;stage=5
    elif stage==5:
        elapsed=anim.get_editor_property('action_elapsed');action=anim.get_editor_property('current_action').name.replace('_','').lower()
        # Queue the second click before GPU screenshot readback can stall a frame.
        if action=='lightl' and elapsed>=.18 and not captured:
            inject('LightPunch');sent=True;capture('light-left');captured=True
        if action=='lightr' and elapsed>=.16:capture('light-right');ready=now+1;stage=6
        if now>ready:raise RuntimeError('Production light-punch input/chain did not play')
    elif stage==6 and now>ready:inject('HeavyPunch');ready=now+3;stage=7
    elif stage==7:
        action=anim.get_editor_property('current_action').name.replace('_','').lower()
        if action=='heavy' and anim.get_editor_property('action_elapsed')>=.38:capture('heavy');ready=now+1;stage=8
        if now>ready:raise RuntimeError('Production heavy-punch input did not play')
    elif stage==8 and now>ready:
        assert len(shots)==5
        finish()
def tick(delta):
    global busy
    if busy:return
    busy=True
    try:impl(delta)
    except Exception as error:unreal.log_error(type(error).__name__+': '+str(error));finish(type(error).__name__+': '+str(error))
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)
