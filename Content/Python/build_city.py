"""Author a 600m district prototype, its road graph and persistent location IDs.

Only updates actors carrying our exact generated tag; manual actors are preserved.
All distances below are Unreal centimeters. Art is explicitly a graybox until task10.
"""
import math
import unreal

MAP = "/Game/CityCourier/Maps/L_CourierCity"
DATA = "/Game/CityCourier/Data/DA_CityDefinition"
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
if not levels.load_level(MAP):
    raise RuntimeError("Run bootstrap_project.py before city authoring")
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
tools = unreal.AssetToolsHelpers.get_asset_tools()
managed = {}
for actor in actors.get_all_level_actors():
    for tag in actor.tags:
        tag = str(tag)
        if tag.startswith("CC_City_") or tag.startswith("CC_Bootstrap"):
            if tag in managed:
                raise RuntimeError("Duplicate managed tag: " + tag)
            managed[tag] = actor


def ensure(cls, tag, position, scale=None, yaw=0):
    actor = managed.get(tag)
    if actor is None:
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*position))
        actor.set_editor_property("tags", [unreal.Name(tag)])
        managed[tag] = actor
    if not isinstance(actor, cls):
        raise RuntimeError("Wrong actor class: " + tag)
    actor.set_actor_label(tag)
    actor.set_actor_location(unreal.Vector(*position), False, False)
    actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), False)
    if scale:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def material(name, color):
    path = "/Game/CityCourier/Materials/" + name
    mat = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, "/Game/CityCourier/Materials", unreal.Material, unreal.MaterialFactoryNew())
    # This script owns these generated prototype materials completely.
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    rgb = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -250, 0)
    rgb.set_editor_property("constant", unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 160)
    rough.set_editor_property("r", 0.85)
    unreal.MaterialEditingLibrary.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assets.save_loaded_asset(mat)
    return mat


mats = {"road": material("M_GrayboxRoad", (0.045, 0.055, 0.065)),
        "sidewalk": material("M_GrayboxSidewalk", (0.32, 0.31, 0.29)),
        "building": material("M_GrayboxBuilding", (0.42, 0.37, 0.30)),
        "line": material("M_GrayboxLine", (0.85, 0.57, 0.10)),
        "merchant": material("M_GrayboxMerchant", (0.9, 0.43, 0.06)),
        "destination": material("M_GrayboxDestination", (0.03, 0.40, 0.65)),
        "station": material("M_GrayboxStation", (0.08, 0.65, 0.35)),
        "ground": material("M_GrayboxGround", (0.18, 0.21, 0.16))}


def box(tag, position, size, mat, yaw=0, collision=True):
    actor = ensure(unreal.StaticMeshActor, tag, position, tuple(s / 100 for s in size), yaw)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, mats[mat])
    actor.static_mesh_component.set_collision_profile_name("BlockAll" if collision else "NoCollision")
    return actor


box("CC_BootstrapGround", (0, 0, -100), (60000, 60000, 200), "ground")
coordinates = [-24000, -12000, 0, 12000, 24000]
points = [(x, y, 8) for y in coordinates for x in coordinates]
points += [(x, y, 8) for x in [-6000, 6000] for y in [-12000, 0, 12000]]
index = {(x, y): i for i, (x, y, z) in enumerate(points)}
edges = []
for y in coordinates:
    xs = sorted(x for x, py, z in points if py == y)
    edges.extend((index[(a, y)], index[(b, y)], False) for a, b in zip(xs, xs[1:]))
for x in coordinates + [-6000, 6000]:
    ys = sorted(y for px, y, z in points if px == x)
    edges.extend((index[(x, a)], index[(x, b)], x in [-6000, 6000]) for a, b in zip(ys, ys[1:]))
road_edges = []
for a, b, shortcut in edges:
    p, q = points[a], points[b]
    length = math.dist(p, q)
    yaw = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
    center = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, 2)
    width = 350 if shortcut else 1000
    road = box(f"CC_City_Road_{a}_{b}", center, (length, width, 4), "road", yaw)
    if shortcut:
        road.set_editor_property("tags", [unreal.Name(f"CC_City_Road_{a}_{b}"), unreal.Name("CC_Shortcut")])
    else:
        for side in [-1, 1]:
            offset = 600 * side
            pos = (center[0] - math.sin(math.radians(yaw)) * offset,
                   center[1] + math.cos(math.radians(yaw)) * offset, 7)
            box(f"CC_City_Walk_{a}_{b}_{side}", pos, (length - 1400, 200, 14), "sidewalk", yaw)
        box(f"CC_City_Line_{a}_{b}", (center[0], center[1], 5), (length - 1400, 9, 1), "line", yaw, False)
    road_edges.append(unreal.RoadEdge(from_=a, to=b, length_meters=length / 100, bidirectional=True))

