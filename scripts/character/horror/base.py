"""Socle du package horror (T-274) : chargement des modules tiki, petit constructeur de graphes de matériaux (NB),
matériaux de base partagés.

Rapatrié du 2e PC (OneDrive/maquette-blender/factory/kits/cursedcoco_kit.py, maquettes mob-coco-maudit /
mob-coco-realiste, 2026-10-07) : classe NB et matériaux mat_bone / mat_leather / mat_claw, copiés tels quels (seuls
les imports changent). Le package tiki (scripts/character/tiki) est celui du dépôt, identique à la copie du 2e PC.
"""
import importlib
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
CHAR = os.path.dirname(HERE)                       # scripts/character
ROOT = os.path.abspath(os.path.join(CHAR, "..", ".."))
ART = os.path.join(ROOT, "art")
TAU = 2 * math.pi
T = {}            # modules tiki : kit, mats, creatures, rig, anims, export, stage


def load():
    """Charge (ou recharge) le package tiki du dépôt ; SPECS remis à neuf."""
    if CHAR not in sys.path:
        sys.path.insert(0, CHAR)
    import tiki  # noqa: F401
    from tiki import anims, creatures, export, kit, mats, rig, stage
    for m in (kit, mats, creatures, rig, anims, stage, export):
        importlib.reload(m)
    T.update(anims=anims, creatures=creatures, export=export, kit=kit, mats=mats, rig=rig, stage=stage)
    T["repo"] = ROOT
    return T


def log(*a):
    print("[horror]", *a, flush=True)


def set_action(arm, act):
    ad = arm.animation_data_create()
    ad.action = act
    if act is not None and hasattr(ad, "action_slot") and act.slots:
        try:
            ad.action_slot = act.slots[0]
        except (TypeError, RuntimeError):
            pass


def srgb(h):
    return T["mats"].srgb(h)


