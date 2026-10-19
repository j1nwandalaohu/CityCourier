"""Pure-math finish overlays for three existing, unchanged canonical buildings.

All placement transforms are building-local centimeters and UE pitch/yaw/roll.
The caller applies ``building.location`` and ``building.yaw`` exactly once.  This
module never imports Blender/Unreal, changes source inventory, or writes a map.
Furniture and rails are blocking; thin finish geometry uses NoCollision.  Moving
door leaves are intentionally described by ``door_styles``, never spawned closed
across navigation routes as decorative geometry.
"""
from __future__ import annotations

import copy
import hashlib
import json
import math

from tools.interiors.layout import compile_building

SCHEMA_VERSION = 1
ROUTE_CLEARANCE_CM = 45.0  # 34 cm courier capsule + 11 cm fitting allowance.
STANDING_HALF_CM = 60.0
FURNITURE = {"bedroom": "Bed", "living": "Sofa", "kitchen": "Kitchen",
             "bathroom": "Toilet", "storage": "Shelf", "office": "Desk",
             "meeting": "Desk", "shop": "Counter", "service": "Counter"}
PREFERRED_IDS = {"shop": "B_Merchant_0", "residential": "B_1_0_0_0_0",
                 "office": "B_0_1_0_0_0"}
# Blender single-precision coordinates carry sub-micron cm noise at flush
# boundaries. This tolerance is 0.001 mm, far below any gameplay clearance.
EPS = 1e-4


def _bounds(bounds):
    lo, hi = bounds.get("minimum_cm"), bounds.get("maximum_cm")
    if (not isinstance(lo, (list, tuple)) or not isinstance(hi, (list, tuple))
            or len(lo) != 3 or len(hi) != 3
            or any(not isinstance(v, (int, float)) or not math.isfinite(v) for v in list(lo) + list(hi))
            or any(lo[i] >= hi[i] for i in range(3))):
        raise ValueError("Invalid actual mesh bounds")
    return lo, hi


def _rotate(point, pyr):
    """UE positive pitch elevates +X; yaw rotates +X toward +Y."""
    p, y, r = map(math.radians, pyr)
    x, yy, z = point
    yy, z = yy * math.cos(r) + z * math.sin(r), -yy * math.sin(r) + z * math.cos(r)
    x, z = x * math.cos(p) - z * math.sin(p), x * math.sin(p) + z * math.cos(p)
    return [x * math.cos(y) - yy * math.sin(y), x * math.sin(y) + yy * math.cos(y), z]


def placement_bounds(placement: dict, mesh_bounds: dict):
    """World-axis AABB in building-local space, including actual mesh pivot."""
    lo, hi = _bounds(mesh_bounds)
    points = []
    for x in (lo[0], hi[0]):
        for y in (lo[1], hi[1]):
            for z in (lo[2], hi[2]):
                point = _rotate([v * s for v, s in zip((x, y, z), placement.get("scale3", [1, 1, 1]))], placement["rotation_pyr"])
                points.append([v + t for v, t in zip(point, placement["location_cm"])])
    return ([min(p[i] for p in points) for i in range(3)],
            [max(p[i] for p in points) for i in range(3)])


def _segment_box(a, b, lo, hi):
    start, end = 0.0, 1.0
    for i in range(len(lo)):
        delta = b[i] - a[i]
        if abs(delta) < EPS:
            if a[i] < lo[i] - EPS or a[i] > hi[i] + EPS:
                return False
        else:
            x, y = (lo[i] - a[i]) / delta, (hi[i] - a[i]) / delta
            start, end = max(start, min(x, y)), min(end, max(x, y))
            if start > end + EPS:
                return False
    return True


def _overlap(lo, hi, other_lo, other_hi, dimensions=3):
    return all(min(hi[i], other_hi[i]) - max(lo[i], other_lo[i]) > EPS for i in range(dimensions))


