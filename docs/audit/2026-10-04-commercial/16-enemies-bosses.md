# 16 — Bestiaire, élites et qualité de l'IA
Domaine : B · Auditeur : designer bestiaire et IA

## Verdict
Le bestiaire tient en un comportement et trois peaux (Coco, Crabe, Veilleur : mêmes 80 PV, 15 dégâts, 4 m/s), un villageois passif et un requin posé une fois ; l'IA est correcte mais naïve (ligne droite, distance seule, pas de tactique de meute). Risque commercial : la menace se contourne en dix minutes (eau, mousquet). Rien observé en jeu.

## Constats
- **[Bloquant commercial] Trois peaux, un comportement**
  - Constat : Coco, Crabe et Veilleur partagent `ATikiWarrior`, un seul `DA_TikiRules`, un tirage uniforme. Leurs différences sont animées (parade −90 % du Crabe, onde de choc, coup imparable, combo) mais le code ne lit que les clips : coup fixe à 0,45 s pour des touches visibles à 0,47–0,77 s.
  - Preuve : TikiRules.h:94-126, TikiWarrior.cpp:170, MobAnimManifest.json, pas de `DA_Enemy` (§21).
  - Recommandation : `DA_Enemy` par archétype (PV, vitesse, coups à fenêtre de touche du manifeste, parade) : Coco rapide en meute, Crabe bouclier frontal, Veilleur lent à zone télégraphiée, tiers par manche.
  - Effort : M
- **[Majeur] Le combat se contourne**
  - Constat : dans plus de 30 cm d'eau, un joueur n'est plus ciblé. Le mousquet (70 dégâts, 80 m) porte plus loin que l'aggro (25 m) et l'abandon (50 m) : à plus de 50 m, deux balles tuent un gardien (80 PV) sans riposte. Aucune attaque à distance.
  - Preuve : TikiCreature.cpp:184-192, TikiRules.h:106,110,494, musket.md:13.
  - Recommandation : importer le Rôdeur des Ronces (sarbacane ; FBX et 20 clips prêts dans `art/export/Characters/BrambleStalker`).
  - Effort : M
- **[Majeur] IA : ligne droite, sans perception**
  - Constat : `MoveToward` va droit ; l'enlisement n'est détecté que pour l'errance, pas la poursuite. Cible = joueur le plus proche à moins de 25 m : ni vue, ni bruit, ni cri de meute. Le cahier (§21) attend une Perception ; `Variant_Shooter` en a une, inutilisée.
  - Preuve : TikiCreature.cpp:144-163, TikiWarrior.cpp:125,154,176, ShooterAIController.h:28-30.
  - Recommandation : évitement à trois rayons, `UAIPerceptionComponent` (vue 120° occultée, tir audible à 40 m), cri d'alerte à 30 m, Navigation Invokers à évaluer (supposé).
  - Effort : M
- **[Majeur] Directeur : embuscades sans signal, aveugle à la manche et au sermon**
  - Constat : un monstre par 18 s et par joueur, surgi à 11–25 m avec son rugissement, sans signal préalable (§8). Rien ne lit la difficulté de manche (+10 %, §24.6) ; 3 gardiens et 4 tiki de nuit fixes la rendraient peu lisible. Pendant le sermon, aucune pression dédiée : pas d'embuscade en village, drapeau après la mort des menaces désignées.
  - Preuve : TikiRules.h:240-256, TikiDirector.cpp:654,1036-1043, PacificationZone.h:56 (branche `dev/T-054`, non fusionnée).
  - Recommandation : buisson qui frémit 1,5 s avant, « tambours du sermon » (deux vagues finies de 2-3 tiki non désignés, annoncées), difficulté en budget, seed et élites, pas en PV.
  - Effort : M
- **[Majeur] La chasse distribue le quota**
  - Constat : tout guerrier lâche son arme (30-50 $), même la nuit ou en alarme (7 tiki par meurtre de villageois) : six à huit kills font 250 $ (quota 1), contre 1-6 $ le butin commun.
  - Preuve : TikiCreature.cpp:584, characters-tiki.md:188, vente non codée (supposé).
  - Recommandation : valeur du drop selon la source (nuit, alarme : ×0 à ×0,25), plafond de primes par manche.
  - Effort : S
- **[Majeur] Élites, mini-boss, factions : verdict**
  - Constat : factions non avant L8 (§6.3), mais un `FactionId` dès maintenant. Élites oui, dès la manche 3 (« Chef masqué » : ×2 PV, cri de ralliement). Mini-boss optionnel : aucun tiki ne dépasse 1,30 m (bean 1,05 m), et la Perle du Gouverneur (1 500 $, plus que le quota de la manche 5 en solo) n'a aucun gardien. 36-54 villageois ne servent qu'à l'alarme.
  - Preuve : characters-tiki.md:20-26, marae-des-perles.md:16,41.
  - Recommandation : Gardien du Marae (Veilleur ×1,8, réveillé quand on lève la perle), villageois public du sermon et source de nourriture sans violence.
  - Effort : M
- **[Mineur] Requin : posé une fois, sans signal ni riposte**
  - Constat : un seul `Shark_Lagoon` dans `Lvl_Raraka`, ni par fragment ni près du galion (§24.3), il ne vise que les nageurs, désarmés et lents (1,2-1,8 contre 5,2 m/s), sans son.
  - Preuve : shark.md:9, SharkRules.h:57, SharkActor.cpp:318-321, player-islander.md:24.
  - Recommandation : événement de traversée attiré par cadavres jetés ou barque surchargée ; aileron et son avant la ruée.
  - Effort : M

## Manques
1. Ennemi à distance (Rôdeur, art prêt).
2. Voleur de butin (crabe de cocotier) : menace sur le butin, moment de clip.
3. Banc d'essai IA (les 27 tests Tiki sont du calcul pur).
4. Monstres à règle (figé sous le regard) : viser 6 comportements avant l'accès anticipé.

## Quick wins
1. Pas d'arme lâchée par les tiki de nuit et d'alarme (`GetRole()`).
2. Délai de coup par archétype (0,47, 0,57, 0,63 s).
3. Un tiki qui repère un joueur appelle `Engage` sur ceux à moins de 30 m.

## Questions qui challengent l'équipe
1. Si les trois tiki sont identiques, quel monstre voudra-t-on revoir à la manche 6 ?
2. Si l'eau reste un refuge total, quel danger la fait payer à qui rame vers le galion ?
3. Un meurtre de villageois rapporte 7 tiki à farmer : punition ou stratégie ?
