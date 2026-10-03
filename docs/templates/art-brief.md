# Brief asset : <SM_Nom>

- **Demandé par** : <agent / feature> · **Pour** : `ue-blender-artist` / `ue-environment-artist`
- **Priorité** : P0 | P1 | P2 · **Statut** : à faire | en cours | à valider | intégré

## Usage en jeu
Où il apparaît, combien d'exemplaires, distance de vue typique, interaction (collision, ramassable, destructible).

## Références
Photos réelles (Tuamotu de préférence), lien satellite / position sur Raraka, assets existants à imiter.

## Spécifications
| Élément | Valeur |
|---------|--------|
| Dimensions réelles (m) | L × l × h |
| Détail | Nanite haut détail / mesh léger + LOD |
| Matériaux | slots, textures attendues (D / N / ORM), variantes de couleur |
| Collision | complexe / UCX convexes / aucune |
| Pivot | centre de la base, posé à Z = 0 |

## Livraison
- Source : `art/blender/<feature>/<nom>.blend` · export : `art/export/<Feature>/SM_<Nom>.fbx`
- UE : `/Game/MonkeIsland/<Feature>/Meshes/SM_<Nom>`
- Licence de toute ressource externe consignée dans `art/SOURCES.md`

## Validation
Capture Blender + `CaptureAssetImage` UE + capture en situation dans le niveau.
