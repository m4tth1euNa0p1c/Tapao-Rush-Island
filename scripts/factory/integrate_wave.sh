#!/bin/bash
# usage: integrate_wave.sh "<test filter>" [<editor action>...]
# The end of a wave of agents, once their code compiles file by file and the editor is CLOSED (ask the owner first and
# take the lock: python scripts/editor_lock.py acquire --owner "$MONKE_LOCK_OWNER" --reason "...").
#   full editor build -> headless test suite (filter) -> editor launch with the lead and PIE sessions -> the editor
#   actions of the wave, in order (imports, data assets, levels) -> summary. Stops at the first failure, with its log.
source "$(dirname "$0")/common.sh"
need_owner
HERE="$(dirname "$0")"
FILTER="${1:-monke_island}"
shift

echo "== build $(date +%H:%M)"
bash "$HERE/build_editor.sh" || exit 1

echo "== tests: $FILTER"
TEST_LOG="$WORK_DIR/logs/tests-$(date +%Y%m%d-%H%M%S).log"
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$REPO/scripts/ue.ps1")" test -Filter "$FILTER" > "$TEST_LOG" 2>&1
python "$HERE/test_report.py" "$REPO/monke_island/Saved/Automation/Reports/index.json" || { echo "log: $TEST_LOG"; exit 1; }

echo "== editor $(date +%H:%M)"
bash "$HERE/launch_editor.sh" || exit 1

for ACTION in "$@"; do
	echo "== action $ACTION $(date +%H:%M)"
	bash "$HERE/run_action.sh" "$ACTION" 1800 | head -c 600
	echo
done
echo "== done $(date +%H:%M)"
