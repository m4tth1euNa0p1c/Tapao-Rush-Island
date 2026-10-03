---
name: character-physics
description: Ragdoll actif façon Fall Guys pour un personnage Unreal — Physics Asset, UPhysicalAnimationComponent (wobble), démarche dandinée dans l'AnimBP, ragdoll à l'impact, relevé, chapeau et basques physiques, réplication serveur-autoritaire, valeurs en DataAsset, tests et réglage.
argument-hint: "<personnage> [aspect : wobble|ragdoll|getup|hat|tuning]"
---

# Physique du personnage (ragdoll actif)

Cible : $ARGUMENTS — conception : `docs/features/character-bean-soldier.md` §5 ; agent : `ue-physics-animator`.

## 1. Préparer
- Prérequis : le Skeleton et le Physics Asset sont importés (`character-rig`) et les noms d'os sont figés.
- Créer `UBeanPhysicsSettings : UPrimaryDataAsset` (`DA_BeanPhysics`) : forces du wobble par zone, poids du blend, seuils d'impact et de chute, vitesse de repos avant relevé, cône et seuil du chapeau, amortissement des basques, taille de capsule.
- Code : `Source/monke_island/Character/Bean/` (skill `ue-new-class` pour les gabarits). Ne pas toucher `AIslanderCharacter`.

## 2. Physics Asset `PA_<Perso>`
- Une capsule par os principal ; la tête est grosse et lourde, mains et pieds sphériques.
- Limites angulaires : colonne ±25°, épaules ±70°, coudes en charnière 0–130°, genoux en charnière.
- Auto-collision coupée entre os voisins. Masse totale ≈ 25 kg pour 1,05 m.

## 3. États (serveur autoritaire)
`EBeanBodyState { Standing, Stumbling, Ragdoll, GettingUp }`, répliqué.

| Transition | Condition (valeurs lues dans `DA_BeanPhysics`) | Effet |
|---|---|---|
| Standing → Stumbling | impulsion moyenne | wobble ×3 pendant 0,4 s |
| → Ragdoll | impulsion forte, chute > hauteur seuil, explosion | `SetAllBodiesBelowSimulatePhysics(pelvis)`, capsule figée et attachée au pelvis |
| Ragdoll → GettingUp | vitesse du pelvis < seuil pendant 0,5 s | `SnapshotPose`, choix face au sol ou au ciel, montage |
| GettingUp → Standing | fin du montage | wobble normal |

## 4. Wobble et démarche
- `UPhysicalAnimationComponent::ApplyPhysicalAnimationProfileBelow(spine_01, "Wobbly")` ; jambes en profil « Firm ».
- AnimBP : balancement du bassin (roulis ±8° à la fréquence des pas), inclinaison avant proportionnelle à l'accélération, petits pas rapides, bras ballants laissés à la physique.

## 5. Accessoires
- Chapeau : `UStaticMeshComponent` simulé + `UPhysicsConstraintComponent` (cône 20°, amorti) vers `hat_socket`. Au-delà du seuil : contrainte rompue, acteur `ABeanHatPickup` répliqué. Au ramassage : retour au socket.
- Basques : corps `coattail_l/r` simulés en permanence avec une raideur faible.

## 6. Prouver et régler
- Specs `monke_island.Bean.Physics.*` : transitions d'état, seuils lus dans le DA, retour à Standing.
- PIE : chute d'un rocher, choc avec un autre joueur, saut dans l'eau. Captures avant/après. Jamais coincé sous le terrain : test de pénétration du pelvis.
- Réseau : PIE 2 joueurs (listen server), le ragdoll est vu identique à ± quelques cm près.
- Coût : `/ue-profile` avec 4 beans en wobble. Budget proposé : < 0,5 ms de physique pour 4.
