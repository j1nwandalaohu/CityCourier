"""Create an independently playable art map from the inherited gameplay map.

Only CC_Art_* actors are owned here. Existing map actors remain intact, including
collision, delivery IDs, road graph and manually authored actors.
"""
import math
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Content/Python'))
import polish_materials

ART='/Game/CityCourier/Art'
MAP='/Game/CityCourier/Maps/L_CourierArtPreview'
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools=unreal.AssetToolsHelpers.get_asset_tools()
if not assets.does_asset_exist(MAP):
    if not assets.duplicate_asset('/Game/CityCourier/Maps/L_CourierCity',MAP):raise RuntimeError('Cannot create isolated art map')
if not levels.load_level(MAP):raise RuntimeError('Cannot open art map')
managed={str(t):a for a in actors.get_all_level_actors() for t in a.tags if str(t).startswith('CC_Art_')}
desired=set()

def ensure(cls,tag,pos,rotation=0):
    desired.add(tag)
    a=managed.get(tag)
    if a is None:a=actors.spawn_actor_from_class(cls,unreal.Vector(*pos));managed[tag]=a
    a.tags=[unreal.Name(tag)];a.set_actor_label(tag);a.set_actor_location(unreal.Vector(*pos),False,False)
    a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=rotation,roll=0),False)
    return a

def mesh(tag,path,pos,scale=(1,1,1),yaw=0):
    a=ensure(unreal.StaticMeshActor,tag,pos,yaw);a.set_actor_scale3d(unreal.Vector(*scale))
    a.static_mesh_component.set_static_mesh(unreal.load_asset(ART+'/'+path))
    a.static_mesh_component.set_collision_profile_name('NoCollision')
    return a

polish_materials.ground('M_RoadWorld','asphalt_02',200,(.48,.48,.46),True)
polish_materials.ground('M_PavementWorld','pavement_05',180,(.7,.69,.66),True)

count=0
for actor in list(actors.get_all_level_actors()):
    tags=[str(t) for t in actor.tags]
    block=next((t for t in tags if t.startswith('CC_City_Block_')),None)
    if block:
        # Preserve tested collision on invisible graybox volumes.
        actor.static_mesh_component.set_visibility(False)
        parts=block.split('_');row,col,frontage,number=map(int,parts[-4:])
        center=actor.get_actor_location();size=actor.get_actor_scale3d()*100
        n=max(1,round(size.x/800));yaw=180 if frontage==0 else 0
        for i in range(n):
            x=center.x-size.x/2+size.x*(i+.5)/n
            variant=(row+col+number+i)%3
            mesh(f'CC_Art_Building_{row}_{col}_{frontage}_{number}_{i}','Architecture/SM_ShopHouse'+chr(65+variant),(x,center.y,0),(size.x/n/800,size.y/1150,1),yaw)
            count+=1
    if any(t.startswith('CC_City_Road_') for t in tags):actor.static_mesh_component.set_material(0,unreal.load_asset(ART+'/Materials/M_RoadWorld'))
    if any(t.startswith('CC_City_Walk_') for t in tags):actor.static_mesh_component.set_material(0,unreal.load_asset(ART+'/Materials/M_PavementWorld'))
    if 'CC_BootstrapGround' in tags:actor.static_mesh_component.set_material(0,unreal.load_asset(ART+'/Materials/M_PavementWorld'))
    if isinstance(actor,unreal.DirectionalLight) and 'CC_BootstrapSun' in tags:
        actor.set_actor_rotation(unreal.Rotator(pitch=-12,yaw=-52,roll=0),False)
        actor.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        actor.light_component.set_editor_property('intensity',5.5)
        actor.light_component.set_editor_property('use_temperature',True);actor.light_component.set_editor_property('temperature',4600)
        actor.light_component.set_editor_property('light_source_angle',1.0)
    if isinstance(actor,unreal.SkyLight) and 'CC_BootstrapSkyLight' in tags:
        actor.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        actor.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        actor.light_component.set_editor_property('cubemap',unreal.load_asset(ART+'/Textures/T_EveningPanorama'))
        actor.light_component.set_editor_property('intensity',.55)
        actor.light_component.set_editor_property('real_time_capture',False)

