"""Deterministic, centimeter-based rooms, doors and real stair geometry.

Local front is -Y; all positions are foot/floor positions. A single rigid world
transform is applied by the UE compiler, never a second per-room scale.
"""
from __future__ import annotations

import math
import re

SCHEMA_VERSION = 1
HEIGHTS = (1770.0, 1440.0, 2090.0)
STAIR_FLIGHT_WIDTH = 160.0
STAIR_CENTER_GAP = 26.0  # 8cm partition plus both actual handrail base shoes.
STAIR_CORE_HALF = (2*STAIR_FLIGHT_WIDTH+STAIR_CENTER_GAP+20)/2
STAIR_FLIGHT_CENTER = (STAIR_CENTER_GAP+STAIR_FLIGHT_WIDTH)/2
STAIR_SHAFT_DEPTH = 680.0
STAIR_TURN_DEPTH = 180.0
ROOM_LABELS = {
    "living": "起居室", "bedroom": "卧室", "kitchen": "厨房", "bathroom": "卫生间",
    "shop": "营业区", "storage": "储藏间", "office": "办公室", "meeting": "会议室",
    "service": "服务区", "hall": "公共走廊", "stairs": "楼梯间", "roof": "屋顶",
}


def _finite(values):
    return all(isinstance(v, (int, float)) and math.isfinite(v) for v in values)


