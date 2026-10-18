import unittest
import json
from pathlib import Path
from tools.interiors.layout import compile_building
from tools.interiors.entrance_clearance import overlaps, relocate_props, entry_zones


class EntranceClearanceTests(unittest.TestCase):
    def test_all_production_external_doors_and_observed_planter(self):
        inventory=json.loads((Path(__file__).resolve().parents[1]/'AssetsSource/CityCourier/Interiors/inventory.json').read_text(encoding='utf-8'))
        zones,footprints=entry_zones([compile_building(b) for b in inventory['buildings']])
        self.assertEqual(len(zones),881)
        self.assertEqual(len(footprints),293)
        door=next(z for z in zones if z['id']=='B_1_3_0_1_0_Entry_Door')
        self.assertTrue(overlaps(dict(minimum=(2910,-11140,0),maximum=(3090,-10960,250)),door))
        self.assertFalse(overlaps(dict(minimum=(3160,-11240,0),maximum=(3340,-11060,250)),door))

    def test_blocking_tree_moves_but_keeps_collision_and_other_props(self):
        zones=[dict(id='front',minimum=(-105,-250,30),maximum=(105,25,270),along=(1,0),outward=(0,-1))]
        props=[dict(id='tree',position=(0,0,0),minimum=(-130,-130,0),maximum=(130,130,500),tree=True),
               dict(id='bench',position=(700,-300,0),minimum=(650,-325,0),maximum=(750,-275,100),tree=False)]
        moved,changes=relocate_props(props,zones,[(-400,0,400,1000)], [((-1000,-950),(1000,-950))])
        self.assertEqual([c['id'] for c in changes],['tree'])
        self.assertEqual(moved[1],props[1])
        self.assertEqual(moved[0]['position'][2],0)
        self.assertFalse(overlaps(moved[0],zones[0]))
        self.assertEqual(relocate_props(moved,zones,[(-400,0,400,1000)],[((-1000,-950),(1000,-950))])[1],[])

    def test_no_safe_location_fails_instead_of_deleting_a_prop(self):
        zone=dict(id='front',minimum=(-105,-250,30),maximum=(105,25,270),along=(1,0),outward=(0,-1))
        prop=dict(id='tree',position=(0,0,0),minimum=(-130,-130,0),maximum=(130,130,500),tree=True)
        with self.assertRaises(ValueError):
            relocate_props([prop],[zone],[(-10000,-10000,10000,10000)],[])


if __name__=='__main__': unittest.main()
