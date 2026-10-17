"""Tests of the safe local SteamPipe manifest generator. No Steamworks access needed."""
from pathlib import Path
import tempfile
import unittest

from make_steampipe_vdf import generate, numeric_id, validate_package


class SteamPipeGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "package"
        self.root.mkdir()
        (self.root / "monke_island.exe").write_bytes(b"TEST-EXE")
        cook = self.root / "monke_island" / "Content" / "Paks"
        cook.mkdir(parents=True)
        (cook / "monke_island-Windows.utoc").write_bytes(b"TEST-COOKED")

    def test_real_ids_only(self):
        self.assertEqual(numeric_id("123456", "AppID"), "123456")
        for bad in ("480", "", "0", "ABC", "123", "../123456"):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    numeric_id(bad, "AppID")

    def test_validate_package(self):
        validate_package(self.root, "monke_island.exe")
        with self.assertRaises(ValueError):
            validate_package(self.root, "../monke_island.exe")
        with self.assertRaises(ValueError):
            validate_package(self.root, "missing.exe")

    def test_refuse_credentials_and_debug_files(self):
        p = self.root / "service-account.json"
        p.write_text("{}", encoding="utf-8")
        with self.assertRaises(ValueError):
            validate_package(self.root, "monke_island.exe")
        p.unlink()
        p = self.root / "monke_island.pdb"
        p.write_bytes(b"debug")
        with self.assertRaises(ValueError):
            validate_package(self.root, "monke_island.exe")

    def test_generate_safely(self):
        app, depot = generate("123456", "123457", self.root, Path(self.tmp.name) / "vdf", "monke_island.exe", "2.3.0")
        self.assertIn('"AppID" "123456"', app.read_text(encoding="utf-8"))
        self.assertIn('"123457" "depot_123457.vdf"', app.read_text(encoding="utf-8"))
        self.assertIn('"Recursive" "1"', depot.read_text(encoding="utf-8"))
        self.assertNotIn("SetLive", app.read_text(encoding="utf-8"))
        with self.assertRaises(ValueError):
            generate("123456", "123456", self.root, Path(self.tmp.name) / "vdf", "monke_island.exe", "2.3.0")
        with self.assertRaises(ValueError):
            generate("123456", "123457", self.root, self.root / "scripts", "monke_island.exe", "2.3.0")


if __name__ == "__main__":
    unittest.main()