def intersects_door_aperture(lo, hi, door):
    """Clear span only; frame jambs outside the endpoints do not count."""
    h, angle = door["hinge"], math.radians(door["closed_yaw"])
    if hi[2] <= h[2] + EPS or lo[2] >= h[2] + door["height"] - EPS:
        return False
    c, s = math.cos(angle), math.sin(angle)
    corners = [((x - h[0]) * c + (y - h[1]) * s,
                -(x - h[0]) * s + (y - h[1]) * c)
               for x in (lo[0], hi[0]) for y in (lo[1], hi[1])]
    return (max(p[0] for p in corners) > EPS
            and min(p[0] for p in corners) < door["width"] - EPS
            and max(p[1] for p in corners) > -5 + EPS
            and min(p[1] for p in corners) < 5 - EPS)


def intersects_door_sweep(lo, hi, door):
    """Conservative closed-to-open sector, including an 8 cm leaf allowance.

    The circumscribed polygon overestimates the arc; no furniture can fall
    between the sampled angular rays.  Only the door's actual vertical span
    participates, so furnishings on other floors are unaffected.
    """
    h = door["hinge"]
    if hi[2] <= h[2] + EPS or lo[2] >= h[2] + door["height"] - EPS:
        return False
    low, high = [lo[i] - 8 for i in range(2)], [hi[i] + 8 for i in range(2)]
    count = max(1, math.ceil(abs(door["open_angle"]) / 2))
    radius = door["width"] / math.cos(math.radians(abs(door["open_angle"]) / count / 2))
    points = [h[:2]]
    for index in range(count + 1):
        angle = math.radians(door["closed_yaw"] + door["open_angle"] * index / count)
        points.append([h[0] + radius * math.cos(angle), h[1] + radius * math.sin(angle)])
    if any(_segment_box(a, b, low, high) for a, b in zip(points, points[1:] + points[:1])):
        return True
    # A box wholly inside the sector has no boundary intersection.
    for x in (low[0], high[0]):
        for y in (low[1], high[1]):
            angle = math.degrees(math.atan2(y - h[1], x - h[0]))
            relative = (angle - door["closed_yaw"]) % 360
            inside_angle = relative <= door["open_angle"] + EPS if door["open_angle"] >= 0 else (relative >= 360 + door["open_angle"] - EPS or relative < EPS)
            if inside_angle and math.hypot(x - h[0], y - h[1]) <= radius:
                return True
    return False


def _route_blocked(lo, hi, building):
    nodes = {n["id"]: n for n in building["nodes"]}
    # Sweep a conservative upright capsule AABB along EVERY graph edge, not
    # only edges whose end room happens to match the furnished room.
    expanded_lo = [lo[0] - ROUTE_CLEARANCE_CM, lo[1] - ROUTE_CLEARANCE_CM, lo[2] - 178]
    expanded_hi = [hi[0] + ROUTE_CLEARANCE_CM, hi[1] + ROUTE_CLEARANCE_CM, hi[2] - 2]
    for edge in building["edges"]:
        a, b = nodes[edge["from"]]["position"], nodes[edge["to"]]["position"]
        if _segment_box(a, b, expanded_lo, expanded_hi):
            return edge
    return None


def _furniture_errors(placement, room, building, bounds):
    lo, hi = placement_bounds(placement, bounds)
    errors = []
    if any(lo[i] < room["minimum"][i] - EPS or hi[i] > room["maximum"][i] + EPS for i in range(3)):
        errors.append("outside room bounds")
    c = room["center"]
    if _overlap(lo, hi, [c[0] - 60, c[1] - 60, c[2]], [c[0] + 60, c[1] + 60, c[2] + 178]):
        errors.append("standing square blocked")
    if _route_blocked(lo, hi, building):
        errors.append("route capsule blocked")
    if any(intersects_door_sweep(lo, hi, d) for d in building["doors"]):
        errors.append("door sweep blocked")
    return errors


def _placement(building, suffix, asset, location, floor, room=None, category="trim", rotation=None, scale=None, collision="NoCollision", **metadata):
    result = dict(name=building["id"] + "_IF_" + suffix, building_id=building["id"],
                  asset="SM_IF_" + asset, location_cm=list(location),
                  rotation_pyr=list(rotation or [0, 0, 0]), scale3=list(scale or [1, 1, 1]),
                  collision=collision, floor=floor, category=category)
    if room:
        result["room_id"] = room["id"] if isinstance(room, dict) else room
    result.update(metadata)
    return result


