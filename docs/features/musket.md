# Mousquet de Bean et inventaire (T-069)

**Lot :** L5, sur demande de l'utilisateur du 2026-10-03. Écart au cahier §5 : les armes à feu y sont prévues comme une extension, l'ADR-001 doit l'arbitrer.
**Maquettes :** produites sur le 2e PC, dans `OneDrive\maquette-blender\maquettes\` : `bean-musket` v0.1.0, `bean-musket-anim`, `bean-ball-pouch` v0.2.0. Copies dans `art/export/Weapons/Musket/`.

## Règles (DA_MusketRules)
| Valeur | Défaut |
|---|---|
| Balles par joueur (`MaxBalls`) | 23 : 1 dans le canon au départ, 22 dans la sacoche |
| Rechargement complet (`ReloadSeconds`) | 6,0 s |
| Délai de la platine (`FireDelaySeconds`) | 0,12 s |
| Recul (`RecoverSeconds`) | 0,6 s |
| Dégâts / portée | 70 / 80 m |
| Dispersion à la hanche / en visée | 4° / 0,6° |
| Tir ami | désactivé |
| FOV en visée | 65° |

Chargement par la bouche : un tir vide le canon, puis il faut recharger entièrement, ce qui prend une balle dans la sacoche. Cliquer pour tirer avec le canon vide lance le rechargement s'il reste des balles.

## Commandes
- **1 à 4** (ou &, é, ", ' en AZERTY, ou le pavé numérique) : sélectionne l'emplacement. Appuyer de nouveau sur la même touche range l'objet, et l'on repasse aux mains nues (coup de poing).
- Mousquet en main : **clic gauche** tire, **clic droit maintenu** épaule, **R** recharge.
  - Le contexte `IMC_Musket` a la priorité 2 et masque le poing, la garde et le changement de vue, qui est aussi sur R.
- **V** (ou R à mains nues) : change de vue. R est pris par le rechargement quand le mousquet est en main, V marche toujours.
- Le contexte du mousquet est retiré dans plusieurs cas : objet porté (le clic gauche lance alors l'objet), nage, chute ragdoll, mort.

## Architecture
- Dossier `Source/monke_island/Weapons/` :
  - `UIslanderInventoryComponent` : emplacements, objets de départ, emplacement actif répliqué ;
  - `AInventoryItemActor` : base d'un objet équipé, attaché au porteur, avec un état équipé répliqué ;
  - `AMusketWeapon` : l'arme ;
  - `UMusketRules` : les règles ;
  - `MusketMath` : la logique pure testée.
- **Serveur autoritaire :**
  - le client envoie seulement « tirer » ;
  - le serveur vérifie les munitions et l'état, puis trace le tir depuis la caméra du porteur dans la direction de visée répliquée ;
  - il applique `ApplyPointDamage` et l'impulsion ;
  - `PouchBalls` n'est répliqué qu'au porteur.
- **1re personne :**
  - les clés (`HoldPose`, `AimPose`, `ReloadKeys`, écrites dans `MusketRules.cpp`) placent seulement le mousquet par rapport à la caméra, et disent quel socket de `SK_Bean_Musket` chaque moufle tient (`socket_grip_r`, `socket_grip_l`, `socket_pan`, `socket_barrel`, `socket_muzzle`, `socket_ramrod_tip`) ou un point libre (la giberne) ;
  - chaque bras est résolu chaque image depuis son épaule (`RightShoulder` / `LeftShoulder`) vers sa prise (`MusketMath::SolveArm`), avec un étirement de bras en caoutchouc jusqu'à `MaxArmStretch`, si bien que les moufles restent sur l'arme, recul compris ;
  - le rechargement suit les phases de la maquette (demi-armé, amorçage, crosse en bas et canon levé, poudre, balle, baguette, armé). La baguette vient de `A_Musket_Reload`, joué sur le mousquet, et la main droite suit son extrémité ;
  - les rotations passent par des quaternions, ce qui supprime les tours complets de l'ancienne interpolation d'angles ;
  - les anciennes poses `fp_poses.json` (caméra virtuelle qui regardait les mains) sont abandonnées.
- **3e personne :**
  - le mousquet est attaché à `hand_r` du pantin Bean ;
  - dans la maquette, le mousquet est un objet à part : pendant le rechargement, la main droite le lâche et la main gauche le tient par le canon. Soudé à `hand_r`, il s'envolait avec la main droite ;
  - `scripts/props/musket_tp_tracks.py` (Blender en arrière-plan, le .blend n'est que lu) cuit donc la position du mousquet par rapport à `hand_r`, image par image, pour `Hold`, `Reload` et `Fire`, dans `MusketThirdPersonTracks.inl` (`HoldMusketTrack`, `ReloadMusketTrack`, `FireMusketTrack`, 24 i/s) ;
  - `UpdateThirdPersonGrip` lit la piste du clip joué par le corps caché, au même instant ;
- **Tir :**
  - la platine claque (`LockSound`), puis après `FireDelaySeconds` vient la détonation (`FireSound`) ;
  - au même moment : flamme à la bouche (sphère étirée, `M_MusketFlash` additif), lumière brève, et fumée de poudre noire (7 bouffées, `M_MusketSmoke` translucide) qui ralentit, monte, grossit et s'efface ;
  - en 1re personne, la vue saute (`ViewKickPitch` / `ViewKickYaw`) et roule un instant (`ViewShakeRoll`), en plus du recul de l'arme et des bras.
- **Sons :** synthétisés par `scripts/props/musket_sounds.py` (aucun enregistrement, donc aucune licence) dans `art/audio/weapons/`.

## Assets Unreal (`/Game/MonkeIsland/Weapons/`)
`SK_Bean_Musket` (avec le socket `muzzle`), `DA_MusketRules`, `DA_Item_Musket`, `BP_Musket`, `Input/IA_Musket*`, `IA_Slot1-4`, `IMC_Musket`, `IMC_Inventory`. Le composant `Inventory` est sur `BP_IslanderBean`, avec le mousquet dans l'emplacement 1, équipé dès l'apparition.

## Reste à faire
- Les 5 animations, les 2 sons et les 2 matériaux s'importent avec `scripts/props/import_musket_anims.py`, éditeur fermé (commandlet), qui les branche aussi dans `DA_MusketRules`.
- Vérifier le roulis de la prise en 3e personne. Remplacer un jour la fumée en sphères par un Niagara (tech-artist).
- Recette PIE hôte + client par l'utilisateur : tir, rechargement, 23 tirs, objet porté, nage.

## Recette
- `monke_island.Weapons.Musket` : 11 tests (23 balles, cycle tir/rechargement, 23 tirs exactement, poses triées, échantillonnage, moufle sur sa cible, rotation par le plus court chemin, pistes 3e personne, cône de dispersion).
- PIE solo, testé éditeur le 2026-10-03 : le HUD affiche « 1 Mousquet » actif et « Chargé · 23 balles ».
- Le tir, le rechargement et la visée n'ont pas encore été testés en jeu : aucun outil MCP ne permet de simuler des touches.
