"""Protect redistributed source licenses and imported surface/channel contracts."""
import copy
import hashlib
from pathlib import Path
import tempfile
import unittest

from tools.art.realism_sources import validate_sources


class RealisticSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root/'surface.jpg').write_bytes(b'verified provider source')
        self.record = dict(schema_version=1,assets=[dict(id='sample',license='CC0-1.0',
            source='https://polyhaven.com/a/sample',files=[dict(path='surface.jpg',
                sha256=hashlib.sha256(b'verified provider source').hexdigest())])])

    def test_modified_download_is_rejected_before_import(self):
        self.assertEqual(validate_sources(self.record,self.root),[])
        (self.root/'surface.jpg').write_bytes(b'wrong texture with the same filename')
        self.assertTrue(validate_sources(self.record,self.root))

    def test_undeclared_license_and_escaping_path_are_rejected(self):
        bad = copy.deepcopy(self.record)
        bad['assets'][0]['license'] = 'free download'
        self.assertTrue(validate_sources(bad,self.root))
        bad = copy.deepcopy(self.record)
        bad['assets'][0]['files'][0]['path'] = '../surface.jpg'
        self.assertTrue(validate_sources(bad,self.root))

    def test_attribution_must_travel_with_cc_by_sources(self):
        self.record['assets'][0]['license'] = 'CC-BY-4.0'
        self.assertTrue(validate_sources(self.record,self.root))
        self.record['assets'][0]['attribution'] = 'Sample Artist, CC BY 4.0, adapted for CityCourier'
        self.assertEqual(validate_sources(self.record,self.root),[])


if __name__=='__main__':
    unittest.main()