# Perimeter blocks and four neighborhoods, reserving every graph corridor.
xs = [-24000, -12000, -6000, 0, 6000, 12000, 24000]
for row, (y0, y1) in enumerate(zip(coordinates, coordinates[1:])):
    for col, (x0, x1) in enumerate(zip(xs, xs[1:])):
        for frontage in [0, 1]:
            y = y0 + 2100 if frontage == 0 else y1 - 2100
            for number in range(3):
                available = x1 - x0 - 2000
                width = available / 3 - 180
                x = x0 + 1000 + (number + 0.5) * available / 3
                height = 1400 + ((row * 7 + col * 3 + number) % 4) * 350
                box(f"CC_City_Block_{row}_{col}_{frontage}_{number}", (x, y, height / 2),
                    (width, 2300, height), "building")

factory = unreal.DataAssetFactory()
factory.set_editor_property("data_asset_class", unreal.CityDefinition)
city = unreal.load_asset(DATA) if assets.does_asset_exist(DATA) else tools.create_asset(
    "DA_CityDefinition", "/Game/CityCourier/Data", unreal.CityDefinition, factory)
city.set_editor_property("roads", unreal.RoadGraph(
    nodes=[unreal.RoadNode(id=i, position=unreal.Vector(*p)) for i, p in enumerate(points)], edges=road_edges))


def location(identifier, name, x, y, kind):
    # Entry on the inner sidewalk, outside both intersecting traffic lanes.
    definition = unreal.CourierLocationDefinition(location_id=unreal.Name(identifier), display_name=name,
        position=unreal.Vector(x + 800, y + 650, 20), entrance_node=index[(x, y)], kind=kind)
    cls = unreal.RiderStation if kind == unreal.DeliveryLocationKind.STATION else unreal.DeliveryLocation
    actor = ensure(cls, "CC_City_Location_" + identifier, (x + 800, y + 650, 20))
    actor.set_editor_property("location_id", unreal.Name(identifier))
    actor.set_editor_property("location_kind", kind)
    actor.set_editor_property("city_definition", city)
    color = "merchant" if kind == unreal.DeliveryLocationKind.MERCHANT else (
        "station" if kind == unreal.DeliveryLocationKind.STATION else "destination")
    actor.get_editor_property("marker").set_material(0, mats[color])
    return definition


merchants = [location("merchant_noodle", "阿荣面馆", -12000, -12000, unreal.DeliveryLocationKind.MERCHANT),
             location("merchant_rice", "巷口烧腊饭", 12000, -12000, unreal.DeliveryLocationKind.MERCHANT),
             location("merchant_tea", "青禾茶铺", -12000, 12000, unreal.DeliveryLocationKind.MERCHANT),
             location("merchant_market", "邻里小厨", 12000, 12000, unreal.DeliveryLocationKind.MERCHANT)]
recipients = [("home_west", "西苑小区", -24000, -12000), ("home_east", "东里公寓", 24000, -12000),
              ("office_south", "南街写字楼", -12000, -24000), ("office_north", "北岸工作室", 12000, 24000),
              ("home_northwest", "桂花里", -24000, 12000), ("home_northeast", "梧桐苑", 24000, 12000),
              ("shop_center", "中心便利店", 0, 0), ("studio_south", "南风设计社", 12000, -24000)]
destinations = [location(i, name, x, y, unreal.DeliveryLocationKind.DESTINATION) for i, name, x, y in recipients]
station = location("station", "骑手驿站", 0, -12000, unreal.DeliveryLocationKind.STATION)
city.set_editor_property("merchants", merchants)
city.set_editor_property("destinations", destinations)
city.set_editor_property("station", station)
city.set_editor_property("fees", [unreal.OrderFee(merchant_id=m.location_id, destination_id=d.location_id,
    fee_cents=1600 + ((mi * 5 + di * 3) % 9) * 200) for mi, m in enumerate(merchants) for di, d in enumerate(destinations)])
ensure(unreal.PlayerStart, "CC_BootstrapPlayerStart", (800, -11550, 130))
assets.save_loaded_asset(city)
if not levels.save_current_level():
    raise RuntimeError("Failed to save city map")
unreal.log(f"CityCourier graybox saved: {len(points)} road nodes, {len(edges)} road edges, 4 merchants, 8 recipients, 2 shortcuts")
