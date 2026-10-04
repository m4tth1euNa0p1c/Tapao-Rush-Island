# Torche de Bean : template Unreal

La torche est portée en **main gauche** (off-hand), si bien que Bean peut tenir la torche et le sabre en même temps. Elle sert à :
- éclairer la nuit, les grottes et l'intérieur des cabanes ;
- allumer le feu de camp, les torches murales du marae ou un brasero ;
- repousser les mobs.

| Fichier | Contenu |
|---|---|
| `maquettes/bean-torch/exports/SM_Bean_Torch.fbx` | La torche : static mesh, 2 UCX, sockets `grip`, `flame`, `light`, `heel`. |
| `maquettes/bean-torch/exports/SM_Bean_Torch_Flame.fbx` | Flamme de secours, émissive. À utiliser si le Niagara n'est pas prêt. |
| `maquettes/bean-torch/torch-settings.json` | Lumière (intensité, rayon, couleur, vacillement), sockets, gameplay. À recopier dans `DA_Bean_Torch`. |
| `maquettes/bean-torch-anim/exports/anims/A_Bean_Torch_*.fbx` | Les 7 animations, sur SKEL_Bean. |
| `maquettes/bean-anim-library/ue/BeanAnimManifest.json` | Contient les animations de la torche, leurs notifies, les sockets `hand_l/torch_grip` et `pelvis/torch_hip`, et la section `torch`. |
| `maquettes/bean-anim-library/ue/import_bean_animations.py` | Importe les animations de la torche avec les autres (montages `AM_Bean_Torch_*`, notifies `AN_*`). |

## 1. Import
1. Importe `SM_Bean_Torch.fbx` dans `/Game/MonkeIsland/Items/Torch/` avec *Combine Meshes* coché ; les collisions UCX et les sockets sont lus automatiquement. Importe `SM_Bean_Torch_Flame.fbx` (material émissif, sans ombre).
2. Lance `import_bean_animations.py` (template de Bean). Les 7 animations `A_Bean_Torch_*` et leurs montages sont créés avec les autres.
3. Sur `SK_Bean_Body_Skeleton`, crée deux sockets avec les valeurs du manifeste (`attach_sockets`) :
   - `torch_grip` sur `hand_l` ;
   - `torch_hip` sur `pelvis`.

## 2. Blueprint `BP_Bean_Torch` (Actor, attaché à Bean)
| Composant | Réglage |
|---|---|
| `StaticMesh` | SM_Bean_Torch, *No Collision* quand elle est tenue, physique quand elle est lâchée (masse 0,55 kg) |
| `Niagara` *(FX_TorchFire)* | au socket `flame` : flamme + braises + fumée légère ; *Auto Activate* désactivé |
| `PointLight` | au socket `light` ; valeurs ci-dessous |
| `Audio` | boucle de crépitement (atténuation 6 m) |

### Lumière
| Paramètre | Valeur | Remarque |
|---|---|---|
| Intensité | 9 cd (11 cd torche levée) | unités *Candelas* |
| Rayon d'atténuation | 950 cm (1 250 cm torche levée) | Bean mesure 1,05 m : environ 9 fois sa taille |
| Température | 1 850 K | *Use Temperature* coché, couleur blanche |
| Source radius / soft | 4 cm / 12 cm | ombres douces |
| Ombres | dynamiques | *Cast Shadows* coché ; mettre `bAffectTranslucentLighting` à false si besoin de performance |
| Vacillement | `Intensity × (0,82 + 0,10·sin(2π·1,3·t) + 0,08·bruit 9 Hz)` | dans Tick ou une Timeline ; décaler aussi la lumière de ±1 cm (bruit 6 Hz) |

Quand la torche est levée (Q maintenu), interpole l'intensité et le rayon en 0,25 s.

### État et carburant (`DA_Bean_Torch`)
- `bLit` : allumée ou éteinte.
- `Fuel_s` : durée de combustion restante (300 s par défaut).
  - Elle baisse seulement quand la torche est allumée.
  - En dessous de 30 s : la flamme et la lumière diminuent (Niagara *SpawnRate* ×0,5), et un avertissement s'affiche dans l'interface.
  - À 0 : la torche s'éteint (`AN_Torch_Extinguish` joué en code) et ne peut plus être rallumée.
- Recharge au feu de camp : chiffon (`bean-cloth`) + huile (`bean-sword-oil`) donnent +300 s.
- Eau : si la torche passe sous la surface de l'eau (trace depuis le socket `flame`), elle s'éteint.

## 3. Entrées et animations
| Entrée | Condition | Animation | Effet |
|---|---|---|---|
| 3 | torche rangée | `AM_Bean_Torch_Equip`, puis `AM_Bean_Torch_Ignite` si `!bLit` | attache au socket `torch_grip` sur `AN_Torch_AttachToHand` ; allumage sur `AN_Torch_FlameOn` |
| 3 | torche en main | `AM_Bean_Torch_Unequip` | extinction sur `AN_Torch_Extinguish`, attache au socket `torch_hip` sur `AN_Torch_AttachToHip` |
| Q maintenu | torche en main | état `Raise` : `A_Bean_Torch_Raise_Idle` | lumière plus forte et rayon plus grand |
| E | cible inflammable à moins de 1,2 m, torche allumée | `AM_Bean_Torch_Light` | `BPI_Ignitable::Ignite()` sur la cible à `AN_Ignite_Target` |
| V | torche en main | `AM_Bean_Torch_Swing` | −6 % d'endurance ; dégâts 10 + brûlure 4 PV/s pendant 3 s si la torche est allumée |
| Prendre le mousquet / monter dans la barque | torche en main | `AM_Bean_Torch_Unequip` automatique | la torche reste allumée tant que `Fuel_s` > 0, si on le souhaite |

**AnimBP** : ajoute une couche *Layered blend per bone* depuis `upperarm_l` (poids 1 quand `bHasTorch`). La pose de la couche est `A_Bean_Torch_Idle` ou `A_Bean_Torch_Raise_Idle`, avec 0,25 s de transition. Les montages `UpperBody` passent par-dessus, `Torch_Light` passe par le slot `FullBody`.

## 4. Interactions avec le monde
- **Interface `BPI_Ignitable`** : `CanIgnite()` et `Ignite(Instigator)`. Elle s'implémente sur :
  - le feu de camp `SM_Camp_Campfire`, qui allume sa flamme, sa lumière (150 W équivalent) et le soin du feu ;
  - les torches murales du marae, `SM_Marae_Torch` ;
  - les braseros.
- **Mobs** (dans la Behavior Tree, si le joueur est à moins de 3 m et que la torche est allumée) :
  - Rôdeur des Ronces : dégâts de feu ×2 et fuite pendant 3 s ;
  - Villageois : `AM_Startle`, puis il garde ses distances ;
  - Gardien Crabe : dégâts de feu ×0,5 ;
  - Guerrier Coco et Veilleur des Cendres : normaux.
- **Perception** : un joueur qui porte une torche allumée est repéré de nuit à 18 m, contre 8 m sans torche (*AI Sight* : bonus de lumière).

## 5. Régénérer
```
uv run factory/tools/maquette.py build bean-torch --headless --force
uv run factory/tools/maquette.py build bean-torch-anim --headless --force
python factory/tools/make_ue_template.py
```
