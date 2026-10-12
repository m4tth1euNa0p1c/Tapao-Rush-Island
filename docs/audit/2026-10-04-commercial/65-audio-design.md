# 65 — Audio design : voix, ambiance, mixage, musique
Domaine : G · Auditeur : sound designer

## Verdict
Le son est un échafaudage : 100 clips (34 synthétisés, 63 de packs CC0, 3 mixtes), huit systèmes câblés, mais ni musique, ni voix de personnage, ni son de barque, de tempête, de requin ou de village. Mixage non mesuré, aucun espace sonore, seuls les pas validés à l'oreille (T-071). Risque commercial : un jeu de menace et de satire dont le son n'avertit de rien et n'a aucune signature.

## Constats
- **[Bloquant commercial] Aucune voix, donc aucune identité**
  - Constat : ni Percepteur, ni sermon de 20 s (§6.2 exige voix et sous-titres), ni effort ou cri du bean. Les trois guerriers tiki partagent 34 clips (hauteur 1 / 0,85 / 1,12) ; au meurtre d'un villageois, jusqu'à 11 voisins crient dans la même image, avec 5 clips humains.
  - Preuve : grep Sound = 0 dans Player/, Character/ ; BP_Tiki*.uasset ; TikiDirector.cpp:526-533
  - Recommandation : Percepteur en charabia à syllabes (MetaSound, FR/EN sans doublage) ; sermon en 4 segments × 4 variantes, coupé net à l'interruption ; une banque par espèce.
  - Effort : L
- **[Majeur] Mixage non mesuré, aucune règle de voix**
  - Constat : clips normalisés en crête (−1 dBFS), pas en sonie. RMS 400 ms max, non pondéré, médiane par famille : fanfare −9,7 dBFS, voix tiki −13,4, mousquet −17,8, pas −21,3, rechargement −30,7 (jusqu'à −39), houle −18,5 avant ×0,25 et ×0,12. Cinq SoundClass plates, sans ducking ; aucun `USoundConcurrency` ; 32 voix par défaut ; tirage sans exclusion de répétition (1 pas sur 4).
  - Preuve : mesure des WAV ; SC_*.uasset ; AudioSettings.h:93 (moteur) ; FootstepSubsystem.cpp:117
  - Recommandation : hiérarchie cible (alarmes, armes > voix > pas > lits) par script, classes filles (Voix, Armes, Pas, Alertes), ducking sous voix et alertes, tirage sans répétition.
  - Effort : M
- **[Majeur] La menace n'est ni annoncée ni localisable**
  - Constat : saut et rugissement du tiki partent dans la même image (ni froissement ni silence d'oiseaux, contre §8). Les cris n'ont que l'atténuation par défaut du moteur (4-40 m, linéaire, sans filtre) ; plugins de spatialisation, réverbération et occlusion vides. Le coup de mousquet n'a aucune atténuation : plein volume, centré, partout (déduit du code moteur, non écouté), avec un écho de lisière cuit dans l'échantillon.
  - Preuve : TikiWarrior.cpp:82-83 ; Attenuation.cpp:18-24 et AudioDevice.cpp:6720,6742 (moteur) ; DefaultEngine.ini:57-60 ; MusketWeapon.cpp:477,505 ; musket_sounds.py:45-49
  - Recommandation : froissement de 1,2 s, creux d'ambiance et roulement avant le cri ; 3 presets ATT (passe-bas, occlusion par trace, détonation à 300 m) ; 3 zones de réverbération (air libre, minka, cale) ; essai Steam Audio (gratuit).
  - Effort : M
- **[Majeur] Barque, requin, tempête, village : muets ; rivage synthétique**
  - Constat : `OarSplashSound` et `OarDripSound` sont déclarés, jamais lus : ramer, geste central, est silencieux. La morsure du requin ne fait que bouger le corps. Village, galion, vent : rien, alors que `WindStrength` est calculé par image. Le seul lit existant, le rivage, est du bruit blanc filtré (boucles de 48 et 32 s), avec 81 tracés par sonde, 2 fois par seconde.
  - Preuve : BoatRules.h:204-212 ; SharkActor.cpp:318-321 ; storm-flood.md:41,104 ; shore_sounds.py:65-90 ; ShoreAmbienceActor.cpp:109-122
  - Recommandation : lits MetaSound pilotés par l'état répliqué (vent, houle et coque, village, nuit, galion, feux), enregistrés sur 2 min et plus ; puis rame, requin, cloche.
  - Effort : M
- **[Bloquant commercial] Aucun plan de production audio**
  - Constat : `docs/audio/` n'existe pas ; ni chantier (après T-077) ni backlog ne planifient musique, voix ou mixage ; `Music/` vide ; §16 exige une musique de menu, §18 une sortie mono et une plage dynamique ; une seule recette audio (UI07).
  - Preuve : ue-audio-designer.md:16 ; setup_settings_assets.py:31 ; board.md ; backlog.md ; cahier §23
  - Recommandation : plan L4-L6. A fondations (1 sem., agents : fiches, bus, ATT, concurrence) ; B douze moments (sermon, +2:00, alertes, submersion, tempête, départ, palan, vente, naufrage, mort, victoire, défaite) et six lits (3 sem.) ; C en parallèle, humains (6-8 sem.) : compositeur 10-12 min, comédien (Percepteur, sermon, bean), sound designer 5-8 j ; D mixage casque, enceintes, Steam Deck, 2 machines (2 sem.). Budget 10-20 k€ (supposé).
  - Effort : L

## Manques
1. Musique adaptative : cuivres pompeux de la Couronne contre drones de mer et résonateurs en bois, pilotée par phase, poursuivants et horloge.
2. Tempête et compteur : vent, pluie, tonnerre borné (§8), cloche de quart à 60/30/10 s (`DA_HudTimerRules` sans son : voir 15), grondement de la montée des eaux.
3. Galion (cloche, corne §24.2, palan), nuit, feux.
4. Sortie audio du §18, sous-titres (voir 25), VoIP en L7.

## Quick wins
1. Tirage sans répétition immédiate (pas, voix).
2. Créer `ATT_Musket_Shot` (300 m, passe-bas) pour `PlaySoundAtLocation`.
3. `Hull` et `Galleon` en bois : le pont du galion retombe sur Stone (supposé ; FootstepRules.h:98).
4. Concurrence : pas ≤ 8, cris ≤ 3.
5. Les 4 notes de la fanfare (G-C-E-G) comme signature du +2:00.

## Questions qui challengent l'équipe
1. Qui valide le son hors du propriétaire, avec quel budget avant la page Steam ?
2. Quel son reconnaît-on en trois secondes dans un clip : fanfare, cloche, sermon ?
3. Des villageois tués avec des cris humains réalistes : cohérent avec la satire sans gore et le classement (voir 80) ?
