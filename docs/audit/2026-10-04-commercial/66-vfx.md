# 66 — Effets visuels (VFX)
Domaine : G · Auditeur : artiste VFX

## Verdict
Le jeu n'a aucun système Niagara : ses effets (mousquet, torche, feuilles, papillons, traces de barque) sont des meshes, instances ou decals animés en C++. La mer n'a ni sillage, ni gerbe, ni écume de rivage, et la tempête finale n'a ni pluie ni éclairs. Risque commercial : un climax muet et une mer lisse, sans budget ni outil VFX.

## Constats
- **[Bloquant commercial] Tempête finale et montée des eaux sans effets**
  - Constat : la tempête ne touche que soleil, brouillard, exposition, palmes et feuilles volantes ; ni pluie, éclairs, embruns, écume de crue, ni rendu sous-marin ; `FoamAmount` fixe.
  - Preuve : storm-flood.md:38,104 ; build_ocean_material.py:113,233-235 ; rapports 15, 45.
  - Recommandation : pluie collée à la caméra, éclairs avec l'option « réduire les flashs » (cahier §8, §18), embruns, écume et débris à la lisière de crue, filtre sous-marin.
  - Effort : L

- **[Majeur] La mer ne réagit à rien**
  - Constat : 13 champs de gerbes, écume et sons de rame sans composant ni matériau ; ni étrave ni sillage ; naufrage = texte de débogage ; pas d'écume de rivage. Piège : la houle est une formule maison, une particule à altitude fixe flotte ou plonge jusqu'à 37 cm.
  - Preuve : BoatRules.h:193-244 ; BoatActor.cpp:953-960 ; T-014 `a-faire` ; sea-waves.md:10,14-16.
  - Recommandation : carte d'interaction 512² centrée caméra, écrite par Niagara, lue par `M_Water_Ocean` et `M_Water_Lagoon` (sillage, anneaux, rivage) ; gerbes de rame en sprites GPU.
  - Effort : L

- **[Majeur] Zéro Niagara, zéro budget, zéro outil**
  - Constat : ni module `Niagara` en C++, aucune référence Niagara dans les 1 492 assets de `/Game/MonkeIsland` (seul le gabarit en a). `NiagaraToolsets` (édition par MCP) est désactivé et la recette `ue-editor` en brouille le nom. Le menu Qualité pilote déjà `fx.Niagara.QualityLevel`.
  - Preuve : Build.cs:11-24 ; monke_island.uproject ; ue-editor/SKILL.md:47-48 ; BaseScalability.ini:798-914 ; SettingsSubsystem.cpp:231.
  - Recommandation : activer `NiagaraToolsets`, ajouter `Niagara` au Build.cs (éditeur fermé), `UNiagaraEffectType`, `docs/features/vfx.md`, cues de 24 ; Niagara GPU dès 30 particules, rien sur serveur dédié.
  - Effort : M

- **[Majeur] La découpe ne se voit pas**
  - Constat : une touche = un son et un chiffre ; la tranche est un disque de « bois nu ». Tiki en bois, défaite « sans gore » : sève ambrée, copeaux, poussière, pas de sang.
  - Preuve : CutlassWeapon.cpp:451-466 ; TikiRules.h:213-220 ; cahier §24 l.534.
  - Recommandation : traînée de lame, copeaux à l'impact et à la coupe, sève sur la tranche, étincelles à la parade.
  - Effort : M

- **[Majeur] Mousquet et torche : effets bricolés**
  - Constat : fumée = 7 sphères translucides, chacune avec son matériau dynamique ; celle de la platine naît près de l'œil en 1re personne (surcharge, supposé). Torche : flamme en mesh, sans braises. Aucun impact de balle.
  - Preuve : MusketWeapon.cpp:547-586 ; MusketRules.h:289-333 ; TorchItem.cpp:402-422.
  - Recommandation : un Niagara par arme, impacts par surface ; version mesh au palier Faible.
  - Effort : M

- **[Majeur] Sermon, drapeau, palan, vente, frégate : aucun effet**
  - Constat : sermon sur `dev/T-054` ; palan, vente, frégate `a-faire` ; périmètre du sermon invisible.
  - Preuve : board.md:61,119,121,123 ; rapports 03, 24.
  - Recommandation : anneau et jauge 0-20 s, rayons, drapeau en tissu (`MPC_Wind`), pulsation dorée du +2:00 ; cordage et gouttes du filet ; pièces et tampon ; canonnade réutilisant gerbes et fumée.
  - Effort : M

- **[Mineur] Papillons et lucioles : garder en instances**
  - Constat : 14 + 24 par village, animés en CPU près de la caméra seulement ; coût négligeable (supposé). Éclairage indirect dynamique et lancer de rayons restent actifs pour eux ; les feuilles coupent l'indirect.
  - Preuve : VillageAmbienceActor.cpp:178-264 ; WindLeavesActor.cpp:89-90.
  - Recommandation : garder ; couper ces deux contributions.
  - Effort : S

## Manques
Budget proposé (à mesurer, hôte compris) : VFX ≤ 1,0 ms GPU, ≤ 0,3 ms de thread de jeu, ≤ 5 000 particules, 0 octet réseau, pris sur la ligne « eau et translucides » (1,5 ms), déjà occupée par l'eau (non mesurée, rapport 45) : à rouvrir.
- **P0** : infrastructure et option flashs (M, 0 ms) ; mer (L, 0,3) ; tempête (L, 0,4) ; découpe et impacts (M, 0,1).
- **P1** : sermon, drapeau, +2:00 (M, 0,2) ; mousquet, torche (M, 0,1) ; palan, pièces, lueur du butin (S, 0,1).
- **P2** : frégate (M, 0,8 ponctuel) ; requin, pas dans l'eau, noix (S, 0,05).
- Total P0-P1 : 5 à 6 semaines d'un artiste VFX (supposé) ; pire cas, tempête à 4 joueurs ≈ 0,8 ms (estimé).

## Quick wins
1. Activer `NiagaraToolsets` (nom exact) dans le `.uproject`, tester une pluie par MCP.
2. Lier `FoamAmount` et la houle au facteur de tempête (`MPC_Waves`).
3. Couper éclairage indirect dynamique et lancer de rayons sur papillons, lucioles, fumée, flamme.
4. Teinter `CutCapMaterial` : bois clair, sève ambrée.

## Questions qui challengent l'équipe
1. Effets naturels comme l'atoll, ou stylisés comme les beans ? Sans réponse écrite, une trentaine de systèmes seront incohérents.
2. Qui les construit, sans artiste VFX, et sur quelles millisecondes prises à Lumen ou aux ombres ?
3. Le sabre « sans gore » est-il un choix de classement assumé, au risque d'un clip moins viral ?
