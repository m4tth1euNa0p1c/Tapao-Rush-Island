"""Behavioral checks for scripts/tidemark/check_*.py on temporary fixtures; never runs Unreal.

Run: python3 -m unittest scripts/tests/test_tidemark_checks.py -v
"""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tidemark"))
import check_credits
import check_no_game_dependency
import check_no_unseeded_random

PLUGIN = Path("monke_island/Plugins/Tidemark")
SOURCE = PLUGIN / "Source"

GOOD_CREDIT = {"producer": "Service de l'urbanisme, Te Fenua", "license": "Licence Ouverte Etalab 2.0",
               "url": "https://www.tefenua.gov.pf", "date": "2026-10-20"}
GOOD_ASSET = {"path": "Content/Demo/T_FirstWet.uasset", "tool": "Tidemark bake 0.1.0", "date": "2026-10-20",
              "license": "MIT", "sha256": "a" * 64}


def run(module, root: Path):
    """Runs a check's main() on a fixture root, returning (exit code, stdout)."""
    with redirect_stdout(io.StringIO()) as output:
        code = module.main(["--root", str(root)])
    return code, output.getvalue()


class FixtureCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tidemark checks ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.write(SOURCE / "TidemarkRuntime/TidemarkRuntime.Build.cs",
                   'PublicDependencyModuleNames.AddRange(new string[] { "Core", "Engine" });\n')
        self.write(SOURCE / "TidemarkRuntime/Private/Solver/TidemarkSolver.cpp",
                   '#include "Tidemark/Solver/TidemarkSolver.h"\n'
                   'FRandomStream Stream = Seed.Derive(TEXT("Rain")).ToRandomStream();\n'
                   'UE_LOG(LogTidemark, Log, TEXT("step %u"), StepIndex);\n')
        self.write(SOURCE / "TidemarkRuntime/Private/Tests/TidemarkSolverSpec.cpp",
                   'const float Noise = FMath::FRand(); // tests may use global random\n')
        self.write(PLUGIN / "PROVENANCE.json", json.dumps({"assets": [GOOD_ASSET]}))
        self.write(PLUGIN / "Content/Demo/manifest.json", json.dumps({"version": 1, "credits": [GOOD_CREDIT]}))
        self.write(PLUGIN / "Docs/README.md", "# Tidemark\nSimulation ludique, non calibrée.\n")
        self.write(PLUGIN / "Docs/LICENSES/THIRD_PARTY.md", "Imagery (c) Maxar is NOT embedded; listed for exclusion.\n")

    def write(self, rel: Path, text: str) -> Path:
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path


class NoGameDependencyTests(FixtureCase):
    def test_clean_plugin_passes(self):
        code, out = run(check_no_game_dependency, self.root)
        self.assertEqual(0, code, out)
        self.assertIn("OK check_no_game_dependency", out)

    def test_include_of_game_fails_with_file_and_line(self):
        self.write(SOURCE / "TidemarkRuntime/Private/Bad.cpp",
                   '#include "Tidemark/TidemarkModule.h"\n#include "Flood/FloodGrid.h" // from monke_island\n')
        code, out = run(check_no_game_dependency, self.root)
        self.assertEqual(1, code)
        self.assertIn("monke_island/Plugins/Tidemark/Source/TidemarkRuntime/Private/Bad.cpp:2:", out)

    def test_game_asset_and_log_category_fail(self):
        for text in ('LoadObject<UTexture2D>(nullptr, TEXT("/Game/MonkeIsland/T_X"));\n',
                     'UE_LOG(Logmonke_island, Log, TEXT("x"));\n'):
            with self.subTest(text=text):
                self.write(SOURCE / "TidemarkRuntime/Private/Bad.cpp", text)
                code, out = run(check_no_game_dependency, self.root)
                self.assertEqual(1, code)
                self.assertIn("Bad.cpp:1:", out)

    def test_build_cs_listing_game_module_fails(self):
        self.write(SOURCE / "TidemarkEditor/TidemarkEditor.Build.cs",
                   'PrivateDependencyModuleNames.AddRange(new string[] {\n\t"UnrealEd",\n\t"monke_island"\n});\n')
        code, out = run(check_no_game_dependency, self.root)
        self.assertEqual(1, code)
        self.assertIn("TidemarkEditor.Build.cs:3: Build.cs référence le module monke_island", out)

    def test_missing_source_folder_fails(self):
        code, out = run(check_no_game_dependency, self.root / "nowhere")
        self.assertEqual(1, code)
        self.assertIn("introuvable", out)


