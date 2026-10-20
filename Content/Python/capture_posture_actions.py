"""Exercise production Enhanced Input in PIE; capture real native posture playback."""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/PostureActions'
out.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
started = time.monotonic()
stage, busy, closing, failure = 'startup', False, False, None
rows = []


def cmd(text):
    unreal.SystemLibrary.execute_console_command(world, text)


def finish(error=None):
    global closing, failure
    closing, failure = True, error
    for name in ('Prone', 'Crouch', 'Move'):
        cmd('Input.-action IA_'+name)
    levels.editor_request_end_play()


def shot(name):
    cmd('Shot filename='+str(out/(name+'.png'))+' nosuffix')


def impl(delta):
    global world, rider, pc, camera, inputs, move, stage, ready, origin
    if time.monotonic()-started > 150:
        raise RuntimeError('Posture PIE capture timed out')
    world = editor.get_game_world()
    if not world:
        return
    now = unreal.GameplayStatics.get_time_seconds(world)
    if stage == 'startup':
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        rider = pc.get_controlled_pawn()
        assert isinstance(rider, unreal.CourierCharacter)
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
        parts = {str(t).removeprefix('CC_Probe_'): a for a in actors for t in a.tags if str(t).startswith('CC_Probe_')}
        camera = parts['Camera']
        for actor in actors:
            if actor.get_components_by_class(unreal.MeshComponent) and actor not in (rider, parts['Floor']):
                actor.set_actor_hidden_in_game(True)
            if hasattr(actor, 'set_population_active'):
                actor.set_population_active(False)
        rider.set_actor_hidden_in_game(False)
        rider.set_actor_location(unreal.Vector(0, 0, 90), False, True)
        rider.set_actor_rotation(unreal.Rotator(yaw=270), False)
        rider.character_movement.stop_movement_immediately()
        rider.mesh.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        inputs = next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
        move = unreal.load_asset('/Game/CityCourier/Input/IA_Move')
        pc.set_control_rotation(unreal.Rotator(yaw=270))
        pc.set_view_target_with_blend(camera, 0)
        for text in ('t.MaxFPS 60', 'r.ScreenPercentage 100', 'r.MotionBlurQuality 0'):
            cmd(text)
        stage, ready = 'warmup', now+4
    pos = rider.get_actor_location()
    feet = pos.z-rider.capsule_component.get_scaled_capsule_half_height()
    target = unreal.Vector(pos.x, pos.y, feet+30)
    camera.set_actor_location(target+unreal.Vector(290, -180, 140), False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), target), False)
    if stage == 'warmup' and now >= ready:
        cmd('Input.+action IA_Prone true')
        stage, ready = 'prone-idle', now+1
    elif stage == 'prone-idle' and now >= ready:
        assert rider.is_prone(), 'Prone input did not enter prone'
        assert 5 < rider.mesh.get_socket_location('head').z-feet < 60, 'Rendered head is upright or below the floor'
        shot('prone-idle')
        origin = pos
        stage, ready = 'crawl', now+2
    elif stage == 'crawl':
        inputs.inject_input_vector_for_action(move, unreal.Vector(0, 1, 0), [], [])
        anim = rider.mesh.get_anim_instance()
        rows.append({'time': now, 'position': [pos.x, pos.y, pos.z], 'speed': rider.get_velocity().length2d(),
                     'prone': rider.is_prone(), 'animation_index': anim.get_editor_property('animation_index'),
                     'head_above_floor_cm': rider.mesh.get_socket_location('head').z-feet,
                     'mesh_relative_z': rider.mesh.get_editor_property('relative_location').z})
        if now >= ready:
            assert (pos-origin).length2d() > 50, 'Prone input did not move the rider'
            assert 5 < rider.get_velocity().length2d() <= 65, 'Crawl speed is outside expected range'
            assert anim.get_editor_property('animation_index') == 7, 'Native crawl clip was not selected'
            assert all(5 < r['head_above_floor_cm'] < 60 for r in rows), 'Crawling pose entered the floor'
            shot('prone-crawl')
            cmd('Input.-action IA_Prone')
            stage, ready = 'stand', now+1
    elif stage == 'stand' and now >= ready:
        assert not rider.is_prone(), 'Releasing prone failed to stand in clear space'
        assert rider.mesh.get_socket_location('head').z-feet > 130, 'Standing pose was not restored'
        shot('standing')
        cmd('Input.+action IA_Crouch true')
        stage, ready = 'crouch', now+1
    elif stage == 'crouch' and now >= ready:
        assert 65 < rider.mesh.get_socket_location('head').z-feet < 115, 'Crouch animation is upright or below the floor'
        shot('crouch')
        finish()


def tick(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        if closing:
            if not editor.get_game_world():
                (out/'results.json').write_text(json.dumps({'complete': failure is None, 'error': failure,
                    'input': 'Production Enhanced Input', 'single_node_animation': False, 'map_saved': False,
                    'frames': rows}, indent=2)+'\n', encoding='utf-8')
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        else:
            impl(delta)
    except Exception:
        error = traceback.format_exc()
        unreal.log_error(error)
        finish(error)
    finally:
        busy = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/CityCourier/Maps/L_CharacterProbe')
handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
