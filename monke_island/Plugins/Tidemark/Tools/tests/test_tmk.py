#!/usr/bin/env python3
"""Tests of Tools/tmk.py: round trip, sha256, source hash, manifest accepted by scripts/tidemark/check_credits.py.

Run from the repository root: ``python3 -I monke_island/Plugins/Tidemark/Tools/tests/test_tmk.py``
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

TOOLS = Path(__file__).resolve().parents[1]


def find_repo_root(start: Path) -> Path:
    """Remonte jusqu'au dossier qui contient scripts/tidemark/check_credits.py (Tapao ou le dépôt du plugin)."""
    for candidate in [start, *start.parents]:
        if (candidate / "scripts" / "tidemark" / "check_credits.py").is_file():
            return candidate
    raise FileNotFoundError("scripts/tidemark/check_credits.py introuvable au-dessus de " + str(start))


REPO = find_repo_root(TOOLS)
sys.path.insert(0, str(TOOLS))
import tmk  # noqa: E402

CREDITS = [{"producer": "Tidemark test", "license": "CC0 1.0", "url": "https://example.invalid/test", "date": "2026-10-23"}]


def load_check_credits():
    path = REPO / "scripts" / "tidemark" / "check_credits.py"
    spec = importlib.util.spec_from_file_location("check_credits", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sample_grid() -> tuple[np.ndarray, np.ndarray]:
    elevation = np.arange(12, dtype=np.int16).reshape(3, 4) * 100 - 300  # -300 .. 800 cm
    flags = np.zeros((3, 4), dtype=np.uint8)
    flags[0, :] |= tmk.FLAG_BORDER
    flags[1, 0] |= tmk.FLAG_OPEN_SEA
    flags[2, 3] |= tmk.FLAG_FORBIDDEN
    return elevation, flags


class TestHashes(unittest.TestCase):
    def test_fnv1a64_reference(self):
        self.assertEqual(tmk.fnv1a64(b""), 0xCBF29CE484222325)
        self.assertEqual(tmk.fnv1a64(b"Tidemark"), 0xC415B2835DD67644)

    def test_source_hash_reference(self):
        # Reference vector shared with the C++ spec Tidemark.Grid.Hash ("matches the reference value"):
        # origin (1000, -500), cell 400 cm, 2 x 2, elevation [10, -20, 300, 32767], flags [0, 1, 2, 4].
        elevation = np.array([[10, -20], [300, 32767]], dtype=np.int16)
        flags = np.array([[0, 1], [2, 4]], dtype=np.uint8)
        expected_bytes = struct.pack("<ddfii", 1000.0, -500.0, 400.0, 2, 2) + elevation.astype("<i2").tobytes() + flags.tobytes()
        self.assertEqual(tmk.source_hash((1000, -500), 400, elevation, flags), tmk.fnv1a64(expected_bytes))
        self.assertEqual(tmk.source_hash_hex((1000, -500), 400, elevation, flags), REFERENCE_SOURCE_HASH)

    def test_source_hash_changes_with_one_cell(self):
        elevation, flags = sample_grid()
        before = tmk.source_hash((0, 0), 400, elevation, flags)
        elevation[1, 1] += 1
        self.assertNotEqual(before, tmk.source_hash((0, 0), 400, elevation, flags))


class TestRoundTrip(unittest.TestCase):
    def test_save_then_load(self):
        elevation, flags = sample_grid()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "Island.tmk"
            manifest = tmk.save(folder, elevation, flags, origin_cm=(12.5, -40.0), cell_size_cm=400, credits=CREDITS, crs="EPSG:32706")
            grid, credits = tmk.load(folder)
            np.testing.assert_array_equal(grid["elevation_cm"], elevation)
            np.testing.assert_array_equal(grid["flags"], flags)
            self.assertEqual(grid["size"], (4, 3))
            self.assertEqual(grid["origin_cm"], (12.5, -40.0))
            self.assertEqual(grid["cell_size_cm"], 400.0)
            self.assertEqual(grid["crs"], "EPSG:32706")
            self.assertEqual(grid["source_hash"], manifest["source_hash"])
            self.assertEqual(credits, CREDITS)
            # Layer files are little-endian, row-major.
            raw = (folder / tmk.ELEVATION_FILE).read_bytes()
            self.assertEqual(struct.unpack("<12h", raw), tuple(int(v) for v in elevation.ravel(order="C")))

    def test_sha256_mismatch_is_detected(self):
        elevation, flags = sample_grid()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "Island.tmk"
            tmk.save(folder, elevation, flags, (0, 0), 400, CREDITS)
            data = bytearray((folder / tmk.FLAGS_FILE).read_bytes())
            data[0] ^= 0x04
            (folder / tmk.FLAGS_FILE).write_bytes(bytes(data))
            with self.assertRaisesRegex(tmk.TmkError, "sha256 mismatch"):
                tmk.load(folder)

    def test_source_hash_mismatch_is_detected(self):
        elevation, flags = sample_grid()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "Island.tmk"
            tmk.save(folder, elevation, flags, (0, 0), 400, CREDITS)
            manifest = json.loads((folder / tmk.MANIFEST).read_text(encoding="utf-8"))
            manifest["source_hash"] = "0" * 16
            (folder / tmk.MANIFEST).write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(tmk.TmkError, "source_hash"):
                tmk.load(folder)

    def test_missing_manifest_fails_cleanly(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(tmk.TmkError, "missing"):
                tmk.load(Path(tmp) / "Nope.tmk")

    def test_wrong_size_is_detected(self):
        elevation, flags = sample_grid()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "Island.tmk"
            tmk.save(folder, elevation, flags, (0, 0), 400, CREDITS)
            (folder / tmk.ELEVATION_FILE).write_bytes(b"\x00" * 10)
            with self.assertRaisesRegex(tmk.TmkError, "bytes, expected"):
                tmk.load(folder)


class TestManifest(unittest.TestCase):
    def test_save_refuses_incomplete_credits(self):
        elevation, flags = sample_grid()
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(tmk.TmkError, "credits"):
                tmk.save(Path(tmp) / "A.tmk", elevation, flags, (0, 0), 400, [])
            with self.assertRaisesRegex(tmk.TmkError, r"credits\[0\]\.date"):
                tmk.save(Path(tmp) / "B.tmk", elevation, flags, (0, 0), 400, [{"producer": "x", "license": "y", "url": "z", "date": ""}])

    def test_manifest_keys_and_check_credits(self):
        elevation, flags = sample_grid()
        check_credits = load_check_credits()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "Island.tmk"
            tmk.save(folder, elevation, flags, (0, 0), 400, CREDITS)
            manifest = json.loads((folder / tmk.MANIFEST).read_text(encoding="utf-8"))
            for key in ("schema_version", "origin_cm", "cell_size_cm", "size", "layers", "source_hash", "credits"):
                self.assertIn(key, manifest)
            self.assertEqual([layer["name"] for layer in manifest["layers"]], ["elevation_cm", "flags"])
            self.assertEqual([layer["type"] for layer in manifest["layers"]], ["int16", "uint8"])
            for layer in manifest["layers"]:
                self.assertRegex(layer["sha256"], r"^[0-9a-f]{64}$")
            violations: list[str] = []
            check_credits.check_manifest(folder / tmk.MANIFEST, Path(tmp), violations)
            self.assertEqual(violations, [])

    def test_info_lists_dimensions_and_credits(self):
        elevation, flags = sample_grid()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "Island.tmk"
            tmk.save(folder, elevation, flags, (0, 0), 400, CREDITS)
            text = tmk.info(folder)
            self.assertIn("size: 4 x 3 cells", text)
            self.assertIn("min -300", text)
            self.assertIn("max 800", text)
            self.assertIn("Tidemark test", text)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(tmk.main(["info", str(folder)]), 0)


# Frozen reference, pasted as is into TidemarkGridSpec.cpp ("matches the reference value computed by tmk.py").
REFERENCE_SOURCE_HASH = "2cc618ee4cf82873"


if __name__ == "__main__":
    unittest.main()
