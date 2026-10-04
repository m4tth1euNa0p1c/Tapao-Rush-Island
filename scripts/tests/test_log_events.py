import csv
import importlib.util
import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

SPEC = importlib.util.spec_from_file_location("log_events", Path(__file__).resolve().parents[1] / "log_events.py")
events = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(events)
ID = "a" * 32


class LogEventsTests(unittest.TestCase):
    def test_duration_uses_start_and_last_observed_end(self):
        rows = [events.parse_line(f"Expedition {ID}: {body}") for body in (
            "started at 25.00 s, flood at 565.00 s schema=1 build=0.1+abc rules_hash=" + "b" * 32,
            "phase EExpeditionPhase::Submerging -> EExpeditionPhase::Resolved at 625.00 s",
            "every player is dead at 635.00 s, game over, restart in 8.0 s")]
        self.assertEqual(events.summarize(rows)["durations_seconds"], [610.0])

    def test_deaths_by_cause_exclude_creatures(self):
        rows = [events.parse_line(f"Expedition {ID}: died schema=1 cause={cause} t=3.25 phase=EExpeditionPhase::Exploring alive=2 entity={kind}")
                for cause, kind in (("fall", "player"), ("shark", "player"), ("tiki", "creature"))]
        self.assertEqual(events.summarize(rows)["player_deaths_by_cause"], {"fall": 1, "shark": 1})

    def test_incomplete_and_legacy_are_not_fabricated(self):
        rows = [events.parse_line(f"Expedition {ID}: started at 3 s, flood at 123 s"),
                events.parse_line("Player_Private died (killer: None)")]
        result = events.summarize(rows)
        self.assertEqual(result["incomplete"], 1)
        self.assertEqual(result["legacy_deaths_unclassified"], 1)
        self.assertIsNone(result["mean_duration_seconds"])

    def test_privacy_allowlist(self):
        for line in ("User: Alice", "Computer: Workstation", "C:/Users/Alice/private.txt", "/home/alice/private", "192.168.1.2"):
            self.assertIsNone(events.parse_line(line))
        row = events.parse_line(f"C:/Users/Alice/private Expedition {ID}: started at 3 s, flood at 123 s build=C:/Users/Alice rules_hash=/home/alice")
        self.assertNotIn("Alice", json.dumps(row))
        self.assertEqual(row["build"], "unversioned")
        self.assertEqual(row["rules_hash"], "")

    def test_malformed_events_rejected(self):
        for body in ("died schema=2 cause=fall", "died schema=1 cause=fall t=nan phase=Exploring alive=2 entity=player",
                     "died schema=1 cause=fall t=1 phase=Exploring alive=2 entity=player C:/Users/Alice"):
            self.assertIsNone(events.parse_line(f"Expedition {ID}: {body}"))

    def test_overlapping_logs_keep_simultaneous_deaths(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / name for name in ("one.log", "two.log")]
            line = f"[2026.10.04-10.00.00:000][  1]Expedition {ID}: died schema=1 cause=fall t=1 phase=Exploring alive=0 entity=player\n"
            for path in paths:
                path.write_text(line * 2, encoding="utf-8")
            self.assertEqual(len(list(events.read_events(paths))), 2)

    def test_cli_outputs_parseable_csv_and_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "private.log").write_text(f"User: Alice\nComputer: Secret\nExpedition {ID}: started at 3 s, flood at 123 s\n", encoding="utf-8")
            with redirect_stdout(io.StringIO()):
                self.assertEqual(events.main(["--logs", str(root), "--csv", str(root / "out.csv"), "--summary", str(root / "out.json")]), 0)
            with (root / "out.csv").open(newline="", encoding="utf-8") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 1)
            self.assertEqual(json.loads((root / "out.json").read_text())["expeditions"], 1)
            self.assertNotIn("Alice", (root / "out.csv").read_text())


if __name__ == "__main__":
    unittest.main()