def _fit_furniture(building, room, bounds, kind=None, existing=()):
    kind = kind or FURNITURE[room["kind"]]
    asset = "SM_IF_" + kind
    candidates = []
    for yaw in (0, 90, 180, 270):
        suffix = "_Furniture" if not existing else "_Furniture_" + kind
        base = _placement(building, room["id"] + suffix, kind, [0, 0, 0], room["floor"], room,
                          "furniture", rotation=[0, yaw, 0], collision="BlockAll",
                          scale=[.85, .50 if kind == "Toilet" else .60, 1] if len(building["floors"]) == 1 else None)
        lo, hi = placement_bounds(base, bounds[asset])
        for corner in range(4):
            translation = [room["maximum"][0] - hi[0] - 6 if corner & 1 else room["minimum"][0] - lo[0] + 6,
                           room["maximum"][1] - hi[1] - 6 if corner & 2 else room["minimum"][1] - lo[1] + 6,
                           room["minimum"][2] - lo[2]]
            candidate = {**base, "location_cm": translation}
            if not _furniture_errors(candidate, room, building, bounds[asset]):
                c_lo, c_hi = placement_bounds(candidate, bounds[asset])
                if any(_overlap(c_lo, c_hi, *placement_bounds(other, bounds[other["asset"]])) for other in existing):
                    continue
                # All seven kits face local -Y. Prefer their usable fronts
                # toward the room, rather than showing furniture backs.
                angle = math.radians(yaw)
                toward = [room["center"][i] - (c_lo[i] + c_hi[i]) / 2 for i in range(2)]
                facing = math.sin(angle)*toward[0] - math.cos(angle)*toward[1]
                candidates.append((facing, candidate))
    if candidates:
        return max(candidates, key=lambda value: value[0])[1]
    if existing:
        return None
    raise ValueError("Cannot fit " + kind + " in " + room["id"] + " with actual bounds, standing square, all route capsules and door sweeps")


def _wall_finishes(building, bounds):
    """Use actual wall fragments, whose gaps already contain all door openings."""
    result = []
    if "SM_IF_Skirting" not in bounds:
        return result
    floors = [f for f in building["floors"] if f["kind"] != "roof"]
    for wall in building["boxes"]:
        if wall["kind"] != "wall" or min(wall["size"][:2]) != 20 or wall["size"][2] < 220:
            continue
        low = [wall["center"][i] - wall["size"][i] / 2 for i in range(3)]
        floor = next((f for f in floors if abs(f["z"] - low[2]) < EPS), None)
        if floor is None:
            continue
        axis = 0 if wall["size"][1] == 20 else 1
        length = wall["size"][axis]
        # Keep the tiny finish end caps inside their source wall fragment.
        length -= .2
        if length < .1:
            continue
        for face in (-1, 1):
            location = low[:]
            location[axis] += .1
            location[1 - axis] = wall["center"][1 - axis] + face * 10.8
            trim = _placement(building, wall["id"] + "_Skirting_" + str(face), "Skirting", location,
                              floor["number"], category="skirting", rotation=[0, 90 if axis else 0, 0], scale=[length / 100, 1, 1], wall_id=wall["id"])
            lo, hi = placement_bounds(trim, bounds[trim["asset"]])
            if not _route_blocked(lo, hi, building) and not any(intersects_door_aperture(lo, hi, d) for d in building["doors"]):
                result.append(trim)
            if "SM_IF_DadoPanel" in bounds:
                probe = wall["center"][:]
                probe[1 - axis] += face * 11
                public = next((r for r in building["rooms"]
                               if r["floor"] == floor["number"] and r["kind"] in ("hall", "stairs")
                               and all(r["minimum"][i] - EPS <= probe[i] <= r["maximum"][i] + EPS for i in range(2))), None)
                if public is None:
                    continue
                # Expose the -Y face to the room, keeping the +Y back on the
                # actual plaster face; reverse the segment when necessary.
                panel_lo, panel_hi = _bounds(bounds["SM_IF_DadoPanel"])
                yaw = (180 if face > 0 else 0) if axis == 0 else (90 if face > 0 else 270)
                along_sign = 1 if yaw in (0, 90) else -1
                location = low[:]
                location[axis] = low[axis] + .1 if along_sign > 0 else low[axis] + wall["size"][axis] - .1
                location[1 - axis] = wall["center"][1 - axis] + face * (10 + panel_hi[1])
                panel = _placement(building, wall["id"] + "_Dado_" + str(face), "DadoPanel", location,
                                   floor["number"], public, "dado", rotation=[0, yaw, 0],
                                   scale=[length / (panel_hi[0] - panel_lo[0]), 1, 1], wall_id=wall["id"])
                lo, hi = placement_bounds(panel, bounds[panel["asset"]])
                if not _route_blocked(lo, hi, building) and not any(intersects_door_aperture(lo, hi, d) for d in building["doors"]):
                    result.append(panel)
    return result


