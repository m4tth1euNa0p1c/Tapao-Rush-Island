import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("build_metadata", Path(__file__).resolve().parents[1] / "build_metadata.py")
metadata = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(metadata)


class BuildMetadataTests(unittest.TestCase):
    def test_ini_preserves_arrays_comments_and_other_sections(self):
        original = "; keep\r\n[Other]\r\n+Maps=A\r\n+Maps=B\r\n[Version]\r\nProjectVersion=old\r\n[End]\r\nX=1\r\n"
        result = metadata.set_ini_value(original, "Version", "ProjectVersion", "1.2.3")
        self.assertEqual(result, original.replace("ProjectVersion=old", "ProjectVersion=1.2.3"))
        self.assertEqual(metadata.set_ini_value(result, "Version", "ProjectVersion", "1.2.3"), result)

    def test_duplicate_key_or_section_rejected(self):
        for text in ("[V]\nX=1\nX=2\n", "[V]\nX=1\n[V]\n"):
            with self.assertRaises(ValueError):
                metadata.set_ini_value(text, "V", "X", "3")

    def setup_build(self, directory):
        root = Path(directory)
        project = root / "monke_island"
        (project / "Config").mkdir(parents=True)
        (project / "Config/DefaultGame.ini").write_text("ProjectVersion=1.2.3\n")
        (project / "monke_island.uproject").write_text("{}")
        binary = project / "Binaries/Win64"
        binary.mkdir(parents=True)
        (binary / "monke_island.exe").write_bytes(b"test-exe")
        (binary / "monke_island.pdb").write_bytes(b"test-pdb")
        saved = project / "Saved/BuildMetadata"
        saved.mkdir(parents=True)
        (saved / "build.json").write_text(json.dumps({"version": "1.2.3", "source_config_sha256": metadata.source_digest(root)}))
        package = root / "build/package"
        package.mkdir(parents=True)
        (package / "monke_island.exe").write_bytes(b"test-exe")
        (package / "CrashReportClient.exe").write_bytes(b"test-crash-client")
        return root, package

    def test_archive_verifies_copies_outside_package_without_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root, package = self.setup_build(directory)
            output = metadata.archive(root, package)
            self.assertFalse(output.is_relative_to(package))
            self.assertEqual((output / "monke_island.pdb").read_bytes(), b"test-pdb")
            self.assertEqual(json.loads((output / "manifest.json").read_text())["symbols"][0]["exe_sha256"], metadata.sha256(package / "monke_island.exe"))
            with self.assertRaises(ValueError):
                metadata.archive(root, package)

    def test_archive_rejects_modified_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root, package = self.setup_build(directory)
            (root / "monke_island/Config/DefaultGame.ini").write_text("changed")
            with self.assertRaises(ValueError):
                metadata.archive(root, package)

    def test_archive_rejects_wrong_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            root, package = self.setup_build(directory)
            (package / "monke_island.exe").write_bytes(b"other-build")
            with self.assertRaises(ValueError):
                metadata.archive(root, package)

    def test_archive_rejects_pdb_in_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root, package = self.setup_build(directory)
            (package / "private.pdb").write_bytes(b"pdb")
            with self.assertRaises(ValueError):
                metadata.archive(root, package)

    def test_archive_rejects_destination_inside_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root, package = self.setup_build(directory)
            with self.assertRaises(ValueError):
                metadata.archive(root, package, package / "symbols")

    def test_archive_requires_crash_reporter(self):
        with tempfile.TemporaryDirectory() as directory:
            root, package = self.setup_build(directory)
            (package / "CrashReportClient.exe").rename(package / "not-crash.txt")
            with self.assertRaises(ValueError):
                metadata.archive(root, package)


if __name__ == "__main__":
    unittest.main()
