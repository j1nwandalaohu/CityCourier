"""Filesystem conflict/closure tests; does not start Unreal or build C++."""
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[2] / "Content/Python"
sys.path.insert(0, str(SCRIPTS))


class MigrationTests(unittest.TestCase):
    def test_receipt_falls_back_to_tracked_without_overriding_local(self):
        from epic_character_sample_paths import read_receipt
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            local, tracked = root / "Saved", root / "docs"
            tracked.mkdir()
            (tracked / "generation.json").write_text('{"version": 1}', encoding="utf-8")
            self.assertEqual(read_receipt("generation.json", local, tracked), {"version": 1})
            self.assertFalse(local.exists())
            local.mkdir()
            (local / "generation.json").write_text('{"version": 2}', encoding="utf-8")
            self.assertEqual(read_receipt("generation.json", local, tracked), {"version": 2})
            (local / "generation.json").write_text("invalid", encoding="utf-8")
            with self.assertRaises(ValueError):
                read_receipt("generation.json", local, tracked)

    def test_migration_preflight_exists(self):
        self.assertTrue((SCRIPTS / "migrate_epic_character_samples.py").exists(),
                        "Missing migration preflight implementation")

    def test_conflicts_are_all_detected_before_writing(self):
        from migrate_epic_character_samples import preflight
        import hashlib
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "A.uasset").write_bytes(b"manual")
            entries = [{"package": "/Game/A", "sha256": hashlib.sha256(b"source").hexdigest()}]
            with self.assertRaisesRegex(RuntimeError, "Conflict"):
                preflight(entries, root)
            self.assertEqual((root / "A.uasset").read_bytes(), b"manual")

    def test_identical_rerun_has_no_changes(self):
        from migrate_epic_character_samples import preflight
        import hashlib
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "A.uasset").write_bytes(b"same")
            entries = [{"package": "/Game/A", "sha256": hashlib.sha256(b"same").hexdigest()}]
            self.assertEqual(preflight(entries, root), [])

    def test_path_escape_is_rejected(self):
        from epic_character_sample_paths import package_file
        with self.assertRaises(ValueError):
            package_file("Content", "/Game/../secret")

    def test_resaved_package_requires_matching_receipt_and_source(self):
        from migrate_epic_character_samples import preflight, sha256
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            target = root / "A.uasset"
            target.write_bytes(b"UE-resaved")
            entry = {"package": "/Game/A", "sha256": "source-digest"}
            receipt = {"/Game/A": {"sha256": "source-digest", "destination_sha256": sha256(target)}}
            self.assertEqual(preflight([entry], root, receipt), [])
            target.write_bytes(b"artist-edited")
            with self.assertRaises(RuntimeError):
                preflight([entry], root, receipt)


if __name__ == "__main__":
    unittest.main()