def _door_fixtures(building, door, room, bounds):
    """Blank room plates, wall controls and high exit fixtures; no text actors."""
    result = []
    angle = math.radians(door["closed_yaw"])
    along = [math.cos(angle), math.sin(angle)]
    normal = [-along[1], along[0]]
    middle = [door["hinge"][i] + along[i] * door["width"] / 2 for i in range(2)]
    sign = 1 if sum((room["center"][i] - middle[i]) * normal[i] for i in range(2)) >= 0 else -1
    # Wall center plane is door hinge plane; the room-facing surface is 10 cm
    # away. +11 cm leaves the thin back against the plaster face.
    rotation = [0, door["closed_yaw"] + (180 if sign > 0 else 0), 0]
    for asset, category, height in (("NumberPlate", "number_plate", 155),
                                    ("SwitchPlate", "switch", 112),
                                    ("Outlet", "outlet", 30)):
        if "SM_IF_" + asset not in bounds:
            continue
        location = [door["hinge"][i] - along[i] * 22 + normal[i] * sign * 11 for i in range(2)] + [door["hinge"][2] + height]
        p = _placement(building, door["id"] + "_" + asset, asset, location, room["floor"], room,
                       category, rotation=rotation, door_id=door["id"])
        lo, hi = placement_bounds(p, bounds[p["asset"]])
        if not _route_blocked(lo, hi, building) and not any(intersects_door_aperture(lo, hi, d) for d in building["doors"]):
            result.append(p)
    if door["external"] and "SM_IF_ExitSign" in bounds:
        location = [middle[i] + normal[i] * sign * 11 for i in range(2)] + [door["hinge"][2] + 235]
        p = _placement(building, door["id"] + "_ExitSign", "ExitSign", location, room["floor"], room,
                       "exit_sign", rotation=rotation, door_id=door["id"])
        lo, hi = placement_bounds(p, bounds[p["asset"]])
        if hi[2] <= room["maximum"][2] and not _route_blocked(lo, hi, building):
            result.append(p)
    return result


