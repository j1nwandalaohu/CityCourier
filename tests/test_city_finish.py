"""City rollout must include the one-storey station and preserve sample layouts."""
import json
from pathlib import Path
import unittest
from tools.interiors import finish_kit as finish

ROOT = Path(__file__).resolve().parents[1]


class CityFinishTests(unittest.TestCase):
    def test_rollout_covers_annex_and_keeps_approved_sample(self):
        inventory = json.loads((ROOT/'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
        manifest = json.loads((ROOT/'AssetsSource/CityCourier/InteriorArt/asset-manifest.json').read_text(encoding='utf-8'))
        bounds = {a['name']: a for a in manifest['assets']}
        ids = {'B_Station', 'B_Merchant_0'}
        selected = {**inventory, 'buildings': [b for b in inventory['buildings'] if b['id'] in ids]}
        self.assertTrue(callable(getattr(finish, 'plan_city', None)), 'City-wide planner must exist')
        plan = finish.plan_city(selected, bounds)
        self.assertEqual({s['building_id'] for s in plan['samples']}, ids)
        self.assertEqual(finish.validate_plan(plan, bounds), [])
        station = next(s for s in plan['samples'] if s['building_id'] == 'B_Station')
        for room in station['building']['rooms']:
            placements = [p for p in station['placements'] if p.get('room_id') == room['id']]
            self.assertTrue(any(p['category'] == 'light_fixture' for p in placements), room['id'])
            if room['kind'] in finish.FURNITURE:
                self.assertTrue(any(p['category'] == 'furniture' for p in placements), room['id'])
        old = json.loads((ROOT/'AssetsSource/CityCourier/InteriorArt/sample-layout.json').read_text(encoding='utf-8'))
        merchant = next(s for s in plan['samples'] if s['building_id'] == 'B_Merchant_0')
        self.assertEqual(merchant, next(s for s in old['samples'] if s['building_id'] == 'B_Merchant_0'))


if __name__ == '__main__':
    unittest.main()
