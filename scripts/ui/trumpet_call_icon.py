"""T-333: picture of the trumpet call in the HUD (a League of Legends like spell icon, bottom left).

Built from the kit's render of the herald trumpet (art/ui/tapao/sprites/items/item_trumpet.png, ui-tapao maquette of
the second PC: SM_Bean_Trumpet with its banner), no Blender: the trumpet raised towards the top right on a painted
crimson ground with rays of light and the arcs of its call coming out of the bell, darker towards the edges. Square and
opaque: the gold frame, the key and the recharge are drawn by the HUD (STapaoSpellSlot). Deterministic.

  python -I scripts/ui/trumpet_call_icon.py  ->  art/ui/icons/spells/T_Icon_TrumpetCall.png (256 x 256)
Imported by scripts/editor/trumpet_call_ue.py.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "art/ui/tapao/sprites/items/item_trumpet.png"
OUT = ROOT / "art/ui/icons/spells/T_Icon_TrumpetCall.png"

SIZE = 256
SUPER = 4                                   # drawn 4 times larger, then reduced (smooth edges)
CANVAS = SIZE * SUPER
# the trumpet in its render: mouthpiece and centre of the bell (px of the 512 picture)
MOUTHPIECE = (75.0, 135.0)
BELL = (440.0, 252.0)
RAISE_DEGREES = 42.0                        # turned up: the bell points to the top right
TRUMPET_SPAN = 0.8                          # its rotated box fills this share of the icon
TRUMPET_CENTRE = (0.42, 0.6)                # where that box sits (shares of the icon)
GROUND_INNER = np.array([150, 34, 30], dtype=np.float32)    # crimson behind the bell
GROUND_OUTER = np.array([22, 12, 26], dtype=np.float32)     # deep night towards the corners
RAY_COLOUR = (255, 214, 140)
RAYS = 14
ARC_COLOUR = (255, 236, 176)
SEED = 333


def ground(bell_xy):
    """Radial gradient from the bell, with soft rays of light and a little paint grain"""
    ys, xs = np.mgrid[0:CANVAS, 0:CANVAS].astype(np.float32)
    dx, dy = xs - bell_xy[0], ys - bell_xy[1]
    dist = np.sqrt(dx * dx + dy * dy) / (CANVAS * 1.05)
    t = np.clip(dist, 0.0, 1.0) ** 0.8
    colour = GROUND_INNER[None, None, :] * (1 - t[..., None]) + GROUND_OUTER[None, None, :] * t[..., None]
    # rays turning around the bell, fading with the distance
    angle = np.arctan2(dy, dx)
    rays = (0.5 + 0.5 * np.cos(angle * RAYS)) ** 6 * np.clip(1.0 - dist * 1.3, 0.0, 1.0)
    colour += np.array(RAY_COLOUR, dtype=np.float32)[None, None, :] * (0.16 * rays)[..., None]
    grain = np.random.default_rng(SEED).normal(0.0, 3.0, (CANVAS, CANVAS, 1)).astype(np.float32)
    colour = np.clip(colour + grain, 0, 255).astype(np.uint8)
    return Image.fromarray(colour, "RGB").convert("RGBA").filter(ImageFilter.GaussianBlur(SUPER * 0.6))


def rotate_point(point, centre, degrees, new_centre):
    """Where a point of a picture lands once Image.rotate(degrees, expand=True) turned it (counterclockwise on screen)"""
    a = math.radians(degrees)
    dx, dy = point[0] - centre[0], point[1] - centre[1]
    return (new_centre[0] + dx * math.cos(a) + dy * math.sin(a), new_centre[1] - dx * math.sin(a) + dy * math.cos(a))


def trumpet():
    """The trumpet turned and scaled, where it goes on the canvas, and its bell and axis there"""
    picture = Image.open(SOURCE).convert("RGBA")
    centre = (picture.width / 2.0, picture.height / 2.0)
    turned = picture.rotate(RAISE_DEGREES, resample=Image.BICUBIC, expand=True)
    new_centre = (turned.width / 2.0, turned.height / 2.0)
    bell = rotate_point(BELL, centre, RAISE_DEGREES, new_centre)
    mouth = rotate_point(MOUTHPIECE, centre, RAISE_DEGREES, new_centre)
    box = turned.getbbox()
    cropped = turned.crop(box)
    scale = TRUMPET_SPAN * CANVAS / max(cropped.width, cropped.height)
    cropped = cropped.resize((round(cropped.width * scale), round(cropped.height * scale)), Image.LANCZOS)
    left = round(TRUMPET_CENTRE[0] * CANVAS - cropped.width / 2)
    top = round(TRUMPET_CENTRE[1] * CANVAS - cropped.height / 2)

    def place(point):
        return (left + (point[0] - box[0]) * scale, top + (point[1] - box[1]) * scale)

    bell_xy, mouth_xy = place(bell), place(mouth)
    axis = math.atan2(bell_xy[1] - mouth_xy[1], bell_xy[0] - mouth_xy[0])
    return cropped, (left, top), bell_xy, axis, scale


def arcs(canvas, bell_xy, axis, scale):
    """Three arcs of the call in front of the bell, fainter as they go"""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    spread = 34.0
    for index, alpha in enumerate((230, 180, 120)):
        radius = (60 + 55 * index) * scale
        centre = (bell_xy[0] - 20 * scale * math.cos(axis), bell_xy[1] - 20 * scale * math.sin(axis))
        box = (centre[0] - radius, centre[1] - radius, centre[0] + radius, centre[1] + radius)
        start = math.degrees(axis) - spread
        draw.arc(box, start, start + 2 * spread, fill=ARC_COLOUR + (alpha,), width=round(11 * scale))
    glow = layer.filter(ImageFilter.GaussianBlur(5 * SUPER))
    canvas.alpha_composite(glow)
    canvas.alpha_composite(layer)


def vignette(canvas):
    ys, xs = np.mgrid[0:CANVAS, 0:CANVAS].astype(np.float32)
    r = np.sqrt((xs / CANVAS - 0.5) ** 2 + (ys / CANVAS - 0.5) ** 2) / math.sqrt(0.5)
    shade = (np.clip((r - 0.55) / 0.45, 0.0, 1.0) ** 1.6 * 170).astype(np.uint8)
    dark = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    dark.putalpha(Image.fromarray(shade, "L"))
    canvas.alpha_composite(dark)


def main(argv):
    out = Path(argv[1]) if len(argv) > 1 else OUT
    piece, corner, bell_xy, axis, scale = trumpet()
    canvas = ground(bell_xy)
    # a soft shadow under the trumpet, then the trumpet
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    silhouette = Image.new("RGBA", piece.size, (0, 0, 0, 255))
    silhouette.putalpha(piece.getchannel("A").point(lambda a: a * 0.6))
    shadow.alpha_composite(silhouette, (corner[0] + 6 * SUPER, corner[1] + 9 * SUPER))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(5 * SUPER)))
    arcs(canvas, bell_xy, axis, scale)
    canvas.alpha_composite(piece, corner)
    vignette(canvas)
    icon = canvas.resize((SIZE, SIZE), Image.LANCZOS)
    icon.putalpha(255)
    out.parent.mkdir(parents=True, exist_ok=True)
    icon.save(out)
    print(f"{out} {icon.size[0]} x {icon.size[1]}, bell at {bell_xy[0] / SUPER:.0f}, {bell_xy[1] / SUPER:.0f}")


if __name__ == "__main__":
    main(sys.argv)