def _handrails(building, bounds):
    if not all("SM_IF_" + name in bounds for name in ("HandrailPost", "HandrailBar")):
        return []
    result = []
    post_lo, post_hi = _bounds(bounds["SM_IF_HandrailPost"])
    bar_lo, bar_hi = _bounds(bounds["SM_IF_HandrailBar"])
    radius = max(abs(post_lo[0]), abs(post_hi[0]), abs(bar_lo[1]), abs(bar_hi[1]))
    rail_x = 25 - radius - .5
    if rail_x - radius <= 4:
        raise ValueError("Actual handrail shoe cannot fit between partition and original 120 cm treads")
    for index, (low, high) in enumerate(zip(building["floors"], building["floors"][1:])):
        partition = next(b for b in building["boxes"] if b["id"] == building["id"] + "_F%02d_stair_partition" % (index + 1))
        start = partition["center"][1] - partition["size"][1] / 2
        run = partition["size"][1]
        count = round(run / 28)
        rise = (high["z"] - low["z"]) / (2 * count)
        room_id = building["id"] + "_F%02d_Stairs" % (index + 1)
        # Original flights cover [-145,-25] and [25,145]. Both rail faces stay
        # inside the 50 cm center gap, entirely outside those 120 cm treads.
        for side in (-1, 1):
            points = []
            for step in sorted(set([0, count] + list(range(3, count, 3)))):
                y = start + step * 28
                z = low["z"] + (step if side < 0 else 2 * count - step) * rise
                points.append([side * rail_x, y, z + 90])
                result.append(_placement(building, "F%02d_Rail_%s_Post_%02d" % (index + 1, side, step),
                                         "HandrailPost", [side * rail_x, y, z], low["number"], room_id,
                                         "handrail", collision="BlockAll"))
            for number, (a, b) in enumerate(zip(points, points[1:])):
                delta = [b[i] - a[i] for i in range(3)]
                result.append(_placement(building, "F%02d_Rail_%s_Bar_%02d" % (index + 1, side, number),
                                         "HandrailBar", a, low["number"], room_id, "handrail",
                                         rotation=[math.degrees(math.atan2(delta[2], math.hypot(*delta[:2]))), 90, 0],
                                         scale=[math.dist(a, b) / 100, 1, 1], collision="BlockAll"))
    return result


def _remaining_finishes(building, bounds):
    """Attach the delivered kit to actual treads, high walls and existing glazing."""
    result = []
    if 'SM_IF_StairNosing' in bounds:
        for step in building['boxes']:
            if step['kind'] != 'step' or not any(t in step['id'] for t in ('_up_', '_return_')):
                continue
            # Flight direction reverses on the return; sit inside the riser edge.
            direction = 1 if '_return_' in step['id'] else -1
            location = [step['center'][0], step['center'][1] + direction*(step['size'][1]/2-2),
                        step['center'][2] + step['size'][2]/2 + .05]
            floor = next(f['number'] for f in building['floors'] if '_'+f['id']+'_' in step['id'])
            result.append(_placement(building, step['id']+'_Nosing', 'StairNosing', location, floor,
                category='stair_nosing', scale=[step['size'][0]/100, 1, 1], step_id=step['id']))
    if 'SM_IF_AirconWall' in bounds:
        for room in building['rooms']:
            if room['kind'] not in ('living', 'bedroom', 'office', 'meeting', 'shop', 'service'):
                continue
            side = -1 if room['center'][0] < 0 else 1
            # Back on the outer side wall; local -Y faces inward after rotation.
            x = room['minimum'][0] if side < 0 else room['maximum'][0]
            p = _placement(building, room['id']+'_Aircon', 'AirconWall',
                [x-side*11, room['center'][1], room['maximum'][2]-36], room['floor'], room,
                'wall_aircon', rotation=[0, 90 if side < 0 else -90, 0])
            lo, hi = placement_bounds(p, bounds[p['asset']])
            if not _route_blocked(lo, hi, building) and not any(intersects_door_sweep(lo, hi, d) for d in building['doors']):
                result.append(p)
    if 'SM_IF_WindowReturn' in bounds and len(building['floors']) > 1:
        # Match build_environment.building's existing CLOSED facade windows.
        # These are exterior glazing surrounds, not new walkable wall openings.
        variant = building['variant']
        for floor in building['floors'][1:]:
            if floor['kind'] == 'roof':
                continue
            index = floor['number']-2
            for col, x in enumerate((-265, 0, 265)):
                x += (3.5, -6.5, 4)[(index+variant+col)%3]
                width = (173, 182, 163)[(col+variant)%3]
                height = 192 if variant != 1 else 182
                result.append(_placement(building, floor['id']+'_Window_'+str(col), 'WindowReturn',
                    [x*building['width']/800, -building['depth']/2-4*building['depth']/1150-18,
                     530+index*330-height/2], floor['number'], category='existing_facade_window',
                    scale=[width/120*building['width']/800, 1, height/135]))
    return result


