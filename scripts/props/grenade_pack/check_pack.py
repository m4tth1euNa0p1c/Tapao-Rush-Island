"""Checks build/packs/bean-grenade and build/packs/ammo with the helpers of scripts/character/import_pirate_weapons.py
(registry entries "Grenade" and "Ammo", T-249): files present, sha256 of the manifests, FBX units (UnitScaleFactor
100 = metres), destination plan; plus the pack contract of T-253: 14 gestures x 2 views at 30 fps, FP and TP lengths
and events identical, expected events and sockets, weapon bones, baked textures, UCX hulls of the pickups.

  python scripts/props/grenade_pack/check_pack.py        exit 0 = CHECK OK
(the importer's own dry run: MONKE_PIRATE_WEAPONS=Grenade,Ammo python scripts/character/import_pirate_weapons.py --check)
"""
import importlib.util
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
GESTURES = ['Equip', 'Idle', 'Walk', 'Sprint', 'AimIn', 'AimIdle', 'AimOut', 'Fire', 'AimFire', 'Reload', 'DryFire', 'Inspect', 'Holster', 'Discard']
EXPECTED_EVENTS = {'Fire': ['Light', 'Fire'], 'AimFire': ['Light', 'Fire'], 'Reload': ['BagReach', 'HammerCocked', 'Ready'],
                   'DryFire': ['DryClick'], 'Equip': ['DrawWeapon', 'Ready'], 'Holster': ['StowWeapon'], 'Discard': ['Empty', 'Release']}
BONES = ['gun_root', 'body', 'fuse', 'match', 'bag', 'socket_grip_r', 'socket_body', 'socket_muzzle', 'socket_fuse', 'socket_match', 'socket_bag']
AMMO_NAMES = ['SM_Ammo_GrenadeBag', 'SM_Ammo_CrateMixed', 'SM_Ammo_PowderKeg']


