#!/bin/bash
# usage: build_editor.sh                      full build of the editor target (editor CLOSED, lock taken)
#        build_editor.sh --single <file.cpp>  one file compiled without linking (allowed while the editor runs)
# An editor that hung on exit keeps the module DLL loaded: its DLL and PDB are renamed out of the way (the old process
# keeps the renamed copy) so the link can write new ones. Log in build/factory/logs.
source "$(dirname "$0")/common.sh"
BIN="$REPO/monke_island/Binaries/Win64"
ARGS=(monke_islandEditor Win64 Development "-Project=$(cygpath -m "$PROJECT")" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=8)
if [ "$1" = "--single" ]; then
	ARGS+=("-SingleFile=$(cygpath -m "$(realpath "$2")")")
	LOG="$WORK_DIR/logs/single-$(basename "$2" .cpp).log"
else
	need_owner
	if tasklist //FI "IMAGENAME eq UnrealEditor.exe" 2>/dev/null | grep -qi "UnrealEditor.exe"; then
		for F in UnrealEditor-monke_island.dll UnrealEditor-monke_island.pdb; do
			if [ -f "$BIN/$F" ] && ! mv "$BIN/$F" "$BIN/$F.zombie-$(date +%H%M%S)" 2>/dev/null; then
				echo "an editor still runs and holds $F: close it first"
				exit 3
			fi
		done
	fi
	LOG="$WORK_DIR/logs/build-editor-$(date +%Y%m%d-%H%M%S).log"
fi
cd "$UE_ROOT/Engine/Source" || exit 1
dotnet ../../Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll "${ARGS[@]}" > "$LOG" 2>&1
CODE=$?
grep -E "error C|error LNK|fatal error|Result:|Total execution" "$LOG" | head -20
echo "log: $LOG (exit $CODE)"
exit $CODE
