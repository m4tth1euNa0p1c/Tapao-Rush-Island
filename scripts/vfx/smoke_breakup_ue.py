"""T-280: the firearm smoke and the machine gun's steam were drawn as round, evenly lit balls (v1.8 check of 2026-10-10,
the machine gun's overheat in PIE). M_Firearm_Smoke (/Game/MonkeIsland/VFX/Weapons, scripts/vfx/build_firearm_vfx_ue.py)
is rebuilt with a broken-up shape:
- the soft disc of the sprite is eaten by a 3D noise of the world position: every puff has its own outline, and the
  outline boils as the puff drifts through the noise;
- a little shading inside (colour x 0.65..1 by the same noise), so a puff in full sun is no longer one flat white disc.
Same parameter (Opacity, its value kept), same use (Niagara sprites, translucent, volumetric lighting): every smoke
system of the firearms keeps working with it.
Lead session action "smokebreakup" (editor open, no game running). Report: monke_island/Saved/T280/smoke.json.
Replays rebuild the same graph.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T280/smoke.json"
PATH = "/Game/MonkeIsland/VFX/Weapons/M_Firearm_Smoke"
MEL = ue.MaterialEditingLibrary
# noise: features about 1 / NOISE_SCALE cm across; how much of the disc's edge it eats; how dark the hollows get
NOISE_SCALE = 0.035
DISC_GAIN = 2.0
EDGE_EATEN = 1.25
SHADE_LOW = 0.65


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for name, value in props.items():
        expression.set_editor_property(name, value)
    return expression


def link(source, target, source_pin="", target_pin=""):
    if not MEL.connect_material_expressions(source, source_pin, target, target_pin):
        raise RuntimeError(f"link {source.get_name()}.{source_pin} -> {target.get_name()}.{target_pin} refused")


def noise_function():
    for name in ("NOISEFUNCTION_SIMPLEX_TEX", "NOISEFUNCTION_GRADIENT_TEX", "NOISEFUNCTION_VALUE_TEX"):
        value = getattr(ue.NoiseFunction, name, None)
        if value is not None:
            return value
    return None


def main():
    report = {"material": PATH}
    material = ue.EditorAssetLibrary.load_asset(PATH)
    if not material:
        raise RuntimeError(f"{PATH} missing (scripts/vfx/build_firearm_vfx_ue.py builds it)")
    opacity_value = MEL.get_material_default_scalar_parameter_value(material, "Opacity")
    report["opacity_kept"] = opacity_value
    MEL.delete_all_material_expressions(material)

    particle = node(material, ue.MaterialExpressionParticleColor, -1300, -250)
    opacity = node(material, ue.MaterialExpressionScalarParameter, -500, 420, parameter_name="Opacity", group="Firearm",
                   default_value=opacity_value if opacity_value else 0.6,
                   desc="Density of the smoke or the dust (the particle alpha multiplies it)")
    uv = node(material, ue.MaterialExpressionTextureCoordinate, -1700, 0)
    centre = node(material, ue.MaterialExpressionConstant2Vector, -1700, 120, r=0.5, g=0.5)
    disc = node(material, ue.MaterialExpressionSphereMask, -1450, 0, attenuation_radius=0.5, hardness_percent=0.0)
    link(uv, disc, "", "A")
    link(centre, disc, "", "B")

    # the Noise node samples the world position when its position pin is left free
    noise_props = {"scale": NOISE_SCALE, "levels": 3, "output_min": 0.0, "output_max": 1.0, "turbulence": False, "quality": 1}
    function = noise_function()
    if function is not None:
        noise_props["noise_function"] = function
    noise = node(material, ue.MaterialExpressionNoise, -1450, 260, **noise_props)
    report["noise_function"] = str(function)

    # shape = saturate(disc x DISC_GAIN - (1 - noise) x EDGE_EATEN): solid core, ragged edge, nothing outside the disc
    hollow = node(material, ue.MaterialExpressionOneMinus, -1200, 260)
    link(noise, hollow)
    eaten = node(material, ue.MaterialExpressionMultiply, -1000, 260, const_b=EDGE_EATEN)
    link(hollow, eaten, "", "A")
    core = node(material, ue.MaterialExpressionMultiply, -1200, 40, const_b=DISC_GAIN)
    link(disc, core, "", "A")
    raw = node(material, ue.MaterialExpressionSubtract, -800, 120)
    link(core, raw, "", "A")
    link(eaten, raw, "", "B")
    shape = node(material, ue.MaterialExpressionSaturate, -650, 120)
    link(raw, shape)

    alpha = node(material, ue.MaterialExpressionMultiply, -450, 200)
    link(particle, alpha, "A", "A")
    link(shape, alpha, "", "B")
    final = node(material, ue.MaterialExpressionMultiply, -250, 300)
    link(alpha, final, "", "A")
    link(opacity, final, "", "B")

    shade = node(material, ue.MaterialExpressionLinearInterpolate, -800, -120, const_a=SHADE_LOW, const_b=1.0)
    link(noise, shade, "", "Alpha")
    colour = node(material, ue.MaterialExpressionMultiply, -450, -200)
    link(particle, colour, "RGB", "A")
    link(shade, colour, "", "B")

    if not MEL.connect_material_property(colour, "", ue.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("base colour not connected")
    if not MEL.connect_material_property(final, "", ue.MaterialProperty.MP_OPACITY):
        raise RuntimeError("opacity not connected")
    MEL.recompile_material(material)
    report["expressions"] = MEL.get_num_material_expressions(material)
    report["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"SMOKE_BREAKUP expressions={report['expressions']} saved={report['saved']}")


main()
