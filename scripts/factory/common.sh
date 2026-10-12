#!/bin/bash
# Shared settings of the factory helpers (sourced by the other scripts of this folder).
# The lead's session name must be in MONKE_LOCK_OWNER (the editor lock, scripts/editor_lock.py).
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/c/Program Files/Epic Games/UE_5.8}"
PROJECT="$REPO/monke_island/monke_island.uproject"
SESSION_DIR="$REPO/monke_island/Saved/T186"
SHOTS_DIR="$REPO/monke_island/Saved/Screenshots/WindowsEditor"
WORK_DIR="${MONKE_WORK_DIR:-$REPO/build/factory}"
mkdir -p "$WORK_DIR/logs" "$WORK_DIR/shots"

need_owner() {
	if [ -z "$MONKE_LOCK_OWNER" ]; then
		echo "MONKE_LOCK_OWNER is not set: the session name of the lead"
		exit 2
	fi
}
