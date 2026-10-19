"""Translate validated canonical layouts into the reflected cooked UE schema."""
import hashlib
import json
from pathlib import Path

from tools.interiors.layout import compile_building


def vec(v):
    return dict(zip(("x", "y", "z"), v))


def to_native_catalog(inventory):
    if {b["id"] for b in inventory["buildings"]} != set(inventory["expected_building_ids"]):
        raise ValueError("Source building inventory does not match expected coverage")
    result = dict(schemaVersion=1, expectedBuildingIds=inventory["expected_building_ids"], buildings=[], bindings=[])
    all_rooms, all_doors = set(), set()
    for seed in inventory["buildings"]:
        b = compile_building(seed)
        indices = {n["id"]: i for i, n in enumerate(b["nodes"])}
        native = dict(id=b["id"], origin=vec(b["location"]), yaw=b["yaw"],
                      size=vec([b["width"], b["depth"], b["height"]]), variant=b["variant"], kind=b["kind"],
                      sourceTag=seed.get("source_tag", ""), sourceCollisionTag=seed.get("source_collision_tag", ""),
                      entranceNode=indices[b["entrance_node"]], expectedRoomIds=b["expected_room_ids"],
                      floors=[dict(id=f["id"], number=f["number"], elevation=f["z"], bRoof=f["kind"] == "roof") for f in b["floors"]],
                      nodes=[dict(id=n["id"], position=vec(n["position"]), floor=n["floor"], roomId=n["room_id"]) for n in b["nodes"]],
                      edges=[dict(**{"from": indices[e["from"]], "to": indices[e["to"]]}, kind=e["kind"],
                                  doorId=e["door_id"], lengthMeters=e["length"] / 100) for e in b["edges"]],
                      rooms=[dict(id=r["id"], floor=r["floor"], kind=r["kind"], unit=r["unit"], label=r["label"],
                                  minimum=vec(r["minimum"]), maximum=vec(r["maximum"]), standingPoint=vec(r["center"]), routeNode=indices[r["node"]]) for r in b["rooms"]],
                      doors=[dict(id=d["id"], fromRoom=d["from_room"], toRoom=d["to_room"], hinge=vec(d["hinge"]),
                                  closedYaw=d["closed_yaw"], openAngle=d["open_angle"], width=d["width"], height=d["height"],
                                  bExternal=d["external"], nodeA=indices[d["node_a"]], nodeB=indices[d["node_b"]]) for d in b["doors"]],
                      boxes=[dict(id=x["id"], center=vec(x["center"]), size=vec(x["size"]), yaw=x["yaw"], kind=x["kind"], material=x["material"]) for x in b["boxes"]],
                      placements=[dict(roomId=p["room_id"], kind=p["kind"], position=vec(p["center"]), yaw=p["yaw"], floor=p["floor"]) for p in b["placements"]])
        rooms, doors = {r["id"] for r in b["rooms"]}, {d["id"] for d in b["doors"]}
        if all_rooms & rooms or all_doors & doors:
            raise ValueError("Duplicate global room/door identity")
        all_rooms.update(rooms)
        all_doors.update(doors)
        result["buildings"].append(native)
    locations = set()
    for binding in inventory["bindings"]:
        room_id = binding["building_id"] + "_" + binding["room_suffix"]
        if binding["location_id"] in locations or room_id not in all_rooms:
            raise ValueError("Invalid indoor delivery binding")
        locations.add(binding["location_id"])
        result["bindings"].append(dict(locationId=binding["location_id"], buildingId=binding["building_id"],
                                       roomId=room_id, streetPosition=vec(binding["street_position"]), role=binding["role"]))
    digest = hashlib.sha256(json.dumps(inventory, sort_keys=True).encode("utf-8"))
    for source in [Path(__file__).with_name("layout.py"), Path(__file__).with_name("placement.py"), Path(__file__)]:
        digest.update(source.read_text(encoding="utf-8").replace("\r\n", "\n").encode("utf-8"))
    result["sourceDigest"] = digest.hexdigest()
    return result