def compile_building(seed: dict) -> dict:
    ident = seed.get("id", "")
    width, depth = seed.get("width", 0), seed.get("depth", 0)
    variant = seed.get("variant", 0)
    one_storey = seed.get("storeys") == 1
    location, yaw = seed.get("location", [0, 0, 0]), seed.get("yaw", 0)
    if (not isinstance(ident, str) or not re.fullmatch(r"[A-Za-z0-9_]{1,48}", ident)
            or not _finite([width, depth, yaw]) or not isinstance(location, (tuple, list))
            or len(location) != 3 or not _finite(location)
            or variant not in (0, 1, 2) or width < (550 if one_storey else 700)
            or depth < (500 if one_storey else 1150)):
        raise ValueError("Invalid building identity, dimensions, variant or transform")
    kind = seed.get("kind", "residential")
    if kind not in ("residential", "office", "shop", "station"):
        raise ValueError("Unknown building use")
    external_sides = seed.get("external_sides", [-1, 1])
    if not isinstance(external_sides, list) or any(side not in (-1, 1) for side in external_sides):
        raise ValueError("Invalid external door visibility mask")
    height = float(seed.get("height", 400)) if one_storey else HEIGHTS[variant]
    if not math.isfinite(height) or (one_storey and not 300 <= height <= 600):
        raise ValueError("Invalid single-storey height")
    levels = [40.0] if one_storey else [40.0] + [425.0 + 330 * n for n in range(round((height - 410) / 330))] + [height]
    b = dict(schema_version=SCHEMA_VERSION, id=ident, width=float(width), depth=float(depth),
             height=height, variant=variant, kind=kind, location=list(location), yaw=float(yaw),
             floors=[], rooms=[], doors=[], nodes=[], edges=[], boxes=[], placements=[],
             expected_floor_ids=["F01"] if one_storey else [f"F{n + 1:02}" for n in range(len(levels) - 1)] + ["Roof"])
    front, rear = -depth / 2, depth / 2
    core_front = rear - (140 if one_storey else STAIR_SHAFT_DEPTH)
    rows = 2 if depth < 1900 else 4
    outer, core_half = width / 2 - 20, STAIR_CORE_HALF
    node_positions = {}

    def node(key, position, floor, room):
        key = f"{ident}_{key}"
        if key in node_positions:
            raise ValueError("Duplicate route node: " + key)
        p = [round(float(v), 6) for v in position]
        node_positions[key] = p
        b["nodes"].append(dict(id=key, position=p, floor=floor, room_id=room))
        return key

    def edge(a, z, edge_kind="flat", door=""):
        b["edges"].append(dict(**{"from": a, "to": z}, kind=edge_kind, door_id=door,
                               length=math.dist(node_positions[a], node_positions[z])))

    def box(key, center, size, box_kind="wall", material="M_Plaster"):
        if min(size) <= .00001:
            return
        b["boxes"].append(dict(id=f"{ident}_{key}", center=list(center), size=list(size),
                               yaw=0.0, kind=box_kind, material=material))

    def room(key, floor, room_kind, unit, lower, upper, center, route_node=None):
        rid = f"{ident}_{key}"
        rn = route_node or node(key + "_center", center, floor, rid)
        b["rooms"].append(dict(id=rid, floor=floor, kind=room_kind, unit=unit,
                               label=ROOM_LABELS[room_kind], minimum=list(lower), maximum=list(upper),
                               center=list(center), node=rn))
        return rid, rn

    def wall(key, axis, coordinate, start, end, bottom, top, opening=None):
        """A 20 cm wall; opening is (center along wall, clear-width)."""
        def part(suffix, a, z, low, high):
            if z <= a or high <= low:
                return
            center = [coordinate, (a + z) / 2, (low + high) / 2] if axis == "x" else [(a + z) / 2, coordinate, (low + high) / 2]
            size = [20, z - a, high - low] if axis == "x" else [z - a, 20, high - low]
            box(key + suffix, center, size)
        if opening is None:
            part("", start, end, bottom, top)
        else:
            center, clear = opening
            a, z = center - clear / 2 - 5, center + clear / 2 + 5
            if a < start or z > end or top - bottom < 240:
                raise ValueError("Door opening does not fit wall: " + key)
            part("_before", start, a, bottom, top)
            part("_after", z, end, bottom, top)
            part("_header", a, z, bottom + 220, top)

    def door(key, a_room, z_room, hinge, closed_yaw, opening_angle, door_width, a_node, z_node, external=False):
        did = f"{ident}_{key}"
        b["doors"].append(dict(id=did, from_room=a_room, to_room=z_room, hinge=list(hinge),
                               closed_yaw=float(closed_yaw), open_angle=float(opening_angle),
                               width=float(door_width), height=220.0, external=external,
                               node_a=a_node, node_b=z_node))
        edge(a_node, z_node, "door", did)
        # Frames leave the declared clear aperture, rather than narrowing it.
        angle = math.radians(closed_yaw)
        along = (math.cos(angle), math.sin(angle))
        for suffix, distance in [("hinge", -2.5), ("latch", door_width + 2.5)]:
            p = [hinge[0] + along[0] * distance, hinge[1] + along[1] * distance, hinge[2] + 112.5]
            box(key + "_frame_" + suffix, p, [5, 5, 225], "frame", "M_PaintedMetal")
        size = [door_width + 10, 5, 5] if abs(along[0]) > .5 else [5, door_width + 10, 5]
        p = [hinge[0] + along[0] * door_width / 2, hinge[1] + along[1] * door_width / 2, hinge[2] + 222.5]
        box(key + "_frame_header", p, size, "frame", "M_PaintedMetal")

    roots = []
    floor_data = []
    expected = []
    for n, z in enumerate(levels):
        is_roof = not one_storey and n == len(levels) - 1
        fid = "Roof" if is_roof else f"F{n + 1:02}"
        top = z + 260 if is_roof else height - 20 if one_storey else levels[n + 1] - 20
        floor = dict(id=fid, number=n + 1, z=z, kind="roof" if is_roof else "floor", rooms=[])
        b["floors"].append(floor)
        before = len(b["rooms"])
        hall_id = f"{ident}_{fid}_Hall"
        root = node(fid + "_landing", [0, core_front + 70, z], n + 1, hall_id)
        roots.append(root)
        room(fid + "_Hall", n + 1, "hall", "Public", [-70, front + 20, z], [70, core_front + 140, top],
             [0, core_front + 70, z], root)
        # Every floor has a physical landing; the step route is added below.
        if not one_storey:
            stair_id, stair_node = room(fid + "_Stairs", n + 1, "stairs", "Public",
                                        [-core_half, core_front, z], [core_half, rear - 20, top],
                                        [-STAIR_FLIGHT_CENTER, core_front + 70, z])
            edge(root, stair_node)
        box(fid + "_front_slab", [0, (front + core_front + 140) / 2, z - 10],
            [width, core_front + 140 - front, 20], "floor", "M_Concrete")
        for side in [-1, 1]:
            x = side * (core_half + width / 2) / 2
            box(fid + f"_rear_slab_{side}", [x, (core_front + 140 + rear) / 2, z - 10],
                [width / 2 - core_half, rear - core_front - 140, 20], "floor", "M_Concrete")
        if is_roof:
            roof_id, roof_node = room("Roof_Terrace", n + 1, "roof", "Public",
                                      [-outer, front + 20, z], [outer, rear - 20, z + 260],
                                      [0, core_front - 150, z])
            # Remove the unused public-hall envelope on the roof from locating;
            # its actual node remains the inside landing of the roof stairwell.
            b["rooms"][-3]["minimum"] = [-core_half+20, core_front + 10, z]
            b["rooms"][-3]["maximum"] = [core_half-20, core_front + 140, top]
            door("Roof_Door", stair_id, roof_id, [-60, core_front, z], 0, -90, 120, root, roof_node)
            wall("Roof_front", "y", core_front, -core_half, core_half, z, top, (0, 120))
            wall("Roof_left", "x", -core_half, core_front, rear, z, top)
            wall("Roof_right", "x", core_half, core_front, rear, z, top)
            wall("Roof_back", "y", rear - 10, -core_half, core_half, z, top)
            box("Roof_cover", [0, (core_front + rear) / 2, top + 10], [2*core_half, rear - core_front, 20], "roof", "M_Concrete")
            for side in [-1, 1]:
                wall(f"Roof_parapet_x{side}", "x", side * (width / 2 - 10), front, rear, z, z + 110)
            wall("Roof_parapet_front", "y", front + 10, -width / 2, width / 2, z, z + 110)
            wall("Roof_parapet_back", "y", rear - 10, -width / 2, width / 2, z, z + 110)
        else:
            clear_length = (rear - 20 if one_storey else core_front) - (front + 20)
            row_depth = clear_length / rows
            last_room = {}
            prior_hall = None
            for row in range(rows):
                lo, hi = front + 20 + row * row_depth, front + 20 + (row + 1) * row_depth
                y = (lo + hi) / 2
                hn = node(f"{fid}_corridor_{row}", [0, y, z], n + 1, hall_id)
                if prior_hall:
                    edge(prior_hall, hn)
                prior_hall = hn
                for side, unit in [(-1, "A"), (1, "B")]:
                    category = row + (2 if rows == 2 and side > 0 else 0)
                    rkind = ["living", "bedroom", "kitchen", "bathroom"][category]
                    if n == 0:
                        rkind = ["service" if kind == "station" else "shop", "storage", "kitchen", "bathroom"][category]
                    elif kind == "office":
                        rkind = ["office", "meeting", "storage", "bathroom"][category]
                    xlo, xhi = (-outer, -90) if side < 0 else (90, outer)
                    x = (xlo + xhi) / 2
                    rid, rn = room(f"{fid}_{unit}{row + 1:02}", n + 1, rkind, "01" if rows == 2 else unit,
                                   [xlo, lo + 10, z], [xhi, hi - 10, top], [x, y, z])
                    door(f"{fid}_{unit}{row + 1:02}_Door", hall_id, rid,
                         [side * 80, y - 50, z], 90, 90 if side < 0 else -90, 100, hn, rn)
                    wall(f"{fid}_{unit}{row}_hall_wall", "x", side * 80, lo, hi, z, top, (y, 100))
                    if row:
                        wall(f"{fid}_{unit}{row}_divider", "y", lo, xlo - 10, xhi + 10, z, top)
                    last_room[side] = (rid, rn, x)
                    # Furnish the far side of a room, leaving its central 120cm
                    # standing area and the door-to-center route clear.
                    b["placements"].append(dict(room_id=rid, floor=n + 1, kind=rkind,
                                                 center=[x + side * ((xhi - xlo) / 2 - 25), lo + 45, z], yaw=0))
            edge(prior_hall, root)
            # The rear strips beside the stair shaft are real utility rooms.
            for side, unit in ([] if one_storey else [(-1, "A"), (1, "B")]):
                xlo, xhi = (-outer, -core_half - 10) if side < 0 else (core_half + 10, outer)
                x = (xlo + xhi) / 2
                rid, rn = room(f"{fid}_{unit}Store", n + 1, "storage", unit,
                               [xlo, core_front + 10, z], [xhi, rear - 20, top], [x, (core_front + rear) / 2, z])
                prev_id, prev_node, prev_x = last_room[side]
                # Keep the approach beside the bed rather than pinching its outer
                # side; the full 100cm opening remains inside this rear strip.
                door_x = x - side * 35
                near = node(f"{fid}_{unit}Store_approach", [door_x, core_front - 85, z], n + 1, prev_id)
                edge(prev_node, near)
                # Cross the utility doorway squarely before turning toward its
                # standing point. A long diagonal cuts the bedroom furniture
                # corner when the neighboring stair shaft grows wider.
                inside = node(f"{fid}_{unit}Store_inside", [door_x,core_front+85,z], n+1, rid)
                door(f"{fid}_{unit}Store_Door", prev_id, rid, [door_x - 50, core_front, z], 0, 90, 100, near, inside)
                edge(inside,rn)
                wall(f"{fid}_{unit}_rear_room_front", "y", core_front, xlo - 10, xhi + 10, z, top, (door_x, 100))
                floor_data.append((n, side, rid, rn, x))
            # Ground-level main/side/back external doors, not painted door panels.
            front_shops = [r for r in b['rooms'] if n==0 and r['floor']==1 and r['kind']=='shop']
            if front_shops:
                # Keep the real central entrance and open the actual shop
                # envelopes toward the street, rather than hiding their art
                # behind a solid ground-floor wall. Glazing remains blocking.
                openings = [(-65,65,z,z+220,None)]
                for r in front_shops:
                    openings.append((r['minimum'][0]+10,r['maximum'][0]-10,z+55,z+240,r))
                cursor = -width/2
                for index,(a,c,low,high,r) in enumerate(sorted(openings,key=lambda o:o[0])):
                    wall(fid+'_front_pier_'+str(index),'y',front+10,cursor,a,z,top)
                    if low>z:
                        box(fid+'_shop_sill_'+str(index),[(a+c)/2,front+10,(z+low)/2],[c-a,20,low-z])
                    box(fid+'_front_header_'+str(index),[(a+c)/2,front+10,(high+top)/2],[c-a,20,top-high])
                    if r:
                        suffix = r['id'][len(ident)+1:]
                        box(suffix+'_ShopGlass',[(a+c)/2,front+10,(low+high)/2],
                            [c-a,3,high-low],'shop_glass','M_ShopGlass')
                        for side,x in (('left',a),('right',c)):
                            box(suffix+'_ShopFrame_'+side,[x,front-1,(low+high)/2],
                                [4,6,high-low+4],'shop_frame','M_PaintedMetal')
                        for side,zz in (('sill',low),('head',high)):
                            box(suffix+'_ShopFrame_'+side,[(a+c)/2,front-1,zz],
                                [c-a,6,4],'shop_frame','M_PaintedMetal')
                    cursor = c
                wall(fid+'_front_end','y',front+10,cursor,width/2,z,top)
            else:
                wall(fid + "_front", "y", front + 10, -width / 2, width / 2, z, top, (0, 120) if n == 0 else None)
            side_door_y = (core_front + rear) / 2
            for side in [-1, 1]:
                wall(fid + f"_outer_{side}", "x", side * (width / 2 - 10), front, rear, z, top,
                     (side_door_y, 120) if n == 0 and not one_storey and side in external_sides else None)
                if not one_storey:
                    wall(fid + f"_shaft_{side}", "x", side * core_half, core_front, rear, z, top)
            right_store = next((r for r in floor_data if r[0] == n and r[1] == 1), None)
            wall(fid + "_rear", "y", rear - 10, -width / 2, width / 2, z, top,
                 (right_store[4], 120) if n == 0 and right_store else None)
            if one_storey:
                box("Annex_roof", [0, 0, height - 10], [width, depth, 20], "roof", "M_Concrete")
        floor["rooms"] = [r["id"] for r in b["rooms"][before:]]
        expected.extend(floor["rooms"])

    # Exterior and interior threshold nodes both remain fixed across rebuilds.
    entry = node("Street", [0, front - 140, 0], 0, "Outside")
    threshold = node("EntryInside", [0, front + 110, levels[0]], 1, f"{ident}_F01_Hall")
    ground_hall_first = f"{ident}_F01_corridor_0"
    edge(threshold, ground_hall_first)
    door("Entry_Door", "Outside", f"{ident}_F01_Hall", [-60, front + 10, levels[0]], 0, -90, 120, entry, threshold, True)
    # Three real entrance treads bridge the 40cm plinth.
    for i in range(3):
        top = levels[0] * (i + 1) / 3
        box(f"Entry_step_{i}", [0, front - (2.5 - i) * 28, top / 2], [140, 28, top], "step", "M_Concrete")
    for _, side, rid, rn, x in [r for r in floor_data if r[0] == 0]:
        y = (core_front + rear) / 2
        if side in external_sides:
            out = node(f"SideStreet_{side}", [side * (width / 2 + 140), y, 0], 0, "Outside")
            door(f"SideDoor_{side}", "Outside", rid, [side * (width / 2 - 10), y - 60, levels[0]], 90,
                 90 if side < 0 else -90, 120, out, rn, True)
        if side > 0:
            out = node("RearStreet", [x, rear + 140, 0], 0, "Outside")
            door("RearDoor", "Outside", rid, [x - 60, rear - 10, levels[0]], 0, 90, 120, out, rn, True)
        for i in range(3):
            top = levels[0] * (i + 1) / 3
            if side in external_sides:
                box(f"Side_step_{side}_{i}", [side * (width / 2 + (2.5 - i) * 28), y, top / 2],
                    [28, 140, top], "step", "M_Concrete")
            if side > 0:
                box(f"Rear_step_{i}", [x, rear + (2.5 - i) * 28, top / 2], [140, 28, top], "step", "M_Concrete")
    b["entrance_node"] = entry

    # A U-shaped stair has individual treads, an actual half-landing and return
    # flight. Floor slabs deliberately leave its shaft open.
    for n, (low, high) in enumerate(zip(levels, levels[1:])):
        count = math.ceil((high - low) / 17 / 2)
        rise, run = (high - low) / (2 * count), count * 28
        if 140 + run + STAIR_TURN_DEPTH > STAIR_SHAFT_DEPTH-10:
            raise ValueError("Floor rise does not fit stair shaft")
        prefix = f"F{n + 1:02}"
        stair_id = f"{ident}_{prefix}_Stairs"
        first = f"{ident}_{prefix}_Stairs_center"
        previous = first
        for i in range(count):
            y, z = core_front + 140 + (i + .5) * 28, low + (i + 1) * rise
            step = node(f"{prefix}_up_{i}", [-STAIR_FLIGHT_CENTER, y, z], n + 1, stair_id)
            edge(previous, step, "stairs")
            box(f"{prefix}_up_{i}", [-STAIR_FLIGHT_CENTER, y, z - 10], [STAIR_FLIGHT_WIDTH, 28, 20], "step", "M_Concrete")
            previous = step
        mid_z, mid_y = low + count * rise, core_front + 140 + run + STAIR_TURN_DEPTH/2
        for i, p in enumerate([[-STAIR_FLIGHT_CENTER, mid_y, mid_z], [STAIR_FLIGHT_CENTER, mid_y, mid_z], [STAIR_FLIGHT_CENTER, core_front + 140 + run + 14, mid_z]]):
            nxt = node(f"{prefix}_turn_{i}", p, n + 1, stair_id)
            edge(previous, nxt)
            previous = nxt
        box(prefix + "_half_landing", [0, mid_y, mid_z - 10], [2*core_half, STAIR_TURN_DEPTH, 20], "floor", "M_Concrete")
        for i in range(count):
            y, z = core_front + 140 + (count - i - .5) * 28, low + (count + i + 1) * rise
            step = node(f"{prefix}_return_{i}", [STAIR_FLIGHT_CENTER, y, z], n + 1, stair_id)
            edge(previous, step, "stairs")
            box(f"{prefix}_return_{i}", [STAIR_FLIGHT_CENTER, y, z - 10], [STAIR_FLIGHT_WIDTH, 28, 20], "step", "M_Concrete")
            previous = step
        top = node(prefix + "_top", [STAIR_FLIGHT_CENTER, core_front + 70, high], n + 2, f"{ident}_{b['floors'][n + 1]['id']}_Hall")
        edge(previous, top)
        edge(top, roots[n + 1])
        # The center partition prevents stepping sideways into the lower flight.
        box(prefix + "_stair_partition", [0, core_front + 140 + run / 2, (low + high) / 2], [8, run, high - low], "wall")
        box(prefix + "_back_rail", [0, mid_y + STAIR_TURN_DEPTH/2+2, mid_z + 55], [2*core_half, 5, 110], "rail", "M_PaintedMetal")
    b["expected_room_ids"] = sorted(expected)
    errors = validate_building(b)
    if errors:
        raise ValueError("; ".join(errors))
    return b


