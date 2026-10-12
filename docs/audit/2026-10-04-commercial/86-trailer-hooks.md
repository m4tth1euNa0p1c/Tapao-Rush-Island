# 86 — Trailer d'annonce : accroche, plans, capture
Domaine : I · Auditeur : monteur de trailers

## Verdict
Deux images sont prêtes (chaloupe au soleil bas, galion) et un gag codé reste à filmer, mais les quatre plans signature manquent : sermon, tempête, vente au Percepteur, frégate. Ni Sequencer, ni caméra cinématique, ni musique. Risque : un joli lagon et du slapstick générique, sous un nom et un logo impubliables (« Crownfall », « TAPAO »).

## Constats

- **[Majeur] Accroche : la meilleure image est calme**
  - Constat : la chaloupe dorée est l'image forte, mais rien n'y arrive en 5 s ; Steam lance le trailer muet (source secondaire) : seul le compteur d'eau dit l'enjeu.
  - Preuve : T-095/01-02 (vus), steampageanalyzer.com/blog/steam-trailer-best-practices.
  - Recommandation : micro-histoire d'abord (relique, buisson, chapeau, compteur 0:09), ni logo ni titre avant la fin ; test A/B : plan 2 seul.
  - Effort : S

- **[Bloquant commercial] Les plans signature n'existent pas**
  - Constat : sermon (branche `dev/T-054`), pluie, Percepteur, frégate : rester immobile 20 s sous la marée montante est infilmable.
  - Preuve : audit 03, board T-114, T-116, storm-flood.md:104.
  - Recommandation : teaser de 25 s (plans 1-3, 5, titre) dès la capture ; trailer de 75 s après T-054 et T-114 ; plans C étiquetés cinématique (§24.10).
  - Effort : L

- **[Majeur] Aucune chaîne de capture**
  - Constat : 0 Level Sequence, 0 CineCamera ; Movie Render Queue, désactivé par défaut, manque au `.uproject` ; ni interrupteur de HUD, ni saut d'horloge, ni mode photo.
  - Preuve : `UE_5.8/Engine/Plugins/MovieScene/MovieRenderPipeline`, storm-flood.md:85, grep `ShowHUD` : 0, docs.unrealengine.com/4.27/AnimatingObjects/Sequencer/HighQualityMediaExport.
  - Recommandation : gameplay réel en 4K ; Sequencer et MRQ (chauffe ≥ 60 images) pour grues, ralentis, plans C ; `monke.Cine` (HUD coupé), `monke.ExpeditionSkip`.
  - Effort : M

- **[Majeur] Matière : cinq plans sûrs, six fragiles**
  - Constat : sûrs (vus ou testés) : chaloupe, galion, sabre, embuscade, noix et chapeau. Fragiles : eau montante, requin, naufrage, nuit, coop (T-095/03), dandinement (T-027). Démarrage en 1re personne : le bean ne se voit qu'aux rames et en KO.
  - Preuve : galleon_pie.png, characters-tiki.md:126-149, storm-flood.md:57, IslanderCharacter.cpp:81.
  - Recommandation : tourner les cinq sûrs en un jour (`monke.SetHour 17.6`, `monke.DropCoconut`, `monke.TikiDefender`, hors Shipping), alterner 1re et 3e personne (V), sans faire de la 3e le défaut.
  - Effort : S

- **[Majeur] Son et ton à produire**
  - Constat : rame, vent, pluie, requin, cloche, voix, musique : absents. Ton visé : solennité de la Couronne contre chaos physique.
  - Preuve : audits 65, 74.
  - Recommandation : trois cartes « Bureau » lisibles muettes ; marche de la Couronne qui déraille à chaque gag ; fanfare G-C-E-G (`SW_Trumpet_Fanfare.wav`, sans licence) en signature.
  - Effort : M

- **[Bloquant commercial] Plans et mentions interdits**
  - Constat : « Crownfall », logo « TAPAO », « tiki », « Raraka », « Motutapu », villageois frappé ou tranché, Union Jack.
  - Preuve : audits 72-75, 78.
  - Recommandation : liste noire ; découpe des seuls guerriers, en plan large ; relecture polynésienne avant publication.
  - Effort : S

### Storyboard du trailer idéal (75 s)
R = vu en PIE ; M = faisable (supposé), jamais capturé ; C = à créer.
1. 0-2 s : le bean court, relique à deux mains, compteur « Montée des eaux dans 0:09 » (M)
2. 2-5 s : un guerrier bondit d'un buisson, le tricorne tombe, relique jetée dans la barque (M)
3. 5-11 s : chaloupe dorée, « Mandat n° 1 : régulariser l'île avant la marée », galion, cloche (R)
4. 11-19 s : quatre beans embarquent, butin absurde (C), village au crépuscule (M)
5. 19-26 s : mousquet, guerrier coupé en deux, noix, KO, chapeau (R)
6. 26-35 s : sermon, bean immobile, chaos autour, drapeau, « Zone sécurisée : +2:00 », fanfare (C)
7. 35-45 s : tempête, palmiers pliés, eau qui monte, « Évacuation ! Extraction dans 0:20 » (C)
8. 45-52 s : barque surchargée, eau dans la coque, aileron (M)
9. 52-58 s : galion appareille, un bean resté à terre salue (C)
10. 58-64 s : Percepteur, tampon « Cuillère en bois : 1 couronne » (C)
11. 64-70 s : frégate, salve, « Contrat rompu : l'équipage est déclaré traître à la Couronne » (C)
12. 70-75 s : titre, « 1 à 4 joueurs », liste de souhaits, fanfare, le chapeau tombe (C)

≥ 80 % de gameplay (source secondaire). Steam : 1080p, 60 i/s max, vue réelle du joueur (partner.steamgames.com/doc/store/trailer).

## Manques
1. Sermon jouable (T-054, animation, drapeau, bannière).
2. Tempête : pluie, éclairs (option flashs), vent, tonnerre ; nuit capturée.
3. Percepteur (perruque, registre, tampon), frégate.
4. MRQ, `LS_*`, `monke.Cine`, `monke.ExpeditionSkip`, coop filmable à 2-4 beans.
5. Musique de 75 s, Foley ; nom et logo clearés.

## Quick wins
1. Corriger `MI_Bean_SleeveRed` (v1-rec-investisseurs.md:51), puis tourner huit clips bruts 4K sur `build/rec-T124` (`DisableAllScreenMessages`).
2. Activer MRQ ; ajouter `monke.ExpeditionSkip` et `monke.Cine` ; rendre un plan test.
3. Écrire les trois cartes « Bureau », lisibles sans son.

## Questions qui challengent l'équipe
1. Tournez-vous avant ou après le sermon, quitte à étiqueter une cinématique ?
2. Qui monte, compose et valide, quand le seul humain est aussi le testeur ?
3. Le trailer montre le bean de l'extérieur, le jeu de l'intérieur : lequel vendez-vous ?
