# 18 — Fonctionnalités sociales et facteur « clip »
Domaine : B · Auditeur : designer social et viralité

## Verdict
Le jeu a de bons moteurs de chaos (noix visées sur la tête, culbute, sabre qui tranche, barque qui coule) mais presque aucune couche sociale : une emote, zéro ping, zéro voix, quatre beans identiques et anonymes. Un clip exige un événement, un témoin audible, une victime identifiable et un outil de partage ; seul le premier existe. Risque commercial : des moments muets, illisibles et non partageables ne créent pas la viralité dont le genre vit.

## Constats
- **[Bloquant commercial] Pas d'identité de joueur**
  - Constat : ni PlayerState, ni noms, ni couleurs, ni équipage au HUD ; aucun code ne varie tenue ou couleur ; menu sans « Rejoindre ». Lobby, votes, frais de mort, cosmétiques et spectateur en dépendent.
  - Preuve : grep `PlayerState` : aucune classe ; `STapaoScreens.cpp:143-148` ; T-028 `a-faire` ; cahier §21 l.411, RUN02.
  - Recommandation : T-028 d'abord, étendu : PlayerState répliqué (nom, couleur, skin, prêt), noms sur les équipiers, équipage au HUD.
  - Effort : M
- **[Majeur] Voix de proximité absente, repoussée en L7**
  - Constat : le cahier tolère la voix externe en V1 ; rien n'est branché. C'est pourtant le moteur de partage du genre, et le sermon n'a qu'une voix préenregistrée.
  - Preuve : cahier §8 l.185 ; `backlog.md:35` ; `monke_island.Build.cs:49` ; grep `voice|voip` : tiki et un curseur de volume.
  - Recommandation : choisir Steam ou EOS avec les sessions (#56) ; prototype positionnel dès que le multi tourne, PTT par défaut. Idée : sermon « lu à voix haute » devant un prompteur satirique.
  - Effort : L
- **[Majeur] Le bean n'a aucune voix**
  - Constat : ni cri, ni « oof », ni noyade ; noix, culbute, chute et naufrage se jouent en silence.
  - Preuve : aucun son vocal dans `Player/` ni `Character/` (grep).
  - Recommandation : 10 à 15 cris tirés du CC0 déjà licencié (`art/SOURCES.md`), aigus (×1,4), sur `Knockout`, `Landed`, naufrage, mort.
  - Effort : S
- **[Majeur] Emotes : une seule, deux clips prêts jamais joués**
  - Constat : trompette V (5,1 s, recharge 6 s, mélodie unique, interdite assis ou à la nage), sans effet de jeu ni test. `Wave` et `Celebrate` sont importés, jamais joués.
  - Preuve : `TrumpetEmoteComponent.cpp:114-135` ; `BeanAnimLibrary.h:87-93` ; `BeanAnimDriverComponent.cpp:220,267`.
  - Recommandation : `UEmoteComponent` générique (enum répliqué, débit limité), roue de 6, 3e personne forcée pendant l'emote ; la trompette devient « rappel à la barque ».
  - Effort : S / M
- **[Majeur] Aucun ping**
  - Constat : le cahier ne prévoit les pings que sur une carte tactique inexistante, inutile en 1re personne.
  - Preuve : cahier §19 l.375 ; grep `ping` : 0, carte tactique absente ; `IslanderHUD.cpp:306` projette déjà des marqueurs.
  - Recommandation : ping monde (molette) : Ici / Danger / Butin / Barque, couleur du joueur, 8 s, 1 par seconde, RPC validé (SEC-02/03).
  - Effort : M
- **[Majeur] Les moments « clip » ne sont pas mis en scène**
  - Constat : le sermon n'existe pas. Le ragdoll n'est pas « actif » et chaque machine simule le sien. Naufrage : message de debug, passagers lancés sans ragdoll, ni son, ni chavirage géré. « TRANCHÉ ! » est du texte, sans sang.
  - Preuve : grep `sermon` : 0 ; T-027 `a-faire` ; `IslanderCharacter.h:302` ; `BoatActor.cpp:920,945` ; `IslanderHUD.cpp:315` ; `CoconutFallRules.h:44` (30 % de noix visées).
  - Recommandation : `Knockout` des éjectés, annonce et son du naufrage, ralenti sur les coups « signature », sermon prioritaire.
  - Effort : S / M
- **[Majeur] Cosmétiques : une tenue, un chapeau, aucun choix**
  - Constat : meshes par défaut, pas de `DA_BeanSkin` ; le cahier ne vise que 3 variantes.
  - Preuve : `BeanPuppetComponent.h:61-70` ; cahier §10 l.214.
  - Recommandation : chapeaux et couleurs gagnés en jeu, jamais vendus avant l'accès anticipé ; palmarès du Percepteur (« Casse-tout de la Couronne »).
  - Effort : M / L
- **[Mineur] Mode photo, replays, marqueurs de clip : rien**
  - Constat : 0 code ; un replay exige une réplication fiable, jamais testée à deux.
  - Preuve : grep `PhotoMode|DemoNetDriver` : 0.
  - Recommandation : pas de replay avant 1.0 ; « Avis de recherche » partageable (PNG des 4 beans et bilan, §24.2) ; marqueurs Steam Timeline (supposé, à vérifier).
  - Effort : M / S

## Manques
Noms et couleurs, voix de proximité, pings, roue d'emotes, cris du bean, choix de skin, palmarès partageable, spectateur, mode photo, replays.

## Quick wins
1. Brancher `Wave` et `Celebrate` sur deux touches (`PlayFullBody`, compteur répliqué).
2. Forcer la 3e personne pendant l'emote (`SetForceThirdPerson`).
3. `Knockout` des éjectés du naufrage + annonce HUD.
4. Huit cris du bean (CC0 du dépôt) sur chocs et mort.
5. Nom des équipiers au-dessus des beans.

## Questions qui challengent l'équipe
1. 1re personne par défaut (ADR-002 P-03) : un joueur qui ne voit pas son bean peut-il créer un clip ?
2. Si tout le monde parle sur Discord, la voix de proximité est-elle un différenciateur ou deux mois de plomberie, le multi n'ayant jamais tourné à deux ?
3. Trancher des « villageois » en deux, sans sang : clip viral ou motif de démonétisation et de refus ? Qui arbitre avant la page Steam ?
