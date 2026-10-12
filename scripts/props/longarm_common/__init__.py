"""Common code of the long arm packs built in this repository (T-250): blunderbuss (scripts/props/blunderbuss_pack) and
Baker rifle (scripts/props/rifle_pack). Both reuse the musket pack (scripts/props/musket_pack) by import: its bean source,
its first / third person solvers, its baking and its FP camera (musket_ref.py), parametrised by a weapon profile
(weapon_profile.py, choreo.py); the models, the PBR bakes and the exports are here (materials.py, bake.py, export.py,
pack_build.py, check.py).
"""
