"""Bean blunderbuss pack (T-250): naval blunderbuss model + PBR bake + TP/FP handling clips + Unreal FBX, in
build/packs/bean-blunderbuss-anim/ (blunderbuss-manifest.json). Same format as the musket pack (T-212).

  blender -b --factory-startup --python scripts/props/blunderbuss_pack/build.py -- [options]
    (options: see scripts/props/longarm_common/pack_build.py; --clips, --views, --no-export, --no-bake, --render,
     --product, --save, --out)
Check afterwards: python scripts/props/longarm_common/check.py build/packs/bean-blunderbuss-anim
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
for p in (HERE, HERE.parent / 'longarm_common'):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import pack_build  # noqa: E402
import blunderbuss_model  # noqa: E402
import blunderbuss_profile  # noqa: E402

if __name__ == '__main__':
    pack_build.main(blunderbuss_profile.BlunderbussProfile(), blunderbuss_model.build_blunderbuss, blunderbuss_model.build_ammo,
                    HERE / 'pack.json', REPO / 'build' / 'packs' / 'bean-blunderbuss-anim')
