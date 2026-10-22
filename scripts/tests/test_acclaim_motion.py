import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/art'))


class AcclaimMotionTests(unittest.TestCase):
    def test_downloaded_getups_have_complete_contiguous_channels(self):
        from acclaim_motion import load_asf, load_amc
        folder = ROOT / 'AssetsSource/CityCourier/External/CombatRecovery/Downloads/Direct'
        skeleton = load_asf(folder / '140.asf')
        for name, count in [('140_01.amc', 830), ('140_08.amc', 898)]:
            self.assertEqual(len(load_amc(folder / name, skeleton)), count)

    def test_truncated_frame_is_rejected(self):
        from acclaim_motion import parse_amc
        with self.assertRaisesRegex(ValueError, 'channels'):
            parse_amc(':DEGREES\n1\nroot 0 0 0 0 0 0\n', {'root': 6, 'leg': 1})

    def test_invalid_numbers_and_frame_gaps_are_rejected(self):
        from acclaim_motion import parse_amc
        for data in ['1\nroot nan 0 0 0 0 0\n', '2\nroot 0 0 0 0 0 0\n']:
            with self.assertRaises(ValueError):
                parse_amc(data, {'root': 6})


if __name__ == '__main__':
    unittest.main()
