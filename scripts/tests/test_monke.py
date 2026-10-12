"""Behavioral checks for read-only diagnostics; never runs Unreal."""
from contextlib import redirect_stdout
from datetime import datetime, timedelta, timezone
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import monke


class DiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="monke tooling ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "report with spaces.json"

    def write_report(self, tests=None, **overrides):
        tests = tests if tests is not None else [{"fullTestPath": "monke_island.Run.case", "state": "Success", "errors": 0}]
        data = {"tests": tests, "succeeded": len(tests), "succeededWithWarnings": 0,
                "failed": 0, "notRun": 0, "inProcess": 0}
        data.update(overrides)
        self.path.write_text(json.dumps(data), encoding="utf-8-sig")
        return self.path

    def check(self, **kwargs):
        return monke.report(self.path, kwargs.get("prefix", "monke_island"),
                            kwargs.get("minimum", 1), 24, kwargs.get("since"))

    def test_success_with_warnings_is_explicit(self):
        self.write_report([{"fullTestPath": "monke_island.Run.case", "state": "Success", "warnings": 2}],
                          succeeded=0, succeededWithWarnings=1)
        result = self.check()
        self.assertTrue(result["ok"])
        self.assertEqual(1, len(result["warnings"]))

    def test_missing_and_malformed_report_exit_two(self):
        for content in (None, "{unfinished", "{}", '[]'):
            with self.subTest(content=content):
                if content is not None:
                    self.path.write_text(content, encoding="utf-8")
                with redirect_stdout(io.StringIO()) as output:
                    code = monke.main(["report", str(self.path), "--json"])
                self.assertEqual(2, code)
                self.assertFalse(json.loads(output.getvalue())["ok"])

    def test_failed_incomplete_and_unknown_states_fail(self):
        for state in ("Fail", "NotRun", "InProcess", "Skipped", None):
            with self.subTest(state=state):
                self.write_report([{"fullTestPath": "monke_island.Run.case", "state": state}])
                self.assertFalse(self.check()["ok"])

    def test_errors_override_success_state(self):
        for extra in ({"errors": 1}, {"entries": [{"event": {"type": "Error", "message": "ensure"}}]}):
            self.write_report([dict(fullTestPath="monke_island.Run.case", state="Success", **extra)])
            self.assertFalse(self.check()["ok"])

    def test_empty_report_and_missing_suite_fail(self):
        self.write_report([])
        self.assertFalse(self.check()["ok"])
        self.write_report()
        self.assertFalse(self.check(prefix="monke_island.Boats")["ok"])
        self.assertFalse(self.check(minimum=2)["ok"])

    def test_filter_respects_domain_boundary(self):
        self.write_report([{"fullTestPath": "monke_island.Runaway.case", "state": "Success"}])
        self.assertFalse(self.check(prefix="monke_island.Run")["ok"])

    def test_failed_other_suite_does_not_fail_selected_suite(self):
        self.write_report([{"fullTestPath": "monke_island.Run.case", "state": "Success"},
                           {"fullTestPath": "monke_island.Boats.case", "state": "Fail"}], succeeded=1, failed=1)
        self.assertTrue(self.check(prefix="monke_island.Run")["ok"])
        self.assertFalse(self.check()["ok"])

    def test_old_report_and_previous_run_fail(self):
        self.write_report()
        old = time.time() - 48 * 3600
        os.utime(self.path, (old, old))
        self.assertFalse(self.check()["ok"])
        self.write_report()
        self.assertFalse(self.check(since=datetime.now(timezone.utc) + timedelta(minutes=1))["ok"])

    def test_duplicate_paths_and_inconsistent_totals_fail(self):
        entry = {"fullTestPath": "monke_island.Run.case", "state": "Success"}
        self.write_report([entry, entry])
        self.assertFalse(self.check()["ok"])
        self.write_report(succeeded=50)
        self.assertFalse(self.check()["ok"])

    def test_filtered_report_rejects_unexplained_global_failure(self):
        self.write_report([{"fullTestPath": "monke_island.Run.case", "state": "Success"},
                           {"fullTestPath": "monke_island.Other.case", "state": "Success"}],
                          succeeded=1, failed=1)
        self.assertFalse(self.check(prefix="monke_island.Run")["ok"])

    def test_malformed_events_are_rejected_even_outside_filter(self):
        for entry in (None, {}, {"event": {"type": None}}, {"event": {"type": "mystery"}}):
            for suite in ("Run", "Other"):
                with self.subTest(entry=entry, suite=suite):
                    self.write_report([{"fullTestPath": f"monke_island.{suite}.case", "state": "Success", "entries": [entry]}])
                    with redirect_stdout(io.StringIO()) as output:
                        code = monke.main(["report", str(self.path), "--filter", "monke_island.Run", "--json"])
                    self.assertEqual(2, code)
                    self.assertFalse(json.loads(output.getvalue())["ok"])

    def test_report_cli_failure_exit_one_and_no_writes(self):
        self.write_report([])
        before = self.path.read_bytes()
        with redirect_stdout(io.StringIO()) as output:
            code = monke.main(["report", str(self.path), "--json"])
        self.assertEqual(1, code)
        self.assertEqual(0, json.loads(output.getvalue())["selected"])
        self.assertEqual(before, self.path.read_bytes())

    def test_task_lookup_preserves_board(self):
        board = self.root / monke.BOARD
        board.parent.mkdir(parents=True)
        board.write_text("| T-001 | A | lead | review | run | 2026-10-04 | après T-002 |\n"
                         "| T-002 | B | lead | a-faire | run | 2026-10-04 | |\n", encoding="utf-8")
        before = board.read_bytes()
        result = monke.task(self.root, "t-001")
        self.assertEqual(["T-002"], result["referenced_tasks"])
        self.assertEqual(before, board.read_bytes())
        with self.assertRaises(ValueError):
            monke.task(self.root, "T-999")
        board.write_bytes(before + before)
        with self.assertRaises(ValueError):
            monke.read_board(self.root)

    def test_git_names_are_preserved(self):
        raw = " M docs/été avec espaces.md\0R  new\nname.py\0old name.py\0?? new folder/\0"
        with patch.object(monke, "git", return_value=raw):
            changes = monke.git_changes(self.root)
        self.assertEqual("docs/été avec espaces.md", changes[0]["path"])
        self.assertEqual("new\nname.py", changes[1]["path"])
        self.assertEqual("old name.py", changes[1]["previous_path"])
        self.assertEqual(3, len(changes))


if __name__ == "__main__":
    unittest.main()
