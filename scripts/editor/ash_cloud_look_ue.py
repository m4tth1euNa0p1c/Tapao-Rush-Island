"""The ash cloud of the Ash Watcher (T-275) drawn as a smooth grey egg (v1.8 check of 2026-10-10, PIE L_Test_Flat): the
cloud is the engine sphere stretched to CloudRadius x CloudHeight with M_Tiki_AshCloud, translucent unlit, its opacity
only softened at the rim. M_Tiki_AshCloud (/Game/MonkeIsland/Characters/Tiki/Tactics) is rebuilt:
- the opacity is eaten by a billowing 3D noise of the world position that rises with time (the ash churns and climbs,
  the outline is ragged, holes open and close);
- the colour goes from soot to warm grey by the same noise, with a faint ember glow low in the cloud;
- the rim stays soft (fresnel) and the cloud fades where it meets the ground (depth fade): no hard line on the sand.
Same asset, same use (the rules keep pointing at it). Lead session action "ashcloud" (editor open, no game running).
Report: monke_island/Saved/T275/ash-cloud.json. Replays rebuild the same graph.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T275/ash-cloud.json"
PATH = "/Game/MonkeIsland/Characters/Tiki/Tactics/M_Tiki_AshCloud"
MEL = ue.MaterialEditingLibrary
NOISE_SCALE = 0.011          # features about 90 cm across
RISE_CMPS = (6.0, 4.0, 38.0)  # drift of the noise: the ash climbs
DENSITY = 0.9
SOOT = ue.LinearColor(0.018, 0.016, 0.015, 1.0)
GREY = ue.LinearColor(0.085, 0.078, 0.07, 1.0)
EMBER = ue.LinearColor(0.9, 0.22, 0.04, 1.0)
EMBER_GLOW = 0.35
GROUND_FADE_CM = 90.0


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for name, value in props.items():
        expression.set_editor_property(name, value)
    return expression


def link(source, target, source_pin="", target_pin=""):
    if not MEL.connect_material_expressions(source, source_pin, target, target_pin):
        raise RuntimeError(f"link {source.get_name()}.{source_pin} -> {target.get_name()}.{target_pin} refused")


def link_position(source, noise):
    # the pin of the Noise node is named after its position origin (absolute: "World Position")
    for pin in ("World Position", "Translated World Position", "Position"):
        if MEL.connect_material_expressions(source, "", noise, pin):
            return pin
    raise RuntimeError("the position pin of the Noise node was not found")


def main():
    report = {"material": PATH}
    material = ue.EditorAssetLibrary.load_asset(PATH)
    if not material:
        raise RuntimeError(f"{PATH} missing (scripts/editor/tiki_mechanics_setup_ue.py makes it)")
    MEL.delete_all_material_expressions(material)

    # noise position = world position + time x rise
    world = node(material, ue.MaterialExpressionWorldPosition, -1800, 200)
    time = node(material, ue.MaterialExpressionTime, -1800, 330)
    rise = node(material, ue.MaterialExpressionConstant3Vector, -1800, 420, constant=ue.LinearColor(*RISE_CMPS, 0.0))
    drift = node(material, ue.MaterialExpressionMultiply, -1600, 360)
    link(time, drift, "", "A")
    link(rise, drift, "", "B")
    moved = node(material, ue.MaterialExpressionSubtract, -1450, 260)
    link(world, moved, "", "A")
    link(drift, moved, "", "B")
    noise = node(material, ue.MaterialExpressionNoise, -1250, 260, scale=NOISE_SCALE, levels=4, output_min=0.0,
                 output_max=1.0, turbulence=True, quality=1)
    report["noise_pin"] = link_position(moved, noise)

    # opacity = (1 - fresnel) x saturate(noise x 1.9 - 0.35) x density, faded at the ground
    fresnel = node(material, ue.MaterialExpressionFresnel, -1250, 520, exponent=1.6)
    rim = node(material, ue.MaterialExpressionOneMinus, -1050, 520)
    link(fresnel, rim)
    gain = node(material, ue.MaterialExpressionMultiply, -1050, 260, const_b=1.9)
    link(noise, gain, "", "A")
    cut = node(material, ue.MaterialExpressionSubtract, -900, 260, const_b=0.35)
    link(gain, cut, "", "A")
    billows = node(material, ue.MaterialExpressionSaturate, -750, 260)
    link(cut, billows)
    body = node(material, ue.MaterialExpressionMultiply, -600, 400)
    link(billows, body, "", "A")
    link(rim, body, "", "B")
    dense = node(material, ue.MaterialExpressionMultiply, -450, 400, const_b=DENSITY)
    link(body, dense, "", "A")
    fade = node(material, ue.MaterialExpressionDepthFade, -250, 400, fade_distance_default=GROUND_FADE_CM)
    link(dense, fade, "", "Opacity")

    # colour: soot to warm grey by the noise, plus a faint ember glow in the low, dense parts
    soot = node(material, ue.MaterialExpressionConstant3Vector, -900, -260, constant=SOOT)
    grey = node(material, ue.MaterialExpressionConstant3Vector, -900, -160, constant=GREY)
    ash = node(material, ue.MaterialExpressionLinearInterpolate, -650, -200)
    link(soot, ash, "", "A")
    link(grey, ash, "", "B")
    link(noise, ash, "", "Alpha")
    ember = node(material, ue.MaterialExpressionConstant3Vector, -900, 0, constant=EMBER)
    hot = node(material, ue.MaterialExpressionPower, -900, 90, const_exponent=6.0)
    link(noise, hot, "", "Base")
    glow = node(material, ue.MaterialExpressionMultiply, -650, 40, const_b=EMBER_GLOW)
    link(hot, glow, "", "A")
    embers = node(material, ue.MaterialExpressionMultiply, -500, 0)
    link(ember, embers, "", "A")
    link(glow, embers, "", "B")
    colour = node(material, ue.MaterialExpressionAdd, -300, -120)
    link(ash, colour, "", "A")
    link(embers, colour, "", "B")

    if not MEL.connect_material_property(colour, "", ue.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("emissive not connected")
    if not MEL.connect_material_property(fade, "", ue.MaterialProperty.MP_OPACITY):
        raise RuntimeError("opacity not connected")
    MEL.recompile_material(material)
    report["expressions"] = MEL.get_num_material_expressions(material)
    report["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"ASH_CLOUD expressions={report['expressions']} saved={report['saved']}")


main()
