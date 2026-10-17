import json,pathlib,time,unreal
out=pathlib.Path(unreal.Paths.project_dir())/'Saved/Validation/Task11-Audio';out.mkdir(parents=True,exist_ok=True)
editor=unreal.new_object(unreal.UnrealEditorSubsystem);ready=time.monotonic()+8;stage=0;results=[]
def finish(error=None):
    (out/'audio-results.json').write_text(json.dumps({'stages':results,'error':error},indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.execute_console_command(editor.get_game_world(),'quit')
def tick(delta):
    global stage,ready,world,audio,start
    if time.monotonic()<ready:return
    try:
        world=editor.get_game_world()
        if stage==0:
            mode=unreal.GameplayStatics.get_game_mode(world)
            audio=mode.get_component_by_class(unreal.CourierAudioComponent);assert audio
            audio.set_component_tick_enabled(False)
            unreal.AudioMixerLibrary.start_recording_output(world,24,None)
            start=time.monotonic();audio.set_vehicle_speed(0)
        elif stage in [1,5]:audio.set_vehicle_speed(36)
        elif stage==2:unreal.GameplayStatics.set_game_paused(world,True)
        elif stage==3:unreal.GameplayStatics.set_game_paused(world,False)
        elif stage==4:audio.set_vehicle_speed(0)
        elif stage==6:
            unreal.AudioMixerLibrary.stop_recording_output(world,unreal.AudioRecordingExportType.WAV_FILE,'runtime-audio',str(out),None,None)
            ready=time.monotonic()+2
        else:finish();return
        results.append({'stage':stage,'seconds':time.monotonic()-start})
        stage+=1;ready=time.monotonic()+3
    except Exception as error:finish(str(error))
handle=unreal.register_slate_post_tick_callback(tick)
