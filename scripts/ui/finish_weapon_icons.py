"""Finishes the icons rendered by scripts/ui/render_weapon_icons.py (T-264): a soft dark shadow under each model so
that its outline reads on the dark inventory cells, then a contact sheet art/ui/icons/weapons/_sheet.png.
System Python with Pillow:  python scripts/ui/finish_weapon_icons.py
"""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1].parent
OUT = ROOT / "art/ui/icons/weapons"


def shadow(path):
    image = Image.open(path).convert("RGBA")
    raw = OUT / "_raw" / path.name
    raw.parent.mkdir(exist_ok=True)
    if raw.exists():
        image = Image.open(raw).convert("RGBA")  # always from the render, never twice
    else:
        image.save(raw)
    alpha = image.getchannel("A")
    halo = alpha.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(6)).point(lambda a: int(a * 0.55))
    shade = Image.new("RGBA", image.size, (8, 6, 4, 0))
    shade.putalpha(halo)
    result = Image.new("RGBA", image.size, (0, 0, 0, 0))
    result.paste(shade, (3, 5))
    result.alpha_composite(image)
    result.save(path)


def main():
    rows = json.loads((OUT / "icons.json").read_text(encoding="utf-8"))
    done = [r for r in rows if "png" in r]
    for row in done:
        shadow(ROOT / row["png"])
    cols, cell = 5, 200
    sheet = Image.new("RGBA", (cols * cell, ((len(done) + cols - 1) // cols) * (cell + 22)), (38, 34, 30, 255))
    draw = ImageDraw.Draw(sheet)
    for index, row in enumerate(done):
        icon = Image.open(ROOT / row["png"]).resize((cell - 16, cell - 16))
        x, y = (index % cols) * cell, (index // cols) * (cell + 22)
        sheet.alpha_composite(icon, (x + 8, y + 8))
        draw.text((x + 8, y + cell), row["name"], fill=(235, 225, 200, 255))
    sheet.save(OUT / "_sheet.png")
    print(f"{len(done)} icons finished, sheet {OUT / '_sheet.png'}; errors: {[r['name'] for r in rows if 'error' in r]}")


main()
