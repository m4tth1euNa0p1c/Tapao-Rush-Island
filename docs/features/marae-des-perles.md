# Marae des Perles (T-075)

**Lot :** L5 (monde, point d'intérêt). Demande de l'utilisateur du 2026-10-03. Le lieu sacré est fictif, d'inspiration polynésienne ; il n'est pas lié au village réel de Motutapu.

**Maquette :** produite par le 2e PC, dans `OneDrive\maquette-blender\maquettes\` :
- `marae-des-perles` : scène complète et planche de référence ;
- pièces exportées en FBX : `marae-pyramide`, `marae-autel`, `marae-pierres`, `marae-torche`, `marae-offrandes`, `marae-conque`, `marae-artefact`.

## Contenu
| Élément | Asset | Rôle |
|---|---|---|
| Pyramide à 3 gradins + escalier | `SM_Marae_Pyramid` (52 872 tris, collision = maillage) | décor, on gravit l'escalier |
| Autel | `SM_Marae_Altar` (UCX) | porte la perle au sommet |
| Pierres tombées | `SM_Marae_StoneWall` ×4, échelle 0,7 | au pied des murs |
| Torches ×6 | `BP_Marae_Torch` : `SM_Marae_Torch` + lumière ponctuelle chaude (1,5 cd, 5,5 m : plus fort, elle teintait la pierre et le sable en orange en plein jour) | flamme animée (`M_Marae_Flame`, ondule et vacille) et lumière qui vacille (fonction de lumière `M_Marae_TorchFlicker`) |
| Perle du Gouverneur (perle noire en or) | `BP_Marae_PearlArtifact` (`APhysicsPropActor`, 9 kg) | **objet rare : 1 500 $** (`UValuableComponent::BaseValue`), perd de la valeur aux chocs |
| Bols d'offrandes ×2 | `BP_Marae_Offerings` (1,6 kg) | objet de valeur, 120 $ |
| Conques ×2 | `BP_Marae_Conch` (0,25 kg) | objet de valeur, 80 $ |
| Végétation et corail | meshes de l'atoll déjà dans le projet (naupaka, pandanus, miki-miki, 3 cocotiers, fougères, herbes, blocs et débris de corail, noix et palme sèche) | disposition de la maquette |

## Fiction de la perle (2026-10-03, validé par le propriétaire)
L'objet s'appelle **Perle du Gouverneur**. C'est un bijou de la Couronne abandonné là par une expédition précédente, que les insulaires ont laissé sur l'autel. Le reprendre revient à récupérer un bien du Bureau, pas à voler un objet sacré. Le gameplay ne change pas ; les identifiants techniques (`BP_Marae_PearlArtifact`, `SM_Marae_PearlArtifact`) restent les mêmes. Le nom affiché passera par la String Table (`docs/narrative/islands.md`, T-092) quand l'objet aura un texte en jeu.

## Matériaux
- Textures photo CC0 (Poly Haven, ambientCG), les mêmes que la maquette, empaquetées par `scripts/props/marae_textures.py` en `T_Marae_<Nom>_D / _N / _ORM`. La teinte et la saturation de la maquette sont cuites dans `_D`.
- `M_Marae_Surface` (BaseColor, Normal, ORM, Tiling) et ses MI `MI_Marae_*`. Les UV des FBX sont en mètres, d'où Tiling = 1 / tile de la maquette.
- `M_Marae_Gold`, `M_Marae_BlackPearl` (reflets vert-paon / violet selon l'angle), `M_Marae_Cowrie`.
- `M_Marae_Flame` (non éclairé, additif) et `MI_Marae_FlameCore`.
- La conque et le bol reprennent `MI_LambisShell` et `MI_Minka_wooden_bowl_01`, déjà présents dans le projet.

## Emplacement
`scripts/props/marae_layout.py` écrit `art/export/Props/Marae/marae_layout.json`.
- **Site :** sable corallien plat à (-667 786, -1 013 264), sol à 2,04 m, à 145 m à l'ouest du PlayerStart.
- **Choix du site :** masque sable 0,99, pente < 0,6 m sur 23 m, à plus de 150 m des villages et à plus de 30 m d'un totem. Les tables de végétation n'y ont que 4 touffes d'herbe dans un rayon de 8 m.
- **Orientation :** l'escalier fait face à l'est, vers le PlayerStart.
- **Dans le niveau :** acteurs du dossier `Marae` de Lvl_Raraka.

## Reste à faire / non vérifié
- Ramasser la perle (F), la porter, l'arrimer dans la barque, et sa valeur au bilan (`UValuableComponent`).
- Coût des 6 lumières ponctuelles la nuit : ombres coupées, rayon 5,5 m. Les torches restent allumées le jour.
- Zone calme : `QuietZone_Marae` (TriggerSphere 15 m, tag `TikiQuietZone`, posée par 78) : aucun tiki ni totem dans le marae.
- Souffler dans la conque, fouiller les offrandes (usage prévu par la maquette) : pas de mécanique.
