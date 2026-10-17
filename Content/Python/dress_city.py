"""Dress generated district actors only; preserve untagged user-authored actors."""
import math
import unreal
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/CityCourier/Maps/L_CourierCity')
city=unreal.load_asset('/Game/CityCourier/Data/DA_CityDefinition')
managed={}
for a in actors.get_all_level_actors():
    for t in a.tags:
        tag=str(t)
        if tag.startswith('CC_') and tag not in ['CC_Art_Prop','CC_Art_Sign','CC_Shortcut']:
            assert tag not in managed,'Duplicate generated actor '+tag
            managed[tag]=a

def ensure(cls,tag,p,yaw=0):
    a=managed.get(tag)
    if a is None:
        a=actors.spawn_actor_from_class(cls,unreal.Vector(*p))
        a.set_editor_property('tags',[unreal.Name(tag)])
        managed[tag]=a
    assert isinstance(a,cls),tag
    a.set_actor_label(tag)
    a.set_actor_location(unreal.Vector(*p),False,False)
    a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
    return a

def mesh(tag,name,p,scale=(1,1,1),yaw=0,category='StreetProps',collision=True,kind='Prop'):
    a=ensure(unreal.StaticMeshActor,tag,p,yaw)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/CityCourier/Art/'+category+'/'+name))
    a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_editor_property('tags',[unreal.Name(tag),unreal.Name('CC_Art_'+kind)])
    return a

# The original grid dimensions remain authoritative, including both shortcut corridors.
for tag,a in list(managed.items()):
    if not tag.startswith('CC_City_Block_'):continue
    _,_,_,row,col,frontage,number=tag.split('_')
    row,col,frontage,number=map(int,(row,col,frontage,number))
    # Deterministic original dimensions; never compound scales on repeat runs.
    xs=[-24000,-12000,-6000,0,6000,12000,24000]
    available=xs[col+1]-xs[col]-2000
    width=available/3-180
    height=1400+((row*7+col*3+number)%4)*350
    position=a.get_actor_location()
    a.set_actor_location(unreal.Vector(position.x,position.y,0),False,False)
    a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0 if frontage else 180,roll=0),False)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/CityCourier/Art/Architecture/'+('SM_ShopHouse' if frontage==0 else 'SM_Residential')))
    a.static_mesh_component.set_collision_profile_name('BlockAll')
    a.static_mesh_component.set_material(0,None) # clear the graybox override; imported slots own materials
    a.static_mesh_component.set_editor_property('override_materials',[])
    a.set_actor_scale3d(unreal.Vector(width/830,2300/1340,height/2040))

# Entrance annexes put a recognizable door directly behind each delivery marker.
places=list(city.get_editor_property('merchants'))+list(city.get_editor_property('destinations'))+[city.get_editor_property('station')]
for i,place in enumerate(places):
    p=place.position
    is_shop=i<4 or i==12
    mesh('CC_Art_Entrance_'+str(i),'SM_Shop' if is_shop else 'SM_Gate',(p.x,p.y+(450 if is_shop else 200),0),yaw=180,category='Architecture')
    sign=i if i<4 else 4 if i==12 else 5+(i-4)
    mesh('CC_Art_LocationSign_'+str(i),'SM_Sign_%02d'%sign,(p.x,p.y+175,355 if is_shop else 395),yaw=180,collision=False,kind='Sign')
    mesh('CC_Art_DoorPlanter_'+str(i),'SM_Planter',(p.x-470,p.y+230,0),scale=(.7,.7,.7))
    mesh('CC_Art_DoorBin_'+str(i),'SM_Bin',(p.x+400,p.y+220,0))

# Main commercial strip: 150m, southern frontage near the noodle shop and rider station.
def distance_to_roads(x,y):
    graph=city.get_editor_property('roads')
    nodes={n.id:n.position for n in graph.nodes}
    best=float('inf')
    for edge in graph.edges:
        a,b=nodes[edge.from_],nodes[edge.to]
        dx,dy=b.x-a.x,b.y-a.y
        t=max(0,min(1,((x-a.x)*dx+(y-a.y)*dy)/(dx*dx+dy*dy)))
        best=min(best,math.hypot(x-(a.x+t*dx),y-(a.y+t*dy)))
    return best