def _sample(seed, bounds):
    building = compile_building(seed)
    sample = dict(building_id=building["id"], kind=seed["kind"], source_seed=copy.deepcopy(seed), building=building,
                  placements=[], door_styles=[], preview_room_ids=[])
    placements = sample["placements"]
    for room in building["rooms"]:
        if building["floors"][room["floor"]-1]["kind"] == "roof":
            continue
        if room["kind"] in FURNITURE:
            group = [_fit_furniture(building, room, bounds)]
            # Reusable room assemblies adapt to actual room dimensions. A
            # secondary piece is omitted when its true envelope cannot fit.
            extra = {"shop": "Shelf", "service": "Shelf", "office": "Shelf",
                     "meeting": "Shelf", "living": "Desk", "bedroom": "Desk",
                     "kitchen": "Shelf"}.get(room["kind"])
            if extra:
                secondary = _fit_furniture(building, room, bounds, extra, group)
                if secondary:
                    group.append(secondary)
            placements.extend(group)
        if "SM_IF_LightPanel" in bounds:
            placements.append(_placement(building, room["id"] + "_CeilingLight", "LightPanel",
                                         [room["center"][0], room["center"][1], room["maximum"][2] - 8], room["floor"], room,
                                         "light_fixture"))
            if room["kind"] == "stairs":
                landing = next(box for box in building["boxes"]
                               if box["id"] == building["id"] + "_F%02d_half_landing" % room["floor"])
                landing_z = landing["center"][2] + landing["size"][2]/2
                # The shaft is open above the half landing. Mount the second
                # real fixture on its rear wall, 225cm over that landing, not
                # at the floor's nominal ceiling (which would hit headroom).
                placements.append(_placement(building, room["id"] + "_LandingWallLight", "LightPanel",
                    [0, room["maximum"][1]-5, landing_z+225], room["floor"], room,
                    "light_fixture", rotation=[0,0,90], mount="rear_wall_above_half_landing"))
        if (seed["kind"] == "shop" and room["floor"] == 1
                or seed["kind"] != "shop" and room["floor"] == 2
                or room["floor"] == 1 and room["kind"] in ("hall", "stairs")):
            sample["preview_room_ids"].append(room["id"])
    rooms = {r["id"]: r for r in building["rooms"]}
    for door in building["doors"]:
        room = rooms.get(door["to_room"], rooms.get(door["from_room"]))
        if "SM_IF_DoorFrame" in bounds:
            placements.append(_placement(building, door["id"] + "_Frame", "DoorFrame", door["hinge"], room["floor"], room,
                                         "door_frame", rotation=[0, door["closed_yaw"], 0],
                                         scale=[door["width"] / 100, 1, door["height"] / 220], door_id=door["id"]))
        placements.extend(_door_fixtures(building, door, room, bounds))
        style = "Shop" if room["floor"] == 1 and (seed["kind"] == "shop" or room["kind"] in ("shop", "service")) else "Office" if seed["kind"] == "office" else "Residential"
        sample["door_styles"].append(dict(door_id=door["id"], leaf_asset="SM_IF_Door" + style,
                                           integration="replace_existing_moving_leaf_mesh"))
    placements.extend(_wall_finishes(building, bounds))
    placements.extend(_handrails(building, bounds))
    placements.extend(_remaining_finishes(building, bounds))
    return sample


