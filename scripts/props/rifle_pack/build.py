"""Bean rifle pack (T-250): Baker-type rifle model + PBR bake + TP/FP handling clips + Unreal FBX, in
build/packs/bean-rifle-anim/ (rifle-manifest.json). Same format as the musket pack (T-212).

  blender -b --factory-startup --python scripts/props/rifle_pack/build.py -- [options]
    (options: see scripts/props/longarm_common/pack_build.py; --clips, --views, --no-export, --no-bake, --render,
     --product, --save, --out)
Check afterwards: python scripts/props/longarm_common/check.py build/packs/bean-rifle-anim
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
for p in (HERE, HERE.parent / 'longarm_common'):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import pack_build  # noqa: E402
import rifle_model  # noqa: E402
import rifle_profile  # noqa: E402

if __name__ == '__main__':
    pack_build.main(rifle_profile.RifleProfile(), rifle_model.build_rifle, rifle_model.build_ammo, HERE / 'pack.json',
                    REPO / 'build' / 'packs' / 'bean-rifle-anim')
