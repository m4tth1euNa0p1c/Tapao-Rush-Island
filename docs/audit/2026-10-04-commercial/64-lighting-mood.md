# 64 — Ambiance lumineuse : nuit, tempête, peur
Domaine : G · Auditeur : lighting artist

## Verdict
Bases saines (soleil réel, palette lune froide / lanternes orange, lucioles), mais sans dramaturgie ni validation : aucune capture de nuit ou de tempête du jeu n'existe, et l'heure aléatoire coupe le ciel de l'échéance. Risque commercial : la « peur nocturne » (cahier §1) reste une intention, sans trailer possible.

## Constats
- **[Bloquant commercial] L'arc jour → nuit → tempête est tiré au hasard et jamais vu**
  - Constat : le cahier place crépuscule à 120 s et nuit à 240 s d'expédition (lecture : la nuit, prix de l'avidité). Le code tire l'heure de départ entre 6 h 30 et 18 h : hors facteur de tempête, le ciel ignore l'expédition. Calcul : la tempête éclate en plein jour dans 47 à 81 % des parties. Aucune des 5 images versionnées n'est de nuit.
  - Preuve : DayNightRules.h:48-55 ; DayNightRules.cpp:7-15 ; cahier §6.1 l.102.
  - Recommandation : clés de la DA en temps d'expédition (120 s / 240 s), départ vers 15 h 30, ciel de tempête amorcé 30 s avant l'échéance ; aléa par île.
  - Effort : S

- **[Majeur] Nuit réglée à l'aveugle : lune plate et zénithale**
  - Constat : la lune, sans ombres et opposée au soleil, monte à 69° (≥ 45° la moitié de la nuit, calcul) : ni ombres longues, ni intérieurs sombres. Réglages changés sans capture (EV100 min −3 → −1, lune 0,15) ; la DA donne lune 0,15 et EV100 min −1 (défaut), la fiche 0,2 et −3.
  - Preuve : DayNightRules.h:95,107 ; DayNightCycleActor.cpp:217,259-268 ; DA_DayNightRules.uasset ; journal.md:102-106 ; storm-flood.md:115.
  - Recommandation : ombres de lune quand le soleil est éteint (pas angulaire, voir 45), élévation plafonnée à 45°, six prises de nuit versionnées (cible : maquette 03_hud_nuit_torche.png).
  - Effort : S

- **[Majeur] La lumière ne guide ni ne menace**
  - Constat : face à ~1,5 lux de lune (soleil 10 lux, supposé), la torche (9 cd) ne la dépasse que sous 2,5 m, la lanterne (6 cd) sous 2 m, la lampe de maison (25 cd) sous 4 m (calcul) : pas de ressource lumineuse. Tiki de nuit à 15–35 m sans signature lumineuse (émissif non cuit) ; lampes de village coupées à 90 m, fumée non codée ; pont du galion et barque sans lampe C++ : pas de balise de retour.
  - Preuve : TorchRules.h:74,82 ; VillageRules.h:161 ; TikiRules.h:325-331 ; characters-tiki.md:81.
  - Recommandation : lune ~0,05, torche ~30 cd, lanternes ~15 cd (captures) ; feux de poupe et de mât, lanterne de barque ; yeux ou braises émissifs.
  - Effort : M

- **[Majeur] La tempête est un variateur, pas un ciel**
  - Constat : de jour, soleil ×0,25, que l'exposition automatique compense en grande partie (supposé) ; de nuit, la lune n'est pas modulée : seuls brouillard ×4 et −0,3 EV changent. Nuages `m_SimpleVolumetricCloud_Inst` non pilotés. Ni étoiles ni brouillard volumétrique (supposé). Pluie, éclairs : voir 15, 66.
  - Preuve : DayNightCycleActor.cpp:253,267-271,275,281 ; Lvl_Raraka.umap (ni dôme d'étoiles ni `VolumetricFog`).
  - Recommandation : lier nuages, désaturation, eau et brume à `StormFactor` ; dôme d'étoiles CC0.
  - Effort : M

- **[Majeur] Le menu contredit le cahier : plage de jour, pas de galion**
  - Constat : §16 veut un port au crépuscule avec galion. Réel : capture floue d'un abri en plein jour ; `L_MainMenu` est le niveau gabarit de 49 Ko.
  - Preuve : TapaoUISkin.h:75 ; STapaoScreens.cpp:41-48,119 ; T_Tapao_MenuBackground.png ; cahier §16 l.303.
  - Recommandation : capture du galion à 17 h 40 (`monke.SetHour 17.6`) en fond ; puis scène réelle (`SetClockSpeed(0)`, lanternes).
  - Effort : S (image) / M (scène)

- **[Majeur] Le hub est sans lumière : cabine noire, zone sûre non conçue**
  - Constat : cabine noire (galleon_pie.png, bas-droite ; audit 20, non 19). Lampe de 5 cd ajoutée depuis (fichier de 04:38), non vérifiée en PIE. Aucune lumière de pont. `L_Galleon`, où l'on vend, n'existe pas : avec le cycle aléatoire, le refuge serait sombre ou en plein midi.
  - Preuve : GalleonActor.cpp:116-128 ; galleon.md:44,62 ; cahier §24.2 l.524.
  - Recommandation : prise PIE de la cabine ; lampes de pont ; `L_Galleon` en heure dorée figée.
  - Effort : S

- **[Majeur] Huit ambiances écrites, aucun réglage par île**
  - Constat : islands.md écrit une lumière par fragment ; `UExpeditionIslandDefinition` n'a aucun champ de ciel : levier de variété le moins cher.
  - Preuve : islands.md:93,120,129,147 ; ExpeditionIslandDefinition.h:43-79.
  - Recommandation : `ULightingPreset` (courbe horaire, lune, brume, nuages, eau) référencé par l'île.
  - Effort : M

## Manques
1. Dossier d'éclairage par lieu et phase, captures de référence.
2. Luminosité avec mire (§18) : absente de Settings/.
3. Étoiles, halo lunaire, brume volumétrique.
4. Lueur du butin de valeur.

## Quick wins
1. Six captures de nuit (`monke.SetHour 19.5`, `22`) commitées.
2. Ombres de lune si `SunFactor < 0.02` (Live Coding).
3. Fond de menu = galion à 17 h 40.
4. Lampes de poupe et de mât (copie de la cabine).
5. `ExpeditionStormLeadSeconds` = 30.

## Questions qui challengent l'équipe
1. Montrez une capture de nuit : sans elle, que prouve la « peur nocturne » ?
2. Nuit subie (hasard, 4 oct.) ou choisie (avidité, §6.1) ?
3. Reconnaît-on le jeu en trois secondes sur une plage de jour ?