def plan_samples(inventory: dict, mesh_bounds: dict) -> dict:
    """Return three deterministic overlays or reject an unsafe actual kit.

    Optional finish meshes may be omitted; all seven furniture bounds are
    required.  The input must contain the three original building uses.  IDs,
    transforms, door apertures and every route anchor remain canonical.
    """
    for kind in sorted(set(FURNITURE.values())):
        name = "SM_IF_" + kind
        if name not in mesh_bounds:
            raise ValueError("Missing actual bounds: " + name)
    for name, bounds in mesh_bounds.items():
        try:
            _bounds(bounds)
        except (TypeError, AttributeError, ValueError) as exc:
            raise ValueError("Invalid actual mesh bounds: " + name) from exc
    plan = dict(schema_version=SCHEMA_VERSION, coordinate_space="building_local_cm",
                clearance_cm=dict(capsule_radius=34, route_allowance=11, route_total=45,
                                  central_standing_square=120), samples=[])
    for kind in ("shop", "residential", "office"):
        candidates = sorted([s for s in inventory["buildings"] if s["kind"] == kind and s.get("storeys") != 1],
                            key=lambda s: (s["id"] != PREFERRED_IDS[kind], s["id"]))
        if not candidates:
            raise ValueError("No existing " + kind + " sample in inventory")
        plan["samples"].append(_sample(candidates[0], mesh_bounds))
    errors = validate_plan(plan, mesh_bounds)
    if errors:
        raise ValueError("Unsafe sample overlay: " + "; ".join(errors))
    return plan


def plan_city(inventory: dict, mesh_bounds: dict) -> dict:
    """Apply approved assemblies to every canonical building, including annexes."""
    for name in FURNITURE.values():
        if "SM_IF_" + name not in mesh_bounds:
            raise ValueError("Missing actual bounds: SM_IF_" + name)
    for bounds in mesh_bounds.values():
        _bounds(bounds)
    plan = dict(schema_version=SCHEMA_VERSION, coordinate_space="building_local_cm",
                clearance_cm=dict(capsule_radius=34, route_allowance=11, route_total=45,
                                  central_standing_square=120), samples=[])
    for seed in inventory["buildings"]:
        sample = _sample(seed, mesh_bounds)
        errors = validate_plan(dict(samples=[sample]), mesh_bounds)
        if errors:
            raise ValueError("Unsafe city overlay: " + "; ".join(errors[:12]))
        plan["samples"].append(sample)
    return plan


def validate_plan(plan: dict, mesh_bounds: dict) -> list[str]:
    """Recheck serialized overlays using true asset bounds; never fix by flags."""
    errors = []
    names = set()
    for sample in plan["samples"]:
        building = sample["building"]
        if building != compile_building(sample["source_seed"]):
            errors.append(sample["building_id"] + ": canonical building record/IDs/route anchors changed")
        rooms = {r["id"]: r for r in building["rooms"]}
        doors = {d["id"]: d for d in building["doors"]}
        for p in sample["placements"]:
            prefix = p["name"] + ": "
            if p["name"] in names:
                errors.append(prefix + "duplicate placement identity")
            names.add(p["name"])
            if p["building_id"] != building["id"]:
                errors.append(prefix + "incorrect building parent")
            lo, hi = placement_bounds(p, mesh_bounds[p["asset"]])
            if p["category"] == "furniture":
                if p["collision"] != "BlockAll":
                    errors.append(prefix + "furniture collision disabled")
                errors.extend(prefix + e for e in _furniture_errors(p, rooms[p["room_id"]], building, mesh_bounds[p["asset"]]))
                for other in sample["placements"]:
                    if (other["category"] == "furniture" and other["name"] < p["name"]
                            and other.get("room_id") == p["room_id"]
                            and _overlap(lo, hi, *placement_bounds(other, mesh_bounds[other["asset"]]))):
                        errors.append(prefix + "furniture assembly overlaps " + other["name"])
            if p["category"] == "handrail":
                if p["collision"] != "BlockAll":
                    errors.append(prefix + "handrail collision disabled")
                if lo[0] < -25 - EPS or hi[0] > 25 + EPS:
                    errors.append(prefix + "handrail narrows 120 cm tread width")
                if _route_blocked(lo, hi, building):
                    errors.append(prefix + "handrail blocks route capsule")
            if p["category"] == "skirting" and any(intersects_door_aperture(lo, hi, door) for door in building["doors"]):
                errors.append(prefix + "skirting spans door aperture")
            if p['category'] == 'stair_nosing':
                step = next((s for s in building['boxes'] if s['id'] == p.get('step_id') and s['kind'] == 'step'), None)
                if (not step or p['collision'] != 'NoCollision' or
                        abs(p['location_cm'][2] - step['center'][2] - step['size'][2]/2 - .05) > EPS or
                        hi[2]-lo[2] > 1 or lo[0] < step['center'][0]-step['size'][0]/2-EPS or
                        hi[0] > step['center'][0]+step['size'][0]/2+EPS):
                    errors.append(prefix + 'nosing differs from real tread surface')
            if p["category"] not in ("door_frame", "furniture", "handrail", "stair_nosing"):
                if _route_blocked(lo, hi, building):
                    errors.append(prefix + "finish intrudes into route capsule")
                if any(intersects_door_aperture(lo, hi, door) for door in building["doors"]):
                    errors.append(prefix + "finish spans door aperture")
            if p["category"] == "door_frame":
                door = doors.get(p.get("door_id"))
                if door is None:
                    errors.append(prefix + "frame has no canonical door")
                else:
                    expected = door["hinge"] + [0, door["closed_yaw"], 0] + [door["width"] / 100, 1, door["height"] / 220]
                    actual = p["location_cm"] + p["rotation_pyr"] + p["scale3"]
                    if any(abs(a - b) > EPS for a, b in zip(actual, expected)):
                        errors.append(prefix + "frame differs from canonical clear-aperture transform")
    return errors