# Dress the 150m street around the rider station; keep the road and door paths clear.
for i,x in enumerate(range(-7500,7501,1500)):
    # Leave the cross street and both north/south shortcut corridors clear.
    if abs(x)<1300 or abs(abs(x)-6000)<650:continue
    for side in [-1,1]:
        y=-12000+side*660
        mesh(f'CC_Art_Lamp_{i}_{side}','StreetProps/SM_StreetLamp',(x,y,14),yaw=0 if side==1 else 180)
        light=ensure(unreal.PointLight,f'CC_Art_LampLight_{i}_{side}',(x,y-side*110,655))
        light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.light_component.set_editor_property('intensity',120)
        light.light_component.set_editor_property('attenuation_radius',850)
        light.light_component.set_light_color(unreal.LinearColor(1.0,.60,.28,1.0))
        light.light_component.set_editor_property('cast_shadows',False)
        tree='SM_PlanterTreeB' if (i+side)%3==0 else 'SM_PlanterTree'
        mesh(f'CC_Art_Tree_{i}_{side}','StreetProps/'+tree,(x+350,y,14),yaw=(i*71)%360)
        mesh(f'CC_Art_Drain_{i}_{side}','StreetProps/SM_DrainGrate',(x-160,-12000+side*570,4.5))
        if i%2:
            mesh(f'CC_Art_Bin_{i}_{side}','StreetProps/SM_WheelieBin',(x-240,y+side*55,14),yaw=180 if side>0 else 0)
for i,x in enumerate([-2200,2500,6700]):
    mesh(f'CC_Art_Manhole_{i}','StreetProps/SM_Manhole',(x,-12030,4.5))
for i in range(6):
    stall=['SM_MarketStall','SM_MarketStallProduce','SM_MarketStallNoodles'][i%3]
    mesh(f'CC_Art_Market_{i}','StreetProps/'+stall,(13300+i*270,12900,14),yaw=180)
    mesh(f'CC_Art_MarketCrates_{i}','StreetProps/SM_CrateStack',(13380+i*270,13030,14),yaw=i*17)
    light=ensure(unreal.PointLight,f'CC_Art_MarketLight_{i}',(13300+i*270,12850,225))
    light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.light_component.set_editor_property('intensity',3)
    light.light_component.set_editor_property('attenuation_radius',350)
    light.light_component.set_light_color(unreal.LinearColor(1,.72,.45,1))
    light.light_component.set_editor_property('cast_shadows',False)
fog=ensure(unreal.ExponentialHeightFog,'CC_Art_AtmosphereHaze',(0,0,0))
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property('fog_density',.002)
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property('fog_height_falloff',.35)
pp=ensure(unreal.PostProcessVolume,'CC_Art_Exposure',(0,0,0));pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_min_brightness',True);settings.set_editor_property('auto_exposure_min_brightness',1.0)
settings.set_editor_property('override_auto_exposure_max_brightness',True);settings.set_editor_property('auto_exposure_max_brightness',1.0)
for key,value in [('bloom_intensity',.18),('vignette_intensity',.12),('film_grain_intensity',.06),('ambient_occlusion_intensity',.65)]:
    settings.set_editor_property('override_'+key,True);settings.set_editor_property(key,value)
settings.set_editor_property('override_color_saturation',True);settings.set_editor_property('color_saturation',unreal.Vector4(.92,.92,.92,1))
pp.set_editor_property('settings',settings)
sky=ensure(unreal.StaticMeshActor,'CC_Art_PhotographicSky',(0,0,0))
sky.set_actor_scale3d(unreal.Vector(16000,16000,16000))
sky.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
sky.static_mesh_component.set_material(0,polish_materials.sky_material())
sky.static_mesh_component.set_collision_profile_name('NoCollision')
sky.static_mesh_component.set_editor_property('cast_shadow',False)
sky.static_mesh_component.set_editor_property('affect_distance_field_lighting',False)
camera=ensure(unreal.CameraActor,'CC_Art_InspectionCamera',(0,-12000,200))
camera.camera_component.set_field_of_view(65)
for tag,actor in managed.items():
    if tag not in desired:actors.destroy_actor(actor)
if not levels.save_current_level():raise RuntimeError('Cannot save art map')
unreal.log(f'CityCourier ART CITY COMPLETE: {count} facade modules; original collision and delivery IDs preserved; {MAP}')
