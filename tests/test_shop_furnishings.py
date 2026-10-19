"""Shop identities must survive the hollow-building conversion safely."""
import copy
import json
from pathlib import Path
import unittest
from tools.interiors import finish_kit
from tools.interiors.layout import compile_building

ROOT = Path(__file__).resolve().parents[1]
PRIMARY = ('ShopNoodleCooking', 'ShopGroceryCheckout', 'ShopRoastDisplay')
SECONDARY = ('ShopNoodleDining', 'ShopGroceryShelf', 'ShopRoastDining')


class ShopFurnishingTests(unittest.TestCase):
    def test_street_faces_have_real_display_openings(self):
        for seed in self.inventory['buildings']:
            if seed['id'] not in ('B_Merchant_0','B_Merchant_1','B_Merchant_2','B_1_0_0_0_0'):
                continue
            b = compile_building(seed)
            shops = [r for r in b['rooms'] if r['floor']==1 and r['kind']=='shop']
            glass = [x for x in b['boxes'] if x['kind']=='shop_glass']
            self.assertEqual(len(glass),len(shops))
            for r in shops:
                pane = next(x for x in glass if x['id']==r['id']+'_ShopGlass')
                self.assertEqual(pane['material'],'M_ShopGlass')
                self.assertGreater(pane['size'][0],200)
                point = [pane['center'][0],pane['center'][1],r['center'][2]+150]
                for wall in b['boxes']:
                    if wall['kind']=='wall':
                        self.assertFalse(all(abs(point[i]-wall['center'][i])<wall['size'][i]/2 for i in range(3)),wall['id'])

    @classmethod
    def setUpClass(cls):
        cls.inventory = json.loads((ROOT/'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
        manifest = json.loads((ROOT/'AssetsSource/CityCourier/InteriorArt/asset-manifest.json').read_text(encoding='utf-8'))
        cls.bounds = {a['name']: a for a in manifest['assets']}
        # Real export envelopes replace these before the full-city acceptance.
        for name in PRIMARY+SECONDARY:
            cls.bounds.setdefault('SM_IF_'+name, dict(minimum_cm=[-75,-25,0], maximum_cm=[75,25,230]))

    def test_first_floor_shops_keep_style_and_every_clearance(self):
        seeds = [b for b in self.inventory['buildings'] if b['id'] in
                 ('B_Merchant_0','B_Merchant_1','B_Merchant_2','B_Merchant_3','B_1_0_0_0_0','B_0_1_0_0_0')]
        plan = finish_kit.plan_city(dict(buildings=seeds), self.bounds)
        for sample in plan['samples']:
            variant = sample['building']['variant']
            for room in sample['building']['rooms']:
                furniture = [p for p in sample['placements'] if p.get('room_id')==room['id'] and p['category']=='furniture']
                if room['floor']==1 and room['kind']=='shop':
                    self.assertIn('SM_IF_'+PRIMARY[variant], {p['asset'] for p in furniture}, room['id'])
                    self.assertNotIn('SM_IF_Counter', {p['asset'] for p in furniture})
                    self.assertTrue(all(p['collision']=='BlockAll' for p in furniture))
                    primary = next(p for p in furniture if p['asset']=='SM_IF_'+PRIMARY[variant])
                    self.assertEqual(primary['rotation_pyr'][1]%360,0,room['id']+' must face the street')
                elif room['floor']>1:
                    self.assertFalse(any(p['asset'].startswith('SM_IF_Shop') for p in furniture))
        self.assertEqual(finish_kit.validate_plan(plan,self.bounds), [])
        # The validator must still reject a shop display covering a real door.
        broken = copy.deepcopy(plan)
        sample = broken['samples'][0]
        p = next(p for p in sample['placements'] if p['asset'].startswith('SM_IF_Shop'))
        door = next(d for d in sample['building']['doors'] if d['to_room']==p['room_id'])
        p['location_cm'] = door['hinge'][:]
        self.assertTrue(finish_kit.validate_plan(broken,self.bounds))


if __name__ == '__main__':
    unittest.main()
