"""Assemble the minka reference board (layout of the owner's brief) from the renders of minka_rurale.py.

Usage (system Python with Pillow): python scripts/props/minka_board.py
Input: art/_previews/minka_{trois_quarts,facade,profil,dessus,interieur}.png -> art/_previews/minka_planche.png
"""
from pathlib import Path

PREV = Path(__file__).resolve().parents[2] / "art" / "_previews"


def board(paths, dest):
    """Reference board like the brief's sheet: hero 3/4, facade + profile, roofless top view, interior."""
    from PIL import Image, ImageDraw, ImageFont
    W, H = 3000, 2000
    sheet = Image.new("RGB", (W, H), (246, 246, 247))
    d = ImageDraw.Draw(sheet)

    def font(sz, bold=False):
        for f in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arial.ttf"):
            try:
                return ImageFont.truetype(f"C:/Windows/Fonts/{f}", sz)
            except OSError:
                continue
        return ImageFont.load_default()

    def place(key, box, caption):
        im = Image.open(paths[key]).convert("RGB")
        x0, y0, x1, y1 = box
        im.thumbnail((x1 - x0, y1 - y0 - 50))
        sheet.paste(im, (x0 + (x1 - x0 - im.width) // 2, y0))
        w = d.textlength(caption, font=font(34))
        d.text((x0 + (x1 - x0 - w) / 2, y0 + im.height + 8), caption, fill=(70, 70, 74), font=font(34))

    d.text((60, 40), "RÉFÉRENCE 3D · BLENDER", fill=(60, 60, 64), font=font(64, True))
    d.text((62, 120), "MINKA RURALE", fill=(90, 90, 94), font=font(44))
    place("trois_quarts", (40, 200, 1760, 1180), "3/4 EXTÉRIEUR")
    place("facade", (1800, 200, 2960, 700), "FAÇADE")
    place("profil", (1800, 700, 2960, 1180), "PROFIL")
    place("dessus", (40, 1220, 1500, 1980), "DESSUS · TOIT RETIRÉ")
    place("interieur", (1540, 1220, 2960, 1980), "INTÉRIEUR")
    sheet.save(dest)
    return dest


if __name__ == "__main__":
    print(board({k: PREV / f"minka_{k}.png" for k in ("trois_quarts", "facade", "profil", "dessus", "interieur")},
                PREV / "minka_planche.png"))