# ------------------------------------------------------------------ graphes (copie de cursedcoco_kit.NB)
class NB:
    """Petit constructeur de graphes Principled (coordonnées Generated = mètres, cf. kit.fixed_texspace)."""

    def __init__(self, name):
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        mat.use_nodes = True
        self.mat = mat
        self.nt = mat.node_tree
        self.nt.nodes.clear()
        self.out = self.n("ShaderNodeOutputMaterial")
        self.bs = self.n("ShaderNodeBsdfPrincipled")
        self.l(self.bs.outputs[0], self.out.inputs["Surface"])
        self._gen = None

    def n(self, t, **kw):
        node = self.nt.nodes.new(t)
        for k, v in kw.items():
            setattr(node, k, v)
        return node

    def l(self, a, b):
        self.nt.links.new(a, b)

    def s(self, sockets, *keys):
        return T["mats"].sock(sockets, *keys)

    def put(self, sock, v):
        if isinstance(v, (int, float, tuple, list)):
            sock.default_value = v
        else:
            self.l(v, sock)

    def gen(self):
        if self._gen is None:
            self._gen = self.n("ShaderNodeTexCoord").outputs["Generated"]
        return self._gen

    def vec(self, scale=(1, 1, 1), loc=(0, 0, 0)):
        m = self.n("ShaderNodeMapping")
        self.l(self.gen(), m.inputs["Vector"])
        m.inputs["Scale"].default_value = scale
        m.inputs["Location"].default_value = loc
        return m.outputs["Vector"]

    def noise(self, scale, detail=4.0, rough=0.55, dist=0.0, vec=None):
        x = self.n("ShaderNodeTexNoise")
        self.l(vec if vec is not None else self.gen(), x.inputs["Vector"])
        x.inputs["Scale"].default_value = scale
        x.inputs["Detail"].default_value = detail
        x.inputs["Roughness"].default_value = rough
        x.inputs["Distortion"].default_value = dist
        return x.outputs["Fac"]

    def edges(self, scale, vec=None):
        x = self.n("ShaderNodeTexVoronoi", feature="DISTANCE_TO_EDGE")
        self.l(vec if vec is not None else self.gen(), x.inputs["Vector"])
        x.inputs["Scale"].default_value = scale
        return x.outputs["Distance"]

    def ramp(self, fac, stops):
        r = self.n("ShaderNodeValToRGB")
        self.put(r.inputs["Fac"], fac)
        els = r.color_ramp.elements
        while len(els) < len(stops):
            els.new(0.5)
        for e, (pos, col) in zip(els, stops):
            e.position = pos
            e.color = srgb(col) if isinstance(col, str) else (list(col) + [1.0] if len(col) == 3 else col)
        return r.outputs["Color"]

    def fac(self, fac, lo, hi):
        """smoothstep(lo, hi, fac) -> float"""
        r = self.n("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP")
        self.put(r.inputs["Value"], fac)
        r.inputs["From Min"].default_value = lo
        r.inputs["From Max"].default_value = hi
        return r.outputs["Result"]

    def mix(self, f, a, b, blend="MIX"):
        m = self.n("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        self.put(self.s(m.inputs, "Factor_Float"), f)
        self.put(self.s(m.inputs, "A_Color"), a)
        self.put(self.s(m.inputs, "B_Color"), b)
        return self.s(m.outputs, "Result_Color")

    def math(self, op, a, b=0.0, c=None, clamp=False):
        m = self.n("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.put(m.inputs[0], a)
        self.put(m.inputs[1], b)
        if c is not None:
            self.put(m.inputs[2], c)
        return m.outputs[0]

    def cavity(self, dist=0.04):
        a = self.n("ShaderNodeAmbientOcclusion")
        a.inputs["Distance"].default_value = dist
        return self.math("SUBTRACT", 1.0, a.outputs["AO"], clamp=True)

    def bump(self, height, strength=0.4, dist=0.003, normal=None):
        b = self.n("ShaderNodeBump")
        b.inputs["Strength"].default_value = strength
        b.inputs["Distance"].default_value = dist
        self.l(height, b.inputs["Height"])
        if normal is not None:
            self.l(normal, b.inputs["Normal"])
        return b.outputs["Normal"]

    def finish(self, base, rough, normal=None, sss=0.0, emis=None, emis_strength=0.0, fallback="808080"):
        self.put(self.s(self.bs.inputs, "Base Color"), base)
        self.put(self.s(self.bs.inputs, "Roughness"), rough)
        if normal is not None:
            self.l(normal, self.s(self.bs.inputs, "Normal"))
        if sss:
            self.s(self.bs.inputs, "Subsurface Weight").default_value = sss
            try:
                self.s(self.bs.inputs, "Subsurface Radius").default_value = (0.9, 0.18, 0.12)
                self.s(self.bs.inputs, "Subsurface Scale").default_value = 0.025
            except KeyError:
                pass
        if emis is not None:
            self.put(self.s(self.bs.inputs, "Emission Color"), emis)
            self.s(self.bs.inputs, "Emission Strength").default_value = emis_strength
        self.mat.diffuse_color = srgb(fallback)
        self.mat["realistic"] = True
        return self.mat


# ------------------------------------------------------------------ matériaux de base (copie de cursedcoco_kit)
def mat_bone(name, light="E3D5B0", dark="B9A47A", dirt="4A3A22", crack="2E2214", yellow=0.0):
    g = NB(name)
    tone = g.ramp(g.noise(9.0, 5.0, 0.6), [(0.30, dark), (0.72, light)])
    tone = g.mix(g.math("MULTIPLY", g.fac(g.noise(2.5, 4.0, 0.6), 0.5, 0.75), 0.6), tone, srgb("8C7A56"))
    if yellow:
        tone = g.mix(yellow, tone, srgb("C7AE6E"), "MULTIPLY")
    cav = g.cavity(0.035)
    grime = g.math("ADD", g.math("MULTIPLY", cav, 2.6), g.math("MULTIPLY", g.fac(g.noise(4.0, 3.0), 0.48, 0.78), 0.55),
                   clamp=True)
    col = g.mix(grime, tone, srgb(dirt))
    cr = g.fac(g.edges(7.0, g.vec((1.0, 1.0, 1.6))), 0.035, 0.0)          # 1 dans les fissures
    cr2 = g.fac(g.edges(19.0), 0.02, 0.0)
    cracks = g.math("MULTIPLY", g.math("MAXIMUM", cr, g.math("MULTIPLY", cr2, 0.6)),
                    g.fac(g.noise(3.0, 2.0), 0.45, 0.6))
    col = g.mix(g.math("MULTIPLY", cracks, 0.85), col, srgb(crack))
    chips = g.fac(g.noise(30.0, 6.0, 0.7), 0.70, 0.76)                  # éclats plus clairs
    col = g.mix(g.math("MULTIPLY", chips, 0.35), col, srgb("F2EAD4"))
    rough = g.math("MULTIPLY_ADD", g.noise(14.0, 3.0), 0.35, 0.42)
    h = g.math("SUBTRACT", g.math("MULTIPLY", g.noise(60.0, 4.0), 0.25), g.math("MULTIPLY", cracks, 0.9))
    return g.finish(col, rough, g.bump(h, 0.45, 0.004), fallback=light)


def mat_leather(name):
    g = NB(name)
    col = g.ramp(g.noise(25.0, 5.0, 0.6), [(0.3, "1C100A"), (0.7, "4A2E1C")])
    wear = g.fac(g.noise(9.0, 4.0), 0.62, 0.7)
    col = g.mix(g.math("MULTIPLY", wear, 0.5), col, srgb("7A5A40"))
    return g.finish(col, g.math("MULTIPLY_ADD", wear, -0.2, 0.7), g.bump(g.noise(140.0), 0.3, 0.001),
                    fallback="3A2316")


def mat_claw(name):
    g = NB(name)
    col = g.ramp(g.noise(12.0, 3.0), [(0.3, "070606"), (0.75, "221A1A")])
    return g.finish(col, g.math("MULTIPLY_ADD", g.noise(20.0), 0.2, 0.16), g.bump(g.noise(80.0), 0.15, 0.001),
                    fallback="141214")
