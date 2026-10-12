# 59 — 4 ou 8 joueurs : impacts design et technique
Domaine : F · Auditeur : producteur technique

## Verdict
Le noyau ne plafonne pas les joueurs (calculateur sans limite, plafonds de tiki par joueur), mais barque, cale, sermon et quota sont calibrés pour 4, et aucun équipage n'a été joué sur deux machines. Recommandation : 4 au lancement, pas 8 ; 6 en option après l'accès anticipé, sur mesures ; N paramétrable dès T-112 à T-116. Risque : promettre plus de joueurs qu'on ne peut en prouver.

## Constats
- **[Majeur] La barque est faite pour 4–5 personnes, pas 8**
  - Constat : 8 joueurs = 480 kg sur 500 kg max : 20 kg de butin. Avec les 150 kg du cahier : 630 kg, naufrage en ≈ 5 s ; franc-bord 14 cm contre 27 à 4 (calc., coque 60 cm). Plafond avec 150 kg : 5 joueurs. Seul le rameur s'assoit : les sièges ne limitent rien.
  - Preuve : BoatRules.h:37,41,45,49,61,171 ; BoatPhysics.h:34-59 ; BoatPhysicsSpec.cpp:50.
  - Recommandation : une barque par tranche de 4 joueurs.
  - Effort : M

- **[Majeur] Quota et cale ne passent pas à l'échelle**
  - Constat : la part de quota par tête tombe à 44 % du solo à 4 joueurs, 34 % à 8 (manche 5 : 530 contre 416). La cale (40 emplacements) reste fixe : sur le plafond ≈ 5 000 $ de l'audit 05, mur en manche 7 à 8 joueurs, 9 à 4 (calc.).
  - Preuve : RunCalculator.cpp:44 ; RunRules.h:42,70-74 ; audits 05, 07.
  - Recommandation : table DA joueurs → quota, cale, sermon, nuit ; équilibrer à 4 d'abord.
  - Effort : S (table), M (équilibrage)

- **[Majeur] Sermon, rôles, IA : l'unité de conception est 4**
  - Constat : un officiant par drapeau, 3 zones : 5 joueurs sur 8 n'officient pas ; le « chœur » proposé en 03 (durée ÷ officiants) tombe de 5 s à 2,5 s. Rôles du cahier : quatre. Plafonds de tiki par joueur : 36 aux trousses à 4, 72 à 8.
  - Preuve : cahier §6.2 l.114, §6.3 l.124 ; TikiRules.h:415.
  - Recommandation : tables par N (sermon, contre-attaque), pas plus de joueurs.
  - Effort : M

- **[Majeur] Hôte : ×2,3 connexions, zéro mesure**
  - Constat : la réplication de l'hôte suit le nombre de clients (3 → 7). Sur le budget par client (16 Ko/s moyen, 64 en pic), son envoi passe de 48/192 à 112/448 Ko/s (calc.). 4 110 props et 36–54 villageois répliqués sans fréquence réglée ; aucune mesure.
  - Preuve : performance.md:31 ; audits 34, 50 ; moteur BaseGame.ini:68 (MaxPlayers 16).
  - Recommandation : mesurer à 2, 3, 4 joueurs (bots, `stat net`), extrapoler la pente par client.
  - Effort : M

- **[Bloquant commercial] Aucune taille d'équipage n'est prouvée ; 8 ne le sera jamais**
  - Constat : RUN13 (4 joueurs) n'a jamais tourné ; un humain, deux PC. 8 processus × 8 Go de budget = 64 Go pour 31,6 Go : 8 joueurs = bots seulement, jamais « testé multi-machines ».
  - Preuve : BRIEF ; cahier §24.9 ; memory.md:46 ; audit 50 ; ADR-001 E-05 (2–4).
  - Recommandation : annoncer « 1–4 » ; 5+ « expérimental » derrière une option ; autre plafond = ADR.
  - Effort : S pour annoncer ; L pour viser 8 (≈ 4 sem., supposé)

- **[Majeur] Marché : 4 est la norme, 6 l'exception, 8 un hack de mod**
  - Constat : Steam : R.E.P.O. « up to 6 players », Deep Rock Galactic « 1-4 player », Peak « up to three other scouts » (4). Absent du texte Steam lu : Lethal Company 4 (mémoire ; mods au-delà), Content Warning 4 (mission). Huit amis acheteurs sont rares, une collab de 4 streamers est simple (supposé).
  - Preuve : https://store.steampowered.com/app/3241660, /548430, /3527290 (6 requêtes API appdetails ; WebSearch bloqué : limite de session).
  - Recommandation : vendre décor, prix et chaos physique à 4 ; 6 seulement si des groupes de 5–6 sont refusés.
  - Effort : S

- **[Majeur] Code presque agnostique, mais API mono-barque**
  - Constat : un seul 4 littéral (`Index % 4` : joueurs 5–8 superposés sur le pont) ; le reste est donnée à 4 (`StandingSpots`, `MaxPushers`, 4 `Seat_*`) ou mono-barque : un `barque_dock`, `FindMooredBoat()` rend une barque. T-112 à T-116 `a-faire`.
  - Preuve : GalleonActor.cpp:293 ; BoatStart.h:116 ; BoatRules.h:276 ; barque.md:35 ; GalleonRules.h:149 ; GalleonActor.h:61 ; board.md:119-123.
  - Recommandation : `MaxCrew` dans `DA_RunRules` ; docks en tableau.
  - Effort : S (données), M (docks)

## Manques
1. Table joueurs → paramètres.
2. Deuxième barque : dock, palan, second rameur.
3. Banc de mesure à N clients, critères chiffrés pour 6.

## Quick wins
1. `MaxCrew = 4` en DA ; `GameSession.MaxPlayers` en dérive (moteur : 16).
2. `Index % 4` → rangée sur `MaxCrew`, test à 5–8.
3. `FindMooredBoat()` → `FindMooredBoats()` avant T-112.
4. `PlayerQuotaScale` → courbe DA, mêmes défauts.

## Questions qui challengent l'équipe
1. Quel plus grand équipage pouvez-vous prouver, seul humain avec deux PC, et pourquoi en promettre davantage ?
2. Que ferait un cinquième joueur : des bras, du chaos (la barque surchargée le donne déjà) ou du public ?
3. Quel chiffre (groupes de 5–6 refusés) vous ferait passer à 6 ?
