"""Imported bounds, identity and traversability contracts for sample finishes."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

from tools.interiors.layout import compile_building

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools/interiors/finish_kit.py"
spec = importlib.util.spec_from_file_location("finish_kit", MODULE) if MODULE.exists() else None
finish = importlib.util.module_from_spec(spec) if spec else None
if spec:
    spec.loader.exec_module(finish)


def kit_bounds():
    # Realistic asymmetric envelopes exercise origin correction, not assumed sizes.
    sizes = {"Bed": (79, 177, 110), "Desk": (103, 59, 124),
             "Kitchen": (124, 56, 127), "Toilet": (53, 76, 83),
             "Shelf": (124, 43, 185), "Sofa": (124, 64, 94),
             "Counter": (124, 56, 117), "LightPanel": (100, 18, 4),
             "HandrailPost": (4, 4, 90), "HandrailBar": (100, 4, 4),
             "Skirting": (100, 1.6, 10), "NumberPlate": (24, 1, 8),
             "ExitSign": (40, 2, 15), "SwitchPlate": (8.5, 1, 8.5),
             "Outlet": (8.5, 1, 8.5), "DoorFrame": (110, 8, 225),
             "DadoPanel": (100, 2.5, 110)}
    result = {}
    for name, (x, y, z) in sizes.items():
        lo = [-x / 2, -y / 2, 0]
        hi = [x / 2, y / 2, z]
        if name in ("Skirting", "HandrailBar"):
            lo, hi = [0, -y / 2, 0 if name == "Skirting" else -z / 2], [x, y / 2, z if name == "Skirting" else z / 2]
        if name == "DoorFrame":
            lo, hi = [-5, -4, 0], [105, 4, 225]
        if name == "DadoPanel":
            lo, hi = [0, -.75, 0], [100, 1.75, 110]
        result["SM_IF_" + name] = {"minimum_cm": lo, "maximum_cm": hi}
    # Pivot displaced from geometric center must still land safely on the floor.
    result["SM_IF_Desk"] = {"minimum_cm": [-31, -42, -2], "maximum_cm": [72, 17, 122]}
    return result


class InteriorFinishTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(finish, "The pure Python finish planner must exist")
        self.inventory = json.loads((ROOT / "AssetsSource/CityCourier/Interiors/inventory.json").read_text(encoding="utf-8"))
        self.bounds = kit_bounds()

    def test_three_existing_samples_preserve_complete_canonical_records(self):
        original = copy.deepcopy(self.inventory)
        plan = finish.plan_samples(self.inventory, self.bounds)
        self.assertEqual(self.inventory, original)
        self.assertEqual({s["kind"] for s in plan["samples"]}, {"shop", "residential", "office"})
        seeds = {s["id"]: s for s in self.inventory["buildings"]}
        for sample in plan["samples"]:
            self.assertEqual(sample["building"], compile_building(seeds[sample["building_id"]]))
            self.assertTrue(sample["preview_room_ids"])
        self.assertEqual(plan, finish.plan_samples(self.inventory, self.bounds))

    def test_each_usable_room_has_safe_blocking_furniture_and_all_rooms_have_lights(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        self.assertEqual(finish.validate_plan(plan, self.bounds), [])
        for sample in plan["samples"]:
            for room in sample["building"]["rooms"]:
                if room["floor"] > len(sample["building"]["floors"]) - 1:
                    continue
                placed = [p for p in sample["placements"] if p.get("room_id") == room["id"]]
                self.assertTrue(any(p["asset"] == "SM_IF_LightPanel" for p in placed), room["id"])
                if room["kind"] not in ("hall", "stairs", "roof"):
                    self.assertTrue(any(p["category"] == "furniture" and p["collision"] == "BlockAll" for p in placed), room["id"])
            self.assertFalse(any(p["asset"].startswith("SM_IF_Door") and p["asset"] != "SM_IF_DoorFrame" for p in sample["placements"]))

    def test_oversized_real_furniture_is_rejected_instead_of_becoming_noncolliding(self):
        self.bounds["SM_IF_Bed"]["maximum_cm"][0] = 2000
        with self.assertRaisesRegex(ValueError, "Cannot fit.*Bed"):
            finish.plan_samples(self.inventory, self.bounds)

    def test_missing_and_nonfinite_actual_bounds_fail_closed(self):
        for invalid in ({}, {**self.bounds, "SM_IF_Bed": {"minimum_cm": [0, 0, 0], "maximum_cm": [float("nan"), 1, 1]}}):
            with self.subTest(invalid=len(invalid)), self.assertRaises(ValueError):
                finish.plan_samples(self.inventory, invalid)

    def test_validator_detects_intrusion_into_center_and_rear_store_approach(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        sample = next(s for s in plan["samples"] if s["kind"] == "residential")
        furniture = next(p for p in sample["placements"] if p["category"] == "furniture" and p["room_id"].endswith("F02_A04"))
        room = next(r for r in sample["building"]["rooms"] if r["id"] == furniture["room_id"])
        furniture["location_cm"] = room["center"][:]
        self.assertTrue(any("standing" in e for e in finish.validate_plan(plan, self.bounds)))
        approach = next(n for n in sample["building"]["nodes"] if n["id"].endswith("F02_AStore_approach"))
        furniture["location_cm"] = approach["position"][:]
        self.assertTrue(any("route" in e for e in finish.validate_plan(plan, self.bounds)))

    def test_actual_rotated_bounds_and_pivot_are_used(self):
        p = dict(location_cm=[10, 20, 30], rotation_pyr=[0, 90, 0], scale3=[1, 1, 1])
        lo, hi = finish.placement_bounds(p, self.bounds["SM_IF_Desk"])
        for actual, expected in zip(lo, [-7, -11, 28]):
            self.assertAlmostEqual(actual, expected)
        for actual, expected in zip(hi, [52, 92, 152]):
            self.assertAlmostEqual(actual, expected)

    def test_door_sweep_detects_furniture_away_from_graph_centerline(self):
        door = dict(hinge=[0, 0, 0], closed_yaw=0, open_angle=90, width=100, height=220)
        self.assertTrue(finish.intersects_door_sweep([65, 65, 0], [75, 75, 80], door))
        self.assertFalse(finish.intersects_door_sweep([-80, 65, 0], [-70, 75, 80], door))
        self.assertFalse(finish.intersects_door_sweep([65, 65, 230], [75, 75, 240], door))

    def test_rails_keep_full_tread_width_and_all_route_capsules(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        rails = [p for s in plan["samples"] for p in s["placements"] if p["category"] == "handrail"]
        self.assertTrue(rails)
        for p in rails:
            lo, hi = finish.placement_bounds(p, self.bounds[p["asset"]])
            self.assertGreaterEqual(lo[0], -25)
            self.assertLessEqual(hi[0], 25)
            self.assertEqual(p["collision"], "BlockAll")
        self.assertEqual(finish.validate_plan(plan, self.bounds), [])

    def test_skirting_segments_never_bridge_door_clear_apertures(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        count = 0
        for sample in plan["samples"]:
            for p in sample["placements"]:
                if p["category"] == "skirting":
                    count += 1
                    lo, hi = finish.placement_bounds(p, self.bounds[p["asset"]])
                    for door in sample["building"]["doors"]:
                        self.assertFalse(finish.intersects_door_aperture(lo, hi, door), (p["name"], door["id"]))
        self.assertGreater(count, 50)

    def test_thin_finishes_also_avoid_capsule_routes_and_door_openings(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        categories = set()
        for sample in plan["samples"]:
            for p in sample["placements"]:
                categories.add(p["category"])
                if p["category"] in ("door_frame", "furniture", "handrail"):
                    continue
                lo, hi = finish.placement_bounds(p, self.bounds[p["asset"]])
                self.assertFalse(finish._route_blocked(lo, hi, sample["building"]), p["name"])
                for door in sample["building"]["doors"]:
                    self.assertFalse(finish.intersects_door_aperture(lo, hi, door), p["name"])
        self.assertTrue({"number_plate", "switch", "outlet", "exit_sign"}.issubset(categories))

    def test_serialized_frame_cannot_be_shifted_across_clear_aperture(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        frame = next(p for p in plan["samples"][0]["placements"] if p["category"] == "door_frame")
        frame["location_cm"][0] += 20
        self.assertTrue(any("frame" in e and "canonical" in e for e in finish.validate_plan(plan, self.bounds)))

    def test_real_export_float_precision_and_post_base_do_not_change_clearance(self):
        self.bounds["SM_IF_Bed"] = {"minimum_cm": [-39.5000011, -86.500001, 0],
                                     "maximum_cm": [39.5000011, 88.2500052, 108.9999914]}
        # Actual handrail post includes a seven centimeter base shoe.
        self.bounds["SM_IF_HandrailPost"] = {"minimum_cm": [-3.50000001, -3.50000001, 0],
                                             "maximum_cm": [3.50000001, 3.50000001, 90]}
        plan = finish.plan_samples(self.inventory, self.bounds)
        self.assertEqual(finish.validate_plan(plan, self.bounds), [])
        for sample in plan["samples"]:
            for p in sample["placements"]:
                if p["category"] == "handrail":
                    lo, hi = finish.placement_bounds(p, self.bounds[p["asset"]])
                    self.assertGreaterEqual(lo[0], -25)
                    self.assertLessEqual(hi[0], 25)

    def test_validator_detects_edited_canonical_route_anchor(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        plan["samples"][0]["building"]["nodes"][0]["position"][0] += 1
        self.assertTrue(any("canonical" in e for e in finish.validate_plan(plan, self.bounds)))

    def test_dado_uses_only_public_wall_fragments_and_excludes_door_spans(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        count = 0
        for sample in plan["samples"]:
            rooms = {r["id"]: r for r in sample["building"]["rooms"]}
            for p in sample["placements"]:
                if p["category"] != "dado":
                    continue
                count += 1
                self.assertIn(rooms[p["room_id"]]["kind"], ("hall", "stairs"))
                lo, hi = finish.placement_bounds(p, self.bounds[p["asset"]])
                self.assertFalse(finish._route_blocked(lo, hi, sample["building"]))
                self.assertFalse(any(finish.intersects_door_aperture(lo, hi, door) for door in sample["building"]["doors"]))
        self.assertGreater(count, 30)

    def test_reusable_assemblies_have_distinct_safe_furniture(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        for sample in plan["samples"]:
            furniture = [p for p in sample["placements"] if p["category"] == "furniture"]
            by_room = {}
            for p in furniture:
                by_room.setdefault(p["room_id"], []).append(p)
            self.assertTrue(any(len(group) > 1 for group in by_room.values()))
        self.assertEqual(finish.validate_plan(plan, self.bounds), [])
        sample = plan["samples"][0]
        group = next([p for p in sample["placements"] if p.get("room_id") == room["id"] and p["category"] == "furniture"]
                     for room in sample["building"]["rooms"]
                     if sum(p.get("room_id") == room["id"] and p["category"] == "furniture" for p in sample["placements"]) > 1)
        group[1]["location_cm"] = group[0]["location_cm"][:]
        self.assertTrue(any("assembly overlaps" in error for error in finish.validate_plan(plan, self.bounds)))

    def test_half_landings_have_real_wall_lights_clear_of_headroom(self):
        plan = finish.plan_samples(self.inventory, self.bounds)
        for sample in plan["samples"]:
            b = sample["building"]
            for room in b["rooms"]:
                if room["kind"] != "stairs" or room["floor"] == len(b["floors"]):
                    continue
                p = next(p for p in sample["placements"] if p.get("room_id") == room["id"] and p.get("mount") == "rear_wall_above_half_landing")
                landing = next(box for box in b["boxes"] if box["id"] == b["id"] + "_F%02d_half_landing" % room["floor"])
                lo, hi = finish.placement_bounds(p, self.bounds[p["asset"]])
                self.assertGreater(lo[2] - landing["center"][2] - landing["size"][2]/2, 200)
                self.assertLessEqual(hi[1], room["maximum"][1])
                self.assertEqual(p["rotation_pyr"], [0,0,90])
        self.assertEqual(finish.validate_plan(plan, self.bounds), [])


if __name__ == "__main__":
    unittest.main()