for i,x in enumerate(range(-12500,2501,1000)):
    # Fixtures stay beyond sidewalks; intersection/entrance maneuvering space stays clear.
    blocked=any(distance_to_roads(px,py)<760 for px,py in [(x,-11200),(x+250,-11140),(x+500,-11050)])
    blocked=blocked or any(abs(x-p.position.x)<700 and abs(p.position.y+11350)<500 for p in places)
    if blocked:
        for prefix in ['CC_Art_MainLamp_','CC_Art_MainBench_','CC_Art_MainTree_','CC_Art_MainLight_']:
            tag=prefix+str(i)
            if tag in managed:actors.destroy_actor(managed.pop(tag))
        continue
    mesh('CC_Art_MainLamp_'+str(i),'SM_StreetLamp',(x,-11200,0),yaw=180)
    mesh('CC_Art_MainBench_'+str(i),'SM_Bench',(x+250,-11140,0))
    mesh('CC_Art_MainTree_'+str(i),'SM_Planter',(x+500,-11050,0),scale=(1.1,1.1,1.1))
    light=ensure(unreal.PointLight,'CC_Art_MainLight_'+str(i),(x,-11200,625))
    light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.light_component.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    light.light_component.set_editor_property('intensity',600)
    light.light_component.set_editor_property('attenuation_radius',1200)
    light.light_component.set_editor_property('cast_shadows',False)
    light.light_component.set_light_color(unreal.LinearColor(1,.66,.32))

# Additional original shop names along the representative frontage.
for i in range(8):
    x=-11000+i*1600
    mesh('CC_Art_FrontageSign_'+str(i),'SM_Sign_%02d'%(13+i%5),(x,-11035,380),scale=(1.1,1,1.1),yaw=180,collision=False,kind='Sign')

# Small night market beside the northern merchant, outside the through road.
night=places[3].position
for i in range(7):
    x=night.x+900+i*290
    mesh('CC_Art_MarketStall_'+str(i),'SM_Stall',(x,night.y+260,0),yaw=180)
    mesh('CC_Art_MarketBin_'+str(i),'SM_Bin',(x+100,night.y+470,0))
    glow=ensure(unreal.PointLight,'CC_Art_MarketLight_'+str(i),(x,night.y+100,290))
    glow.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    glow.light_component.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    glow.light_component.set_editor_property('intensity',300)
    glow.light_component.set_editor_property('attenuation_radius',600)
    glow.light_component.set_editor_property('cast_shadows',False)
    glow.light_component.set_light_color(unreal.LinearColor(1,.48,.18))
mesh('CC_Art_MarketBanner','SM_Sign_16',(night.x+1700,night.y+350,350),scale=(2,1,1),yaw=180,collision=False,kind='Sign')

# Boundary walls provide a readable street edge beyond the outer ring.
for side in [-1,1]:
    for i in range(-24,25):
        mesh('CC_Art_BoundaryX_%d_%d'%(side,i),'SM_Wall',(side*29000,i*600,0),yaw=90,category='Architecture')
        mesh('CC_Art_BoundaryY_%d_%d'%(side,i),'SM_Wall',(i*600,side*29000,0),category='Architecture')

sun=managed['CC_BootstrapSun']
sun.set_actor_rotation(unreal.Rotator(pitch=-12,yaw=-40,roll=0),False)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity',10.0)
sun.light_component.set_light_color(unreal.LinearColor(1,.72,.43))
sky=managed['CC_BootstrapSkyLight']
sky.light_component.set_editor_property('intensity',1.2)
sky.light_component.set_editor_property('lower_hemisphere_is_black',False)
sky.light_component.set_editor_property('real_time_capture',True)
assert levels.save_current_level()
unreal.log('CityCourier original district dressing saved')
