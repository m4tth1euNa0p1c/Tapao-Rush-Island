"""Use the project's actual CharacterMovement and grab component for T198."""
from pathlib import Path
import json
import runpy
import datetime as dt
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))  # its helpers sit next to it, whatever session runs it
from fort_showcase_import import check_lock
from fort_showcase_callback_cleanup import cleanup
check_lock();cleanup()
root=Path(__file__).resolve().parents[2]
out=root/'monke_island/Saved/T198'
previous=out/'traversal-result.json'
if previous.exists():
    runs=out/'runs';runs.mkdir(exist_ok=True)
    (runs/(dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.json')).write_bytes(previous.read_bytes())
options=root/'monke_island/Saved/T198/validation-options.json'
config=json.loads(options.read_text()) if options.exists() else {}
runpy.run_path(str(root/'scripts/props/fort_showcase_validate.py'),run_name='__main__',
              init_globals={'VALIDATION_TASK':'T-198','ROUTE_NAMES':config.get('routes'),'SKIP_LOOT':config.get('skip_loot',False)})
