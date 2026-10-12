"""usage: python test_report.py <Saved/Automation/Reports/index.json>
Summary of an automation report; exit 1 when a test failed (integrate_wave.sh stops there)."""
import json
import sys

report = json.load(open(sys.argv[1], encoding="utf-8-sig"))
fails = [t["fullTestPath"] for t in report.get("tests", []) if t.get("state") == "Fail"]
print(f"tests: {report.get('succeeded', 0)} ok, {report.get('succeededWithWarnings', 0)} with warnings, "
      f"{report.get('failed', 0)} failed")
for name in fails[:30]:
    print("  FAIL", name)
sys.exit(1 if fails else 0)
