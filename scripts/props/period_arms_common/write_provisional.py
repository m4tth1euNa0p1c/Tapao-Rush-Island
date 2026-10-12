"""Writes the PROVISIONAL manifests of the T-334 packs (bolt-action rifle, sub-machine gun, bayonet musket) before
their Blender build, so the gameplay code (T-335) can rely on the names, clips, frames, events, bones and sockets:
art/export/Weapons/<Weapon>/pack.json with "status": "provisional", "sha256": null and "planned": true on every asset.
A manifest written by a build ("status": "built") is never overwritten (use --force to reset it).

  python scripts/props/period_arms_common/write_provisional.py [--only BoltRifle,Mitraillette,BayonetMusket] [--force]
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import contract  # noqa: E402

PACKS = {'BoltRifle': 'boltrifle_pack', 'Mitraillette': 'smg_pack', 'BayonetMusket': 'bayonet_musket_pack'}


def load_spec(folder):
    path = REPO / 'scripts' / 'props' / folder / 'spec.py'
    spec = importlib.util.spec_from_file_location(f'{folder}_spec', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def version_of(folder):
    p = REPO / 'scripts' / 'props' / folder / 'pack.json'
    return json.loads(p.read_text(encoding='utf-8')).get('version', '0.0.0') if p.exists() else '0.0.0'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--force', action='store_true')
    args = ap.parse_args()
    only = {s for s in args.only.split(',') if s}
    for tag, folder in PACKS.items():
        if only and tag not in only:
            continue
        spec = load_spec(folder)
        out = REPO / spec.EXPORT_DIR / 'pack.json'
        if out.exists() and not args.force:
            current = json.loads(out.read_text(encoding='utf-8'))
            if current.get('status') == 'built':
                print(f'{tag}: built manifest kept ({out.relative_to(REPO)})')
                continue
        m = contract.manifest_skeleton(spec, version_of(folder), 'provisional', spec.EXPORT_DIR)
        m['provisional_note'] = ('PROVISIONAL (written before the Blender build): names, clips, frames, events, bones and sockets '
                                 'are the contract; positions are the planned ones; sha256 null and planned true on every asset '
                                 'until the build replaces this file ("status": "built").')
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(m, indent=2, ensure_ascii=False), encoding='utf-8')
        print(f'{tag}: {out.relative_to(REPO)} ({len(spec.CLIPS)} clips, {len(m["assets"])} planned assets)')


if __name__ == '__main__':
    main()