def importer():
    spec = importlib.util.spec_from_file_location('import_pirate_weapons', os.path.join(REPO, 'scripts', 'character', 'import_pirate_weapons.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def check_files(mod, name, report):
    """The importer's dry run for one pack: files, sha256, units."""
    root = mod.pack_dir(name)
    if not root:
        report['missing'].append(f'pack {name}')
        return None, None
    m = mod.manifest(name)
    for entry in m['assets']:
        path = os.path.join(root, entry['path'])
        if not os.path.isfile(path):
            report['missing'].append(path)
            continue
        if entry.get('sha256') and mod.sha256(path) != entry['sha256']:
            report['hash_mismatch'].append(path)
        if path.lower().endswith('.fbx'):
            unit = mod.fbx_unit(path)
            report['units'][str(unit)] = report['units'].get(str(unit), 0) + 1
    for mat, textures in mod.materials(name).items():
        for param, path in textures.items():
            if not os.path.isfile(path):
                report['missing'].append(path)
    return root, m


def check_grenade(mod, report):
    root, m = check_files(mod, 'Grenade', report)
    if not m:
        return
    errors = report['contract_errors']
    base = mod.bases('Grenade')
    anims = mod.animation_plan('Grenade')
    report['plan']['Grenade'] = {
        'registry': mod.WEAPONS['Grenade'], 'pack': root.replace(os.sep, '/'),
        'base': f"{mod.dest('Grenade')}/{base['base']} (own skeleton)", 'fp_arms': f"{mod.dest('Grenade')}/{base['fp']} on {mod.BEAN_SKELETON}",
        'fps': mod.fps_of('Grenade'), 'bean_clips': sum(1 for a in anims if a[3] == 'bean'), 'weapon_clips': sum(1 for a in anims if a[3] == 'weapon'),
        'gestures': mod.gestures('Grenade'),
        'static': [f"{mod.dest('Grenade', 'Static')}/{s}{' (pickup)' if p else ''}" for s, p in mod.statics('Grenade')],
        'materials': {mat: sorted(t) for mat, t in mod.materials('Grenade').items()},
        'format': f"{m.get('format')} v{m.get('version')}", 'runtime_tested_by_pack': m.get('unreal_import', {}).get('runtime_tested')}
    if base['base'] != 'SK_Bean_PirateGrenade' or base['fp'] != 'SK_Bean_Grenade_FP':
        errors.append(f'bases {base} != SK_Bean_PirateGrenade / SK_Bean_Grenade_FP')
    if [s for s, p in mod.statics('Grenade') if p] != ['SM_Bean_PirateGrenade']:
        errors.append(f"pickup {mod.statics('Grenade')} != SM_Bean_PirateGrenade")
    for socket in mod.WEAPONS['Grenade'].get('muzzle_sockets', []):
        if socket not in m['weapon_contract']['bones']:
            errors.append(f'registry muzzle socket {socket} is not a weapon bone')
    by_view = {(s['view'], s['clip']): s for s in m['scenes']}
    for clip in GESTURES:
        for view in ('TP', 'FP'):
            if (view, clip) not in by_view:
                errors.append(f'missing clip {view} {clip}')
        tp, fp = by_view.get(('TP', clip)), by_view.get(('FP', clip))
        if tp and fp:
            if (tp['end'] - tp['start']) != (fp['end'] - fp['start']):
                errors.append(f'{clip}: TP {tp["end"]} frames != FP {fp["end"]}')
            if sorted((e['frame'], e['name']) for e in tp['events']) != sorted((e['frame'], e['name']) for e in fp['events']):
                errors.append(f'{clip}: TP events != FP events')
            if tp['fps'] != 30 or fp['fps'] != 30:
                errors.append(f'{clip}: not 30 fps')
            names = [e['name'] for e in tp['events']]
            if names != EXPECTED_EVENTS.get(clip, []):
                errors.append(f'{clip}: events {names} != {EXPECTED_EVENTS.get(clip, [])}')
            for e in tp['events']:
                if e['frame'] <= 0:
                    errors.append(f'{clip}: event {e["name"]} at frame {e["frame"]} (frame 0 events are not scheduled by AFirearmWeapon::StartAction)')
                if e.get('socket') and e['socket'] not in BONES:
                    errors.append(f'{clip}: event socket {e["socket"]} not a weapon bone')
    if sorted(m['delivered_clips']) != sorted(GESTURES):
        errors.append(f'delivered_clips {m["delivered_clips"]} != 14 gestures')
    if m['weapon_contract']['bones'] != BONES:
        errors.append(f"weapon bones {m['weapon_contract']['bones']} != {BONES}")
    for label in ('BaseColor', 'Normal', 'ORM'):
        tex = m['weapon_contract'].get('textures', {}).get(label)
        if not tex or not os.path.isfile(os.path.join(root, tex)):
            errors.append(f'texture {label} missing: {tex}')
    pickup = os.path.join(root, 'exports', 'Static', 'SM_Bean_PirateGrenade.fbx')
    if os.path.isfile(pickup) and not open(pickup, 'rb').read().count(b'UCX_SM_Bean_PirateGrenade_00'):
        errors.append('SM_Bean_PirateGrenade.fbx: UCX hull not found')
    report['events'] = {clip: by_view[('TP', clip)]['events'] for clip in GESTURES if ('TP', clip) in by_view}
    report['fp_framing'] = m['fp_framing']


def check_ammo(mod, report):
    root, m = check_files(mod, 'Ammo', report)
    if not m:
        return
    errors = report['contract_errors']
    rows = []
    names = [s for s, _ in mod.statics('Ammo')]
    for name in AMMO_NAMES:
        if name not in names:
            errors.append(f'ammo {name} missing from the manifest statics')
    mats = mod.materials('Ammo')
    for entry in m['assets']:
        if not entry['path'].lower().endswith('.fbx'):
            continue
        path = os.path.join(root, entry['path'])
        ucx = open(path, 'rb').read().count(('UCX_%s_00' % entry['name']).encode()) if os.path.isfile(path) else 0
        if not ucx:
            errors.append(f'{entry["name"]}: UCX hull not found in the FBX')
        if entry.get('material') not in mats or sorted(mats.get(entry.get('material'), {})) != ['BaseColor', 'Normal', 'ORM']:
            errors.append(f'{entry["name"]}: material {entry.get("material")} without the three baked textures in manifest.materials')
        rows.append(dict(name=entry['name'], unit=mod.fbx_unit(path) if os.path.isfile(path) else None, ucx_in_fbx=bool(ucx),
                         size_m=entry.get('size_m'), triangles=entry.get('triangles'), material=entry.get('material')))
    report['plan']['Ammo'] = {'meshes': [f"{mod.dest('Ammo', 'Meshes')}/{s}" for s in names], 'materials': {k: sorted(v) for k, v in mats.items()},
                              'format': f"{m.get('format')} v{m.get('version')}", 'rows': rows}


def main():
    mod = importer()
    report = {'missing': [], 'hash_mismatch': [], 'units': {}, 'contract_errors': [], 'plan': {}}
    check_grenade(mod, report)
    check_ammo(mod, report)
    print(json.dumps(report, indent=1, ensure_ascii=False))
    ok = not report['missing'] and not report['hash_mismatch'] and not report['contract_errors'] and list(report['units']) == ['100.0']
    print('CHECK OK' if ok else 'CHECK FAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
