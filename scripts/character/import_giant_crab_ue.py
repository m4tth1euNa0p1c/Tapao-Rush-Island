"""Giant crab "Tupa Nui": the action "crabimport" now runs the import of the v2 built in this repository (T-260).

History: until 2026-10-09 this script ran the maquette's own import (<MAQUETTES>/crabe-geant/ue/import_giant_crab.py,
v1 of the 2nd PC, T-211). Since T-260 the crab is built here (scripts/creatures/giant_crab/build.py: legs modelled,
terrifying look, baked textures, new clips) and reimported in place by scripts/creatures/giant_crab/import_giant_crab_v2_ue.py
(action "crabv2import"). Running the v1 maquette script again would put the v1 mesh, materials and clips back over the
v2: it is not run any more, "crabimport" goes to the v2 import (same skeleton, same assets, same clip names).
Editor open, no game running: scripts/editor/lead_session_t186.py, action "crabimport" or "crabv2import".
"""
import runpy
from pathlib import Path

V2 = Path(__file__).resolve().parents[2] / "scripts/creatures/giant_crab/import_giant_crab_v2_ue.py"

if __name__ == "__main__":
	runpy.run_path(str(V2), run_name="__main__")
