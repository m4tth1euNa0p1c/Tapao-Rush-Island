"""Run the prepared T-149 import and targeted map refresh in one commandlet."""
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[2]
runpy.run_path(str(ROOT / "scripts/props/import_galleon_t149.py"), run_name="__main__")
runpy.run_path(str(ROOT / "scripts/editor/refresh_galleon_testflat_t149.py"), run_name="__main__")