def _main():
    import argparse
    from collections import Counter
    from pathlib import Path

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=Path("AssetsSource/CityCourier/Interiors/inventory.json"))
    parser.add_argument("--manifest", type=Path, default=Path("AssetsSource/CityCourier/InteriorArt/asset-manifest.json"))
    parser.add_argument("--output", type=Path, default=Path("AssetsSource/CityCourier/InteriorArt/sample-layout.json"))
    parser.add_argument("--validation", type=Path, default=Path("AssetsSource/CityCourier/InteriorArt/plan-validation.json"))
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    bounds = {a["name"]: a for a in manifest["assets"]}
    plan = plan_samples(inventory, bounds)
    errors = validate_plan(plan, bounds)
    report = dict(schema_version=1, passed=not errors, errors=errors,
                  validation_scope="pure_python_source_geometry_only_not_UE_runtime_or_visual_acceptance",
                  no_city_maps_written=True,
                  inventory_sha256=hashlib.sha256(args.inventory.read_bytes()).hexdigest(),
                  asset_manifest_sha256=hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
                  planner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  geometry_tolerance_cm=EPS, clearance_cm=plan["clearance_cm"],
                  checks=["Canonical compile_building record, all IDs/transforms/doors/nodes/edges unchanged",
                          "All non-roof usable rooms have blocking furniture fitted to actual exported mesh bounds",
                          "120cm central standing square and every graph edge's 34cm capsule +11cm allowance retained",
                          "Furniture avoids full closed-to-open door sweeps, including rear storage approaches",
                          "Handrails include actual base shoes and remain strictly between the original 120cm-wide flights",
                          "Non-frame trim/fixtures avoid clear door apertures and route swept AABBs",
                          "Frames follow canonical hinge/yaw/width/height; the asset generator owns the internal U-shaped hole",
                          "Blank plates; no numeric per-room text actors; no decorative closed door leaves"], samples=[])
    for sample in plan["samples"]:
        report["samples"].append(dict(building_id=sample["building_id"], kind=sample["kind"],
                                      room_count=len(sample["building"]["rooms"]),
                                      furnished_room_count=sum(r["kind"] in FURNITURE for r in sample["building"]["rooms"]),
                                      door_count=len(sample["building"]["doors"]),
                                      route_node_count=len(sample["building"]["nodes"]),
                                      placement_count=len(sample["placements"]),
                                      placement_categories=dict(sorted(Counter(p["category"] for p in sample["placements"]).items())),
                                      preview_room_ids=sample["preview_room_ids"]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.validation.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    args.validation.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": not errors, "samples": [{"building_id": s["building_id"], "placement_count": s["placement_count"]} for s in report["samples"]]}, indent=2))


if __name__ == "__main__":
    _main()
