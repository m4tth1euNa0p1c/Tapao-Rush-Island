#!/bin/bash
# usage: run_action.sh <action> [timeout_s]
# One action of the lead editor session (scripts/editor/lead_session_t186.py, ACTIONS), one caller at a time: the
# request is written, then the result OF THIS ACTION written after the request is awaited (a runner that gave up early
# once let the next request in while the editor was still importing). The editor refuses every action during a game.
source "$(dirname "$0")/common.sh"
need_owner
ACTION="$1"
TIMEOUT="${2:-1800}"
LOCK="$SESSION_DIR/action.lockdir"
RESULT="$SESSION_DIR/session-result.json"
for i in $(seq 1 7200); do
	if mkdir "$LOCK" 2>/dev/null; then
		break
	fi
	if [ -d "$LOCK" ] && [ $(( $(date +%s) - $(stat -c %Y "$LOCK") )) -gt 3600 ]; then
		rmdir "$LOCK" 2>/dev/null
	fi
	sleep 1
done
sleep 1
START=$(date +%s)
printf '{"action": "%s", "requested_at": %s}' "$ACTION" "$START" > "$SESSION_DIR/session-request.json"
for i in $(seq 1 "$TIMEOUT"); do
	NOW=$(stat -c %Y "$RESULT" 2>/dev/null || echo 0)
	if [ "$NOW" -ge "$START" ] && grep -q "\"action\": \"$ACTION\"" "$RESULT" 2>/dev/null; then
		cat "$RESULT"
		rmdir "$LOCK" 2>/dev/null
		exit 0
	fi
	sleep 1
done
echo "TIMEOUT waiting for $ACTION (the editor may still be working: do not send another request)"
exit 1
