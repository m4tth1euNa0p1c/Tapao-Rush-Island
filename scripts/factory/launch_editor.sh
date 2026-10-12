#!/bin/bash
# usage: launch_editor.sh [map]
# The editor at below normal priority (the owner plays meanwhile) with the lead session (lead_session_t186.py); waits
# for the session to be ready, then starts the PIE helper session (action "piesession", needed after every launch).
source "$(dirname "$0")/common.sh"
need_owner
MAP="${1:-/Game/MonkeIsland/Maps/Test/L_Test_Flat}"
READY="$SESSION_DIR/session-ready.json"
BEFORE=$(stat -c %Y "$READY" 2>/dev/null || echo 0)
MSYS2_ARG_CONV_EXCL="*" cmd /c start "" /BELOWNORMAL "$(cygpath -w "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor.exe")" "$(cygpath -w "$PROJECT")" "$MAP" \
	"-ExecutePythonScript=$(cygpath -m "$REPO/scripts/editor/lead_session_t186.py")"
for i in $(seq 1 240); do
	NOW=$(stat -c %Y "$READY" 2>/dev/null || echo 0)
	if [ "$NOW" -gt "$BEFORE" ]; then
		echo "editor ready after $((i * 5)) s"
		sleep 3
		bash "$(dirname "$0")/run_action.sh" piesession 120 > /dev/null && echo "PIE session started"
		exit 0
	fi
	sleep 5
done
echo "the editor was not ready after 20 min"
exit 1