def validate_building(b: dict) -> list[str]:
    errors = []
    nodes = {n["id"]: n for n in b["nodes"]}
    rooms = {r["id"]: r for r in b["rooms"]}
    doors = {d["id"]: d for d in b["doors"]}
    for name, values in [("nodes", nodes), ("rooms", rooms), ("doors", doors)]:
        if len(values) != len(b[name]):
            errors.append("Duplicate " + name)
    if set(rooms) != set(b["expected_room_ids"]):
        errors.append("Room coverage differs from expected inventory")
    if [f["id"] for f in b["floors"]] != b["expected_floor_ids"]:
        errors.append("Floor coverage differs from expected inventory")
    valid_floors = {f["number"] for f in b["floors"]}
    graph = {n: [] for n in nodes}
    for e in b["edges"]:
        if e["from"] not in nodes or e["to"] not in nodes or not _finite([e["length"]]) or e["length"] <= 0:
            errors.append("Invalid route edge")
            continue
        graph[e["from"]].append(e["to"])
        graph[e["to"]].append(e["from"])
        if e["kind"] == "door" and e["door_id"] not in doors:
            errors.append("Missing edge door")
    reached, todo = set(), [b["entrance_node"]]
    while todo:
        n = todo.pop()
        if n not in reached and n in graph:
            reached.add(n)
            todo.extend(graph[n])
    if reached != set(nodes):
        errors.append("Disconnected route node")
    for r in rooms.values():
        if r["floor"] not in valid_floors or r["node"] not in reached:
            errors.append("Invalid room address or access: " + r["id"])
        if not _finite(r["minimum"] + r["maximum"] + r["center"]) or any(r["minimum"][i] >= r["maximum"][i] for i in range(3)):
            errors.append("Invalid room bounds: " + r["id"])
    for d in doors.values():
        if any(r != "Outside" and r not in rooms for r in [d["from_room"], d["to_room"]]):
            errors.append("Door references missing room: " + d["id"])
        if d["node_a"] not in nodes or d["node_b"] not in nodes or not _finite(d["hinge"] + [d["closed_yaw"], d["open_angle"], d["width"], d["height"]]):
            errors.append("Invalid door geometry: " + d["id"])
        if d["width"] < (120 if d["external"] else 100) or d["height"] < 220:
            errors.append("Door aperture too small: " + d["id"])
    for box in b["boxes"]:
        if not _finite(box["center"] + box["size"]) or min(box["size"]) <= 0:
            errors.append("Invalid structural box")
    return errors
