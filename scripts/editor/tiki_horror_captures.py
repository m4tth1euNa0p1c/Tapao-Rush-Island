"""Preuves visuelles de T-274 (Veilleurs « Outlast ») : vignettes rendues par l'éditeur après le réimport en place.

Hors de l'éditeur, par le MCP Unreal (HTTP JSON-RPC, EditorAppToolset.CaptureAssetImage) : lecture seule, rien n'est
écrit dans le contenu du projet. Prend le verrou d'action de la session du lead
(monke_island/Saved/T186/action.lockdir) pour ne jamais tourner pendant une action.

Usage : python scripts/editor/tiki_horror_captures.py [CrabGuardian AshWatcher CocoWarrior]
Écrit docs/production/captures/T-274/<asset>.png.
"""
import base64
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from weather_blood_captures import LOCK, ROOT, Mcp, image_data  # noqa: E402

WEAPONS = {"CrabGuardian": "ShellHammer", "AshWatcher": "ObsidianClub", "CocoWarrior": "Spear"}


def assets(mob):
    base = f"/Game/MonkeIsland/Characters/{mob}"
    return [f"{base}/Meshes/SK_{mob}", f"{base}/Meshes/SK_{mob}_{WEAPONS[mob]}", f"{base}/Dismember/SK_{mob}_Head",
            f"{base}/Dismember/SK_{mob}_Torso", f"/Game/MonkeIsland/Items/MobWeapons/Meshes/SM_{mob}_{WEAPONS[mob]}",
            f"{base}/Materials/MI_{mob}_Body"]


def main():
    mobs = [m for m in sys.argv[1:] if m in WEAPONS] or list(WEAPONS)
    for _ in range(600):
        try:
            LOCK.mkdir()
            break
        except FileExistsError:
            time.sleep(1)
    else:
        sys.exit("action lock busy for 10 min")
    try:
        mcp = Mcp()
        out = ROOT / "docs/production/captures/T-274"
        out.mkdir(parents=True, exist_ok=True)
        for mob in mobs:
            for path in assets(mob):
                text = mcp.raw("EditorToolset.EditorAppToolset", "CaptureAssetImage", {"assetPath": path})
                data = image_data(text)
                if not data:
                    print("NO IMAGE", path, text[:300])
                    continue
                target = out / (path.rsplit("/", 1)[-1] + ".png")
                target.write_bytes(base64.b64decode(data))
                print("captured", target.relative_to(ROOT), len(data) * 3 // 4, "bytes")
    finally:
        LOCK.rmdir()


if __name__ == "__main__":
    main()
