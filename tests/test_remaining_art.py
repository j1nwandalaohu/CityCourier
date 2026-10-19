"""The remaining kit pieces must be placed against real stairs and facade windows."""
import json
from pathlib import Path
import unittest
from tools.interiors import finish_kit

ROOT = Path(__file__).resolve().parents[1]


class RemainingArtTests(unittest.TestCase):
    def test_complete_kit_is_used_without_changing_canonical_rooms(self):
        inventory = json.loads((ROOT/'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
        manifest = json.loads((ROOT/'AssetsSource/CityCourier/InteriorArt/asset-manifest.json').read_text(encoding='utf-8'))
        bounds = {a['name']: a for a in manifest['assets']}
        selected = {**inventory, 'buildings': [b for b in inventory['buildings'] if b['id'] == 'B_Merchant_0']}
        plan = finish_kit.plan_city(selected, bounds)
        sample = plan['samples'][0]
        used = {p['asset'] for p in sample['placements']}
        for name in ('StairNosing', 'WindowReturn', 'AirconWall'):
            self.assertIn('SM_IF_' + name, used)
        self.assertEqual(finish_kit.validate_plan(plan, bounds), [])
        steps = {b['id']: b for b in sample['building']['boxes']
                 if b['kind'] == 'step' and any(t in b['id'] for t in ('_up_', '_return_'))}
        nosings = [p for p in sample['placements'] if p['asset'] == 'SM_IF_StairNosing']
        self.assertEqual(len(nosings), len(steps))
        for p in nosings:
            step = steps[p['step_id']]
            self.assertAlmostEqual(p['location_cm'][2], step['center'][2] + step['size'][2]/2 + .05)
            self.assertEqual(p['collision'], 'NoCollision')
        # A route-check exception for sub-centimeter trim must not allow a
        # floating or blocking strip to pass validation.
        nosings[0]['location_cm'][2] += 2
        self.assertTrue(any('nosing differs from real tread surface' in e
                            for e in finish_kit.validate_plan(plan, bounds)))


if __name__ == '__main__':
    unittest.main()