class NoUnseededRandomTests(FixtureCase):
    def test_seeded_stream_and_test_folder_pass(self):
        code, out = run(check_no_unseeded_random, self.root)
        self.assertEqual(0, code, out)
        self.assertIn("OK check_no_unseeded_random", out)

    def test_each_forbidden_call_fails(self):
        cases = {
            "FMath::Rand": "int32 R = FMath::Rand();",
            "FMath::FRand": "float R = FMath::FRand();",
            "RandRange": "int32 R = FMath::RandRange(0, 3);",
            "FRandRange": "float R = FMath::FRandRange(0.f, 1.f);",
            "VRand": "FVector V = FMath::VRand();",
            "RandBool": "bool B = FMath::RandBool();",
            "FRandomStream(": "FRandomStream Stream(42);",
            "FDateTime::Now": "FDateTime T = FDateTime::Now();",
            "FDateTime::UtcNow": "FDateTime T = FDateTime::UtcNow();",
            "FPlatformTime::Cycles": "uint64 C = FPlatformTime::Cycles64();",
            "GetTimeSeconds()": "float T = GetWorld()->GetTimeSeconds();",
        }
        for name, line in cases.items():
            with self.subTest(name=name):
                self.write(SOURCE / "TidemarkRuntime/Private/Rules/Bad.cpp", f"// header\n{line}\n")
                code, out = run(check_no_unseeded_random, self.root)
                self.assertEqual(1, code, f"{name} should fail")
                self.assertIn("Rules/Bad.cpp:2:", out)

    def test_explicit_allow_comment_passes(self):
        self.write(SOURCE / "TidemarkRuntime/Private/Render/Shimmer.cpp",
                   "float Phase = FMath::FRand(); // tidemark-allow-random: cosmetic only, never written to simulation state\n")
        code, out = run(check_no_unseeded_random, self.root)
        self.assertEqual(0, code, out)

    def test_allow_comment_without_reason_still_fails(self):
        self.write(SOURCE / "TidemarkRuntime/Private/Render/Shimmer.cpp",
                   "float Phase = FMath::FRand(); // tidemark-allow-random:\n")
        code, _ = run(check_no_unseeded_random, self.root)
        self.assertEqual(1, code)

    def test_comments_naming_forbidden_calls_pass(self):
        self.write(SOURCE / "TidemarkRuntime/Public/Tidemark/Solver/TidemarkSeed.h",
                   "/**\n * Never derive from FMath::Rand*, FDateTime::Now or FPlatformTime::Cycles (CONVENTIONS §4).\n */\n"
                   "// FRandomStream(42) would be wrong here\n"
                   "uint64 Value; // not GetTimeSeconds()\n")
        code, out = run(check_no_unseeded_random, self.root)
        self.assertEqual(0, code, out)

    def test_code_before_trailing_comment_still_fails(self):
        self.write(SOURCE / "TidemarkRuntime/Private/Rules/Bad.cpp", "int32 R = FMath::Rand(); // seeded later, promise\n")
        code, _ = run(check_no_unseeded_random, self.root)
        self.assertEqual(1, code)

    def test_editor_module_is_scanned_too(self):
        self.write(SOURCE / "TidemarkEditor/Private/Bake.cpp", "const double T0 = FPlatformTime::Cycles64();\n")
        code, out = run(check_no_unseeded_random, self.root)
        self.assertEqual(1, code)
        self.assertIn("TidemarkEditor/Private/Bake.cpp:1:", out)


class CreditsTests(FixtureCase):
    def test_complete_credits_and_provenance_pass(self):
        code, out = run(check_credits, self.root)
        self.assertEqual(0, code, out)
        self.assertIn("1 manifest(s), 1 PROVENANCE.json", out)

    def test_empty_provenance_assets_pass(self):
        self.write(PLUGIN / "PROVENANCE.json", json.dumps({"assets": []}))
        code, _ = run(check_credits, self.root)
        self.assertEqual(0, code)

    def test_manifest_missing_credit_field_fails(self):
        for field in ("producer", "license", "url", "date"):
            with self.subTest(field=field):
                credit = dict(GOOD_CREDIT, **{field: ""})
                self.write(PLUGIN / "Content/Demo/manifest.json", json.dumps({"credits": [credit]}))
                code, out = run(check_credits, self.root)
                self.assertEqual(1, code)
                self.assertIn(f"Content/Demo/manifest.json: `credits[0].{field}` manquant ou vide", out)

    def test_manifest_without_credits_fails(self):
        self.write(PLUGIN / "Content/Demo/manifest.json", json.dumps({"version": 1}))
        code, out = run(check_credits, self.root)
        self.assertEqual(1, code)
        self.assertIn("`credits[]` absent ou vide", out)

    def test_provenance_bad_sha_or_missing_field_fails(self):
        for asset in (dict(GOOD_ASSET, sha256="deadbeef"), dict(GOOD_ASSET, tool="")):
            with self.subTest(asset=asset):
                self.write(PLUGIN / "PROVENANCE.json", json.dumps({"assets": [asset]}))
                code, out = run(check_credits, self.root)
                self.assertEqual(1, code)
                self.assertIn("PROVENANCE.json: `assets[0].", out)

    def test_missing_or_malformed_provenance_fails(self):
        (self.root / PLUGIN / "PROVENANCE.json").unlink()
        code, out = run(check_credits, self.root)
        self.assertEqual(1, code)
        self.assertIn("PROVENANCE.json: fichier absent", out)
        self.write(PLUGIN / "PROVENANCE.json", "{unfinished")
        code, out = run(check_credits, self.root)
        self.assertEqual(1, code)
        self.assertIn("JSON illisible", out)

    def test_forbidden_strings_in_content_and_docs_fail(self):
        for rel, text in ((PLUGIN / "Docs/README.md", "Relief estimé depuis l'imagerie maxar\n"),
                          (PLUGIN / "Content/Demo/notes.txt", "Source : Google 3D Tiles\n"),
                          (PLUGIN / "Content/Demo/layers.txt", "line1\ncouche cocoteraie DAG\n"),
                          (PLUGIN / "Docs/README.md", "DigitalGlobe imagery\n")):
            with self.subTest(rel=rel.as_posix(), text=text):
                path = self.write(rel, text)
                code, out = run(check_credits, self.root)
                self.assertEqual(1, code)
                self.assertIn("chaîne interdite", out)
                path.unlink() if "Content" in rel.as_posix() else self.write(rel, "# Tidemark\n")

    def test_docs_licenses_is_excluded_from_forbidden_strings(self):
        code, _ = run(check_credits, self.root)
        self.assertEqual(0, code)


if __name__ == "__main__":
    unittest.main()
