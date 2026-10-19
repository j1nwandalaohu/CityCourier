"""Geometry and identity invariants for the actual production-size city layouts."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools/interiors/layout.py"
if MODULE.exists():
    spec = importlib.util.spec_from_file_location("courier_interior_layout", MODULE)
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
else:
    layout = None


class InteriorLayoutTests(unittest.TestCase):
    def test_wide_stairs_and_deep_turns_in_every_variant(self):
        for variant in (0,1,2):
            b = self.make(variant=variant)
            flights = [x for x in b['boxes'] if x['kind']=='step' and any(t in x['id'] for t in ('_up_','_return_'))]
            self.assertTrue(flights)
            self.assertTrue(all(x['size'][0]>=160 for x in flights))
            turns = [x for x in b['boxes'] if x['id'].endswith('_half_landing')]
            self.assertTrue(all(x['size'][1]>=180 for x in turns))
            for step in flights:
                self.assertEqual(step['size'][1],28)
                self.assertEqual(step['size'][2],20)
                self.assertGreater(abs(step['center'][0])-step['size'][0]/2,4)
            nodes = {n['id']:n for n in b['nodes']}
            for door in b['doors']:
                if door['id'].endswith('Store_Door'):
                    a,c = nodes[door['node_a']]['position'],nodes[door['node_b']]['position']
                    self.assertAlmostEqual(a[0],door['hinge'][0]+door['width']/2,delta=1e-6)
                    self.assertAlmostEqual(a[0],c[0])
                    self.assertLess(a[1],door['hinge'][1])
                    self.assertGreater(c[1],door['hinge'][1])

    def make(self, **overrides):
        self.assertIsNotNone(layout, "The canonical enterable-building layout compiler is not implemented")
        seed = dict(id="B_0_0_0_0_0", width=788.333333, depth=2300.0,
                    variant=0, kind="residential", location=[1000, -12000, 0], yaw=180)
        seed.update(overrides)
        return layout.compile_building(seed)

    def test_all_visual_variants_and_production_widths(self):
        for variant, floor_count in [(0, 5), (1, 4), (2, 6)]:
            for width in [788.333333, 1153.333333]:
                for yaw in [0, 180]:
                    with self.subTest(variant=variant, width=width, yaw=yaw):
                        b = self.make(variant=variant, width=width, yaw=yaw)
                        self.assertEqual(layout.validate_building(b), [])
                        self.assertEqual(len([f for f in b["floors"] if f["kind"] != "roof"]), floor_count)
                        self.assertTrue(any(f["kind"] == "roof" for f in b["floors"]))
                        self.assertGreater(len(b["rooms"]), floor_count * 5)
                        self.assertTrue(all(d["width"] >= (120 if d["external"] else 100) for d in b["doors"]))
                        self.assertTrue(all(d["height"] >= 220 for d in b["doors"]))

    def test_every_room_has_a_real_bidirectional_path_from_the_street(self):
        b = self.make(variant=2)
        graph = {n["id"]: set() for n in b["nodes"]}
        for edge in b["edges"]:
            graph[edge["from"]].add(edge["to"])
            graph[edge["to"]].add(edge["from"])
        seen = set()
        todo = [b["entrance_node"]]
        while todo:
            node = todo.pop()
            if node not in seen:
                seen.add(node)
                todo.extend(graph[node] - seen)
        self.assertEqual(seen, set(graph))
        for room in b["rooms"]:
            self.assertIn(room["node"], seen, room["id"])
        self.assertEqual(len(b["expected_room_ids"]), len(b["rooms"]))

    def test_steps_fit_character_and_connect_exact_floor_heights(self):
        b = self.make(variant=2)
        stairs = [e for e in b["edges"] if e["kind"] == "stairs"]
        self.assertTrue(stairs)
        nodes = {n["id"]: n for n in b["nodes"]}
        for edge in stairs:
            a, z = nodes[edge["from"]]["position"], nodes[edge["to"]]["position"]
            self.assertLessEqual(abs(a[2] - z[2]), 17.00001)
            self.assertGreaterEqual(math.dist(a[:2], z[:2]), 27.99999)
        for box in b["boxes"]:
            self.assertTrue(all(math.isfinite(v) and v > 0 for v in box["size"]))

    def test_shallower_production_pickup_shops_have_all_upper_floors(self):
        b = self.make(width=800, depth=1150, kind="shop", variant=2)
        self.assertEqual(layout.validate_building(b), [])
        self.assertEqual(len(b["floors"]), 7)  # six actual floors and reachable roof
        for r in b["rooms"]:
            if r["kind"] not in ("stairs", "hall", "roof"):
                self.assertGreaterEqual(r["maximum"][1] - r["minimum"][1], 120)

    def test_one_story_station_annex_is_fitted_without_inventing_upper_floors(self):
        b = self.make(width=650, depth=500, kind="station", storeys=1, height=400)
        self.assertEqual(layout.validate_building(b), [])
        self.assertEqual(len(b["floors"]), 1)
        self.assertTrue(all(r["floor"] == 1 for r in b["rooms"]))
        self.assertTrue(any(r["kind"] == "service" for r in b["rooms"]))
        self.assertTrue(any(r["kind"] == "bathroom" for r in b["rooms"]))

    def test_rebuilds_are_deterministic_and_transforms_do_not_change_ids(self):
        a = self.make()
        self.assertEqual(json.dumps(a, sort_keys=True), json.dumps(self.make(), sort_keys=True))
        changed = self.make(location=[-24000, 9000, 0], yaw=90)
        self.assertEqual(a["expected_room_ids"], changed["expected_room_ids"])
        self.assertEqual([d["id"] for d in a["doors"]], [d["id"] for d in changed["doors"]])

    def test_room_centers_are_clear_of_structural_boxes(self):
        b = self.make()
        for room in b["rooms"]:
            if room["kind"] in ("stairs", "hall", "roof"):
                continue
            point = room["center"]
            # A 68 cm-wide, 176 cm-tall courier must fit at the room's standing area.
            for box in b["boxes"]:
                if box["kind"] in ("floor", "step", "rail"):
                    continue
                center, size = box["center"], box["size"]
                overlap_xy = all(abs(point[i] - center[i]) < size[i] / 2 + 34 for i in [0, 1])
                overlap_z = point[2] + 176 > center[2] - size[2] / 2 and point[2] + 2 < center[2] + size[2] / 2
                self.assertFalse(overlap_xy and overlap_z, (room["id"], box["id"]))

    def test_invalid_geometry_and_non_finite_input_are_rejected(self):
        self.make()
        for override in [dict(width=300), dict(depth=800), dict(width=float("nan")),
                         dict(location=[0, float("inf"), 0]), dict(variant=8), dict(id="")]:
            with self.subTest(override=override):
                with self.assertRaises(ValueError):
                    self.make(**override)

    def test_validator_cannot_reduce_expected_totals_to_hide_missing_rooms(self):
        b = self.make()
        broken = copy.deepcopy(b)
        broken["rooms"].pop()
        self.assertTrue(layout.validate_building(broken))
        broken = copy.deepcopy(b)
        broken["rooms"][1]["id"] = broken["rooms"][0]["id"]
        self.assertTrue(layout.validate_building(broken))
        broken = copy.deepcopy(b)
        broken["floors"].pop(1)
        self.assertTrue(layout.validate_building(broken))

    def test_door_addresses_and_room_standing_areas_are_valid(self):
        b = self.make()
        rooms = {r["id"]: r for r in b["rooms"]}
        for door in b["doors"]:
            for room in [door["from_room"], door["to_room"]]:
                self.assertTrue(room == "Outside" or room in rooms, (door["id"], room))
        for room in rooms.values():
            if room["kind"] not in ("stairs", "hall", "roof"):
                self.assertTrue(all(room["maximum"][i] - room["minimum"][i] >= 120 for i in [0, 1]))

    def test_actual_source_inventory_compiles_every_building_and_bound_room(self):
        self.assertIsNotNone(layout)
        source = json.loads((ROOT / "AssetsSource/CityCourier/Interiors/inventory.json").read_text(encoding="utf-8"))
        self.assertEqual(len(source["expected_source_tags"]), 288)
        self.assertEqual(len(source["bindings"]), 13)
        self.assertEqual(len(source["gateways"]), 8)
        compiled = {seed["id"]: layout.compile_building(seed) for seed in source["buildings"]}
        self.assertEqual(len(compiled), len(source["buildings"]))
        self.assertEqual(set(compiled), set(source["expected_building_ids"]))
        self.assertEqual(len(compiled), 293)
        for binding in source["bindings"]:
            room = binding["building_id"] + "_" + binding["room_suffix"]
            self.assertIn(room, compiled[binding["building_id"]]["expected_room_ids"])

    def test_all_visible_external_doors_have_real_threshold_steps(self):
        b = self.make()
        for prefix in ["Entry_step_", "Side_step_-1_", "Side_step_1_", "Rear_step_"]:
            steps = [box for box in b["boxes"] if box["id"].split(b["id"] + "_", 1)[-1].startswith(prefix)]
            self.assertEqual(len(steps), 3, prefix)
            self.assertAlmostEqual(max(box["center"][2] + box["size"][2] / 2 for box in steps), 40)

    def test_registered_infill_buildings_do_not_overlap_existing_houses(self):
        source = json.loads((ROOT / "AssetsSource/CityCourier/Interiors/inventory.json").read_text(encoding="utf-8"))
        def bounds(b):
            angle = math.radians(b["yaw"])
            half_x = (abs(math.cos(angle))*b["width"] + abs(math.sin(angle))*b["depth"])/2
            half_y = (abs(math.sin(angle))*b["width"] + abs(math.cos(angle))*b["depth"])/2
            return (b["location"][0]-half_x,b["location"][0]+half_x,b["location"][1]-half_y,b["location"][1]+half_y)
        streets = [b for b in source["buildings"] if b["id"] not in {"B_Station", *[f"B_Merchant_{i}" for i in range(4)]}]
        for annex in [b for b in source["buildings"] if b not in streets]:
            a = bounds(annex)
            for building in streets:
                c = bounds(building)
                overlap = min(a[1], c[1])-max(a[0], c[0]) > .01 and min(a[3], c[3])-max(a[2], c[2]) > .01
                self.assertFalse(overlap, (annex["id"], building["id"]))


if __name__ == "__main__":
    unittest.main()
