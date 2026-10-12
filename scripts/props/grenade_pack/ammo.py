"""Shared ammunition pickups (T-253, owner 2026-10-09 "modèles de munitions"): static meshes with baked PBR atlases.

  SM_Ammo_GrenadeBag   leather bag of three grenados (fuses out of the open mouth)         ~0.17 x 0.17 x 0.24 m
  SM_Ammo_CrateMixed   small open crate of mixed ammunition for the fort's loot: paper cartridges, lead balls,
                       shotgun shells, on a cloth bed, iron corner straps                  ~0.24 x 0.16 x 0.12 m
  SM_Ammo_PowderKeg    small powder keg, oak staves, three rusted iron hoops, side bung, top cork   ~0.19 x 0.19 x 0.25 m

The other ammunition (cartridges, cartridge box, powder horn, pouch, shotgun shells) belongs to the packs of each
weapon: not redone here. Metres, bottom-centre pivot, UCX_<Name>_00 convex hull in the FBX, 1024 atlases
(T_<Name>_D sRGB, T_<Name>_N DirectX, T_<Name>_ORM linear), FBX + GLB, ammo-manifest.json, renders/product-ammo.png.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/grenade_pack/ammo.py -- [options]
    --only GrenadeBag,PowderKeg   subset        --bake-size 1024        --no-bake (quick look)
    --out DIR (default build/packs/ammo)        --save (ammo.blend)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import common as C  # noqa: E402
import grenade_model as G  # noqa: E402
from export_pack import mesh_export  # noqa: E402

V = C.V
TAU = C.TAU


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


# ------------------------------------------------------------------------------------------------- materials

def mat_keg_wood(name, staves=16):
    """Oak staves: grain along Z plus a dark seam every 360/staves degrees (object space)."""
    m = C.mat_wood(name, light=(0.34, 0.21, 0.10), dark=(0.15, 0.085, 0.04), scale=24.0, axis=(0, 0, 1), rough=0.62, seed=2.5)
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    co = C._coords(nt, 'Object')
    sep = nt.nodes.new('ShaderNodeSeparateXYZ')
    nt.links.new(co, sep.inputs['Vector'])
    ang = C._math(nt, 'ARCTAN2', sep.outputs['Y'], sep.outputs['X'])
    t = C._math(nt, 'FRACT', C._math(nt, 'MULTIPLY', ang.outputs[0], staves / TAU).outputs[0])
    d = C._math(nt, 'ABSOLUTE', C._math(nt, 'SUBTRACT', t.outputs[0], 0.5).outputs[0])
    seam = C._math(nt, 'GREATER_THAN', d.outputs[0], 0.47)
    old_col = bsdf.inputs['Base Color'].links[0].from_socket
    dark = C._mix(nt, old_col, (0.05, 0.03, 0.015, 1), seam.outputs[0])
    nt.links.new(C._mix_out(dark), bsdf.inputs['Base Color'])
    bump = next(n for n in nt.nodes if n.type == 'BUMP')
    old_h = bump.inputs['Height'].links[0].from_socket
    h = C._math(nt, 'SUBTRACT', old_h, C._math(nt, 'MULTIPLY', seam.outputs[0], 1.5).outputs[0])
    nt.links.new(h.outputs[0], bump.inputs['Height'])
    bump.inputs['Strength'].default_value = 0.5
    return m


def ammo_materials():
    return dict(
        leather=C.mat_leather('MAT-Ammo-Leather', color=(0.26, 0.14, 0.07), dark=(0.11, 0.055, 0.025), seed=4.0),
        string=C.mat_cord('MAT-Ammo-String', color=(0.52, 0.42, 0.25), dark=(0.26, 0.19, 0.09), twist=24.0),
        crate=C.mat_wood('MAT-Ammo-Crate', light=(0.52, 0.36, 0.18), dark=(0.28, 0.17, 0.07), scale=22.0, axis=(1, 0, 0), rough=0.7, seed=7.0),
        crate_end=C.mat_wood('MAT-Ammo-CrateEnd', light=(0.50, 0.34, 0.17), dark=(0.27, 0.16, 0.07), scale=22.0, axis=(0, 1, 0), rough=0.7, seed=9.0),
        strap=C.mat_cast_iron('MAT-Ammo-Strap', base=(0.05, 0.045, 0.04), rust_amount=0.6, pit_scale=300.0, seed=5.0),
        paper=C.mat_paper('MAT-Ammo-Paper', color=(0.78, 0.70, 0.54), seed=3.0),
        lead=C.mat_metal('MAT-Ammo-Lead', color=(0.30, 0.31, 0.33), rough=0.48, metallic=0.9, seed=1.0, smudge=0.3),
        shell=C.mat_paper('MAT-Ammo-ShellPaper', color=(0.48, 0.07, 0.05), rough=0.75, seed=6.0),
        brass=C.mat_metal('MAT-Ammo-Brass', color=(0.62, 0.45, 0.18), rough=0.36, metallic=1.0, seed=2.0, smudge=0.25),
        cloth=C.mat_leather('MAT-Ammo-Cloth', color=(0.14, 0.13, 0.11), dark=(0.07, 0.065, 0.05), rough=0.9, seed=8.0, scale=400.0),
        keg=mat_keg_wood('MAT-Ammo-KegOak'),
        keghead=C.mat_wood('MAT-Ammo-KegHead', light=(0.36, 0.22, 0.10), dark=(0.16, 0.09, 0.04), scale=26.0, axis=(1, 0, 0), rough=0.62, seed=11.0),
        hoop=C.mat_cast_iron('MAT-Ammo-Hoop', base=(0.06, 0.05, 0.045), rust_amount=0.75, pit_scale=260.0, seed=6.5),
        cork=C.mat_paper('MAT-Ammo-Cork', color=(0.55, 0.40, 0.22), rough=0.9, seed=12.0),
    )


# ------------------------------------------------------------------------------------------------- builders

def build_grenade_bag(coll, M):
    """Leather bag, mouth open, three grenados inside with their fuses out."""
    parts = []
    parts.append(C.lathe('AmmoBag_Body', [(0.0, 0.030), (0.004, 0.058), (0.015, 0.074), (0.040, 0.084), (0.075, 0.084), (0.110, 0.074),
                                          (0.140, 0.058), (0.152, 0.054), (0.160, 0.060), (0.166, 0.064), (0.164, 0.058), (0.150, 0.050),
                                          (0.120, 0.048), (0.100, 0.040), (0.098, 0.006)],
                         (0, 0, 0), M['leather'], coll, segs=32, cap_start=True, cap_end=True))
    parts.append(C.torus('AmmoBag_String', (0, 0, 0.146), 0.058, 0.0028, M['string'], coll, axis='Z', segs=32, minor=6))
    for k, (dx, dy) in enumerate(((0.050, 0.030), (0.040, -0.044))):
        parts.append(C.tube('AmmoBag_StringEnd%d' % k, [V(dx, dy, 0.146), V(dx * 1.25, dy * 1.25, 0.120), V(dx * 1.35, dy * 1.35, 0.092)],
                            [0.0028, 0.0026, 0.0022], M['string'], coll, segs=8))
    for k, (dx, dy, dz) in enumerate(((0.026, 0.018, 0.0), (-0.028, 0.014, -0.004), (0.0, -0.028, 0.002))):
        # ball centres at z = 0.098 inside the belly (radius 0.08 there): the plugs reach the mouth, the fuses rise above it
        grenade = G.build_grenade(coll, only_grenade=True, offset=V(dx, dy, 0.098 - G.BALL.z + dz), suffix='_%d' % k)
        for objs in grenade.values():
            for ob in objs:
                ob.rotation_euler = (0, 0, TAU * (k * 0.37 + 0.1))
            parts.extend(objs)
    pts = [V(0.086 * math.cos(TAU * i / 8), 0.086 * math.sin(TAU * i / 8), z) for z in (0.0, 0.08, 0.16) for i in range(8)]
    pts += [V(0.04 * math.cos(TAU * i / 8), 0.04 * math.sin(TAU * i / 8), 0.225) for i in range(8)]
    return parts, pts, dict(role='bag of three grenados (reload reserve of AGrenadeWeapon)', proposed=dict(grenades=3))


def build_crate_mixed(coll, M):
    """Small open crate: planks, iron corner straps, cloth bed, paper cartridges, lead balls, shotgun shells."""
    parts = []
    L, W, H, t = 0.24, 0.16, 0.12, 0.012
    parts.append(C.box('AmmoCrate_Bottom', (0, 0, t / 2), (L, W, t), M['crate'], coll, bevel=0.0015))
    for s in (-1, 1):
        parts.append(C.box('AmmoCrate_Side%+d' % s, (0, s * (W / 2 - t / 2), H / 2), (L, t, H), M['crate'], coll, bevel=0.0015))
        parts.append(C.box('AmmoCrate_End%+d' % s, (s * (L / 2 - t / 2), 0, H / 2), (t, W - 2 * t, H), M['crate_end'], coll, bevel=0.0015))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(C.box('AmmoCrate_StrapX%+d%+d' % (sx, sy), (sx * (L / 2 - 0.012), sy * (W / 2 + 0.0015), H * 0.5), (0.026, 0.003, H * 0.96), M['strap'], coll, bevel=0.0008))
            parts.append(C.box('AmmoCrate_StrapY%+d%+d' % (sx, sy), (sx * (L / 2 + 0.0015), sy * (W / 2 - 0.012), H * 0.5), (0.003, 0.026, H * 0.96), M['strap'], coll, bevel=0.0008))
            for z in (0.02, H - 0.02):
                parts.append(C.sphere('AmmoCrate_Nail%+d%+d%.2f' % (sx, sy, z), (sx * (L / 2 + 0.0035), sy * (W / 2 - 0.012), z), (0.003, 0.003, 0.003), M['strap'], coll, seg=10, ring=5))
    # cloth bed filling the crate up to 6.5 cm
    parts.append(C.box('AmmoCrate_Cloth', (0, 0, t + 0.026), (L - 2 * t - 0.004, W - 2 * t - 0.004, 0.052), M['cloth'], coll, bevel=0.012))
    top = t + 0.052
    # paper cartridges lying along X on the left half
    for k, y in enumerate((-0.045, -0.026, -0.007, 0.012)):
        x = -0.065 + (k % 2) * 0.012
        parts.append(C.tube('AmmoCrate_Cartridge%d' % k, [V(x - 0.030, y, top + 0.009), V(x + 0.022, y, top + 0.009), V(x + 0.030, y, top + 0.009)],
                            [0.0082, 0.0086, 0.0070], M['paper'], coll, segs=14))
        parts.append(C.torus('AmmoCrate_CartridgeTie%d' % k, (x + 0.021, y, top + 0.009), 0.0086, 0.0012, M['string'], coll, axis='X', segs=14, minor=5))
    # lead balls in the front right corner
    for k, (x, y, z) in enumerate(((0.060, 0.040, 0.0), (0.078, 0.028, 0.0), (0.062, 0.020, 0.0), (0.080, 0.048, 0.0), (0.045, 0.034, 0.0), (0.070, 0.036, 0.015))):
        parts.append(C.sphere('AmmoCrate_Ball%d' % k, (x, y, top + 0.0085 + z), (0.0085, 0.0085, 0.0085), M['lead'], coll, seg=20, ring=10))
    # shotgun shells lying along Y on the right, brass base toward -Y
    for k, x in enumerate((0.012, 0.034, 0.056)):
        y0 = -0.052 + (k % 2) * 0.006
        parts.append(C.tube('AmmoCrate_Shell%d' % k, [V(x, y0 + 0.012, top + 0.0095), V(x, y0 + 0.060, top + 0.0095)], [0.0093, 0.0093], M['shell'], coll, segs=14))
        parts.append(C.lathe('AmmoCrate_ShellBase%d' % k, [(0.0, 0.0), (0.0, 0.0102), (0.013, 0.0102), (0.014, 0.0094), (0.015, 0.0)],
                             (x, y0, top + 0.0095), M['brass'], coll, segs=16, axis='Y'))
    pts = [V(sx * L / 2, sy * W / 2, z) for sx in (-1, 1) for sy in (-1, 1) for z in (0.0, H)]
    return parts, pts, dict(role='mixed ammunition for the fort loot (pistol / musket cartridges, balls, shotgun shells)',
                            proposed=dict(cartridges=4, balls=6, shells=3))


def build_powder_keg(coll, M):
    """Small powder keg: bulged oak staves, three iron hoops, side bung with its spout, top cork."""
    parts = []
    H = 0.24
    profile = [(0.0, 0.080), (0.02, 0.085), (0.06, 0.091), (0.12, 0.094), (0.18, 0.091), (0.22, 0.085), (H, 0.080)]
    parts.append(C.lathe('AmmoKeg_Staves', profile, (0, 0, 0), M['keg'], coll, segs=48))
    for z, r in ((0.004, 0.079), (H - 0.004, 0.079)):
        parts.append(C.sphere('AmmoKeg_Head%.2f' % z, (0, 0, z), (r * 0.94, r * 0.94, 0.006), M['keghead'], coll, seg=40, ring=8))
    for k, z in enumerate((0.030, 0.120, 0.210)):
        r = max(rr for zz, rr in profile if abs(zz - z) <= 0.06) + 0.004
        parts.append(C.lathe('AmmoKeg_Hoop%d' % k, [(z - 0.012, r - 0.003), (z - 0.011, r), (z + 0.011, r), (z + 0.012, r - 0.003)], (0, 0, 0), M['hoop'], coll, segs=48))
        for i in range(2):
            a = TAU * (i / 2 + 0.15 + 0.1 * k)
            parts.append(C.sphere('AmmoKeg_Rivet%d%d' % (k, i), (r * math.cos(a), r * math.sin(a), z), (0.004, 0.004, 0.004), M['hoop'], coll, seg=10, ring=5))
    # bung on the side (toward -Y: the front) and the cork on top
    parts.append(C.lathe('AmmoKeg_Bung', [(0.0, 0.0), (0.0, 0.016), (0.014, 0.016), (0.015, 0.011), (0.022, 0.011), (0.023, 0.0)], (0, -0.088, 0.10),
                         M['hoop'], coll, segs=18, axis='Y'))
    parts.append(C.lathe('AmmoKeg_BungCork', [(0.0, 0.0), (0.0, 0.009), (0.030, 0.009), (0.031, 0.0)], (0, -0.108, 0.10), M['cork'], coll, segs=14, axis='Y'))
    parts.append(C.lathe('AmmoKeg_TopCork', [(-0.004, 0.0), (-0.004, 0.012), (0.016, 0.013), (0.018, 0.0)], (0, 0.02, H), M['cork'], coll, segs=16, cap_end=True))
    pts = [V(0.095 * math.cos(TAU * i / 10), 0.095 * math.sin(TAU * i / 10), z) for z in (0.0, 0.12, H) for i in range(10)]
    return parts, pts, dict(role='small powder keg (loot, explosive barrel candidate)', proposed=dict(powder_charges=20))


BUILDERS = {'GrenadeBag': build_grenade_bag, 'CrateMixed': build_crate_mixed, 'PowderKeg': build_powder_keg}


def build_pickup(name, scene, M, out, do_bake, size, ao_samples):
    """One pickup: parts -> joined mesh (bottom-centre pivot) -> packed UVs -> baked atlas -> UCX."""
    coll = bpy.data.collections.new('COL-Ammo-' + name)
    scene.collection.children.link(coll)
    parts, hull_points, info = BUILDERS[name](coll, M)
    ob = C.join_parts('SM_Ammo_' + name, {'root': parts}, coll)
    # bottom-centre pivot: the parts are built with z >= 0 around the axis; re-centre x, y and the lowest vertex on z = 0
    lo = Vector((min(v.co.x for v in ob.data.vertices), min(v.co.y for v in ob.data.vertices), min(v.co.z for v in ob.data.vertices)))
    hi = Vector((max(v.co.x for v in ob.data.vertices), max(v.co.y for v in ob.data.vertices), max(v.co.z for v in ob.data.vertices)))
    pivot = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    ob.data.transform(Matrix.Translation(-pivot))
    C.pack_uvs(ob)
    textures = {}
    if do_bake:
        bake_scene = bpy.data.scenes.new('SCN-Bake-' + name)
        bake_scene.collection.children.link(coll)
        paths = C.bake_atlas(bake_scene, ob, 'Ammo_' + name, size, Path(out) / 'textures', ao_samples=ao_samples)
        bake_scene.collection.children.unlink(coll)
        bpy.context.window.scene = scene
        textures = {k: Path(v).relative_to(Path(out)).as_posix() for k, v in paths.items()}
    ucx = C.convex('UCX_SM_Ammo_%s_00' % name, [p - pivot for p in hull_points], coll)
    return dict(object=ob, ucx=ucx, collection=coll, pivot=pivot, textures=textures, info=info)


def export_pickup(root, built):
    ob, ucx = built['object'], built['ucx']
    rows = {}
    for kind in ('fbx', 'glb'):
        path = root / 'exports' / (ob.name + '.' + kind)
        mesh_export(path, [ob], kind, [ucx] if kind == 'fbx' else ())
        rows[kind] = dict(path=path.relative_to(root).as_posix(), sha256=sha(path))
    return rows


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--only', default='')
    p.add_argument('--bake-size', type=int, default=1024)
    p.add_argument('--ao-samples', type=int, default=48)
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default=str(REPO / 'build' / 'packs' / 'ammo'))
    args = p.parse_args(argv)
    out = Path(args.out)
    if not out.is_absolute():
        out = REPO / out
    out.mkdir(parents=True, exist_ok=True)
    names = [n for n in args.only.split(',') if n] or list(BUILDERS)
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    scene = bpy.context.scene
    scene.name = 'SCN-ammo'
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.length_unit = 'METERS'
    M = ammo_materials()
    built = {}
    for name in names:
        print('[ammo] ' + name, flush=True)
        built[name] = build_pickup(name, scene, M, out, not args.no_bake, args.bake_size, args.ao_samples)
    assets, materials, pickups = [], {}, []
    for name, b in built.items():
        ob = b['object']
        bounds = C.world_bounds([ob])
        files = export_pickup(out, b)
        material = ob.data.materials[0].name if ob.data.materials and ob.data.materials[0] else ''
        # one asset row per file, as the weapon manifests (import_pirate_weapons.py reads path / sha256 / view)
        for kind in ('fbx', 'glb'):
            row = dict(path=files[kind]['path'], type='static_mesh', skeleton_group='none', sha256=files[kind]['sha256'],
                       view='pickup' if kind == 'fbx' else 'preview', name=ob.name, pivot='bottom_center', units='m',
                       collision='UCX_%s_00 (convex hull, in the FBX)' % ob.name if kind == 'fbx' else 'none',
                       size_m=[round(v, 4) for v in bounds['size']], bounds_m=bounds, triangles=C.triangle_count([ob]),
                       material=material, textures=b['textures'], **b['info'])
            assets.append(row)
        pickups.append(dict(name=ob.name, fbx=files['fbx']['path'], glb=files['glb']['path'], size_m=[round(v, 4) for v in bounds['size']],
                            triangles=C.triangle_count([ob]), material=material, textures=b['textures'], **b['info']))
        if material and b['textures']:
            materials[material] = dict(base_color=[1.0, 1.0, 1.0, 1.0], metallic=0.0, roughness=0.5, textures=b['textures'],
                                       base_color_texture=b['textures'].get('BaseColor'))
        print('[ammo] %s: %d tris, size %s' % (ob.name, C.triangle_count([ob]), [round(v, 3) for v in bounds['size']]), flush=True)
    # product view: the pickups side by side
    product = C.studio_scene('SCN-Product-Ammo', Vector((0.0, 0.0, 0.10)), 0.36, res=(1600, 900))
    xs = {'GrenadeBag': -0.30, 'CrateMixed': 0.02, 'PowderKeg': 0.33}
    for name, b in built.items():
        product.collection.children.link(b['collection'])
        b['ucx'].hide_render = True
        b['object'].location = (xs.get(name, 0.0), 0.0, 0.0)
        b['object'].rotation_euler = (0, 0, math.radians(-25))
    C.render_scene(product, out / 'renders' / 'product-ammo.png')
    for b in built.values():
        b['object'].location = (0, 0, 0)
        b['object'].rotation_euler = (0, 0, 0)
    manifest = dict(format='ammo-pickups/1', version='0.1.0-local', built_by='scripts/props/grenade_pack/ammo.py (T-253)',
                    units='metres (FBX UnitScaleFactor 100: import with bConvertSceneUnit, scale 1, as the weapon packs)',
                    assets=assets, pickups=pickups, materials=materials, renders=['renders/product-ammo.png'],
                    unreal_import=dict(destination='/Game/MonkeIsland/Weapons/Ammo/', static_mesh=True, auto_generate_collision=False,
                                       collision='UCX hulls are in the FBX', textures='materials[<M_Ammo_*>].textures (BaseColor sRGB, Normal '
                                       'DirectX, ORM linear) for the MI_* instances of import_pirate_weapons.py; BaseColor and Normal are '
                                       'also wired in the FBX material (.fbm)', runtime_tested=False),
                    notes=['Not redone here (per weapon pack): pistol / musket cartridges, cartridge box, powder horn, pouch, shotgun shells.',
                           'No Unreal import has been done.'])
    (out / 'ammo-manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=str(out / 'ammo.blend'), compress=True)
    print('[ammo] finished ' + str(out), flush=True)


if __name__ == '__main__':
    main()
