"""Finishes the capture icons rendered by scripts/ui/render_capture_icons.py (T-337), like finish_mob_weapon_icons.py:
a soft dark shadow under each model so its outline reads on the dark inventory cells, then a contact sheet
art/ui/icons/capture/_sheet.png. Always from the raw render (_raw/), never twice.
System Python with Pillow:  python scripts/ui/finish_capture_icons.py
"""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/ui/icons/capture"


def shadow(raw, target):
    image = Image.open(raw).convert("RGBA")
    alpha = image.getchannel("A")
    halo = alpha.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(6)).point(lambda a: int(a * 0.55))
    shade = Image.new("RGBA", image.size, (8, 6, 4, 0))
    shade.putalpha(halo)
    result = Image.new("RGBA", image.size, (0, 0, 0, 0))
    result.paste(shade, (3, 5))
    result.alpha_composite(image)
    result.save(target)
    box = alpha.getbbox()
    return {"coverage_px": [box[2] - box[0], box[3] - box[1]] if box else None}


def main():
    rows = json.loads((OUT / "icons.json").read_text(encoding="utf-8"))
    done = [r for r in rows if "raw" in r]
    for row in done:
        row.update(shadow(ROOT / row["raw"], ROOT / row["png"]))
    (OUT / "icons.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")
    cell = 220
    sheet = Image.new("RGBA", (max(1, len(done)) * cell, cell + 22), (38, 34, 30, 255))
    draw = ImageDraw.Draw(sheet)
    for index, row in enumerate(done):
        icon = Image.open(ROOT / row["png"]).resize((cell - 16, cell - 16))
        sheet.alpha_composite(icon, (index * cell + 8, 8))
        draw.text((index * cell + 8, cell), row["name"], fill=(235, 225, 200, 255))
    sheet.save(OUT / "_sheet.png")
    print(f"{len(done)} icons finished: {[(r['name'], r.get('coverage_px')) for r in done]}; "
          f"errors: {[r['name'] for r in rows if 'error' in r]}")


main()
