#!/bin/bash
# usage: pie_step.sh "<steps>" [timeout_s]
# Steps of scripts/editor/fort_land_pie_check.py (PIE session action "fortcheck"), separated by ";". The first step after
# the lead's own StartPIE must be "claim": the steps never run in a game the lead did not start (the owner's own PIE).
# Prints the result JSON of the run; what the steps saw is appended to monke_island/Saved/T186/fortland-check.json.
source "$(dirname "$0")/common.sh"
need_owner
printf '%s' "$1" > "$SESSION_DIR/fortland-step.txt"
RESULT="$SESSION_DIR/pie-result.json"
BEFORE=$(stat -c %Y "$RESULT" 2>/dev/null || echo 0)
sleep 1
printf '{"action": "fortcheck"}' > "$SESSION_DIR/pie-request.json"
for i in $(seq 1 "${2:-90}"); do
	NOW=$(stat -c %Y "$RESULT" 2>/dev/null || echo 0)
	if [ "$NOW" -gt "$BEFORE" ]; then
		cat "$RESULT"
		exit 0
	fi
	sleep 1
done
echo "TIMEOUT: is the PIE session started (action piesession after each editor launch)?"
exit 1
