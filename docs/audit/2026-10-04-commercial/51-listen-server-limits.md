# 51 — Limites du listen server : charge de l'hôte à 4
Domaine : F · Auditeur : ingénieur netcode (listen server)

## Verdict
L'hôte du listen server (§1) exécute IA, physique, réplication et rendu (Lumen, VSM, RayTracing, 816 000 instances) dans un seul fil de jeu, sans budget, sans mesure à plusieurs, sans repli de qualité. Seul chiffre connu : ~40 i/s en PIE solo (T-108). Risque commercial : un hôte moyen fait ramer trois clients, qui accusent le jeu (remboursement Steam).

## Constats

- **[Bloquant commercial] Aucune machine de référence ni mesure de l'hôte**
  - Constat : le moteur lit et écrit le réseau une fois par image (supposé) : à 25 FPS l'hôte ajoute jusqu'à 80 ms d'aller-retour à chaque client. `docs/perf/` n'existe pas, T-091 est à faire, la machine de référence (§12) n'est pas fixée. ≈ 4 300 acteurs répliqués × 3 connexions ≈ 13 000 évaluations par image (calculé ; 34).
  - Preuve : board T-091, T-108 ; performance.md:22-23.
  - Recommandation : hôte moyen défini (supposé : 6 cœurs, GPU 6 Go, OBS ouvert) ; 1 hôte + 3 clients `-nullrhi` avec `Net PktLag=60` ; `stat unit`, `stat net` ; seuil : 1 % bas ≤ 22 ms.
  - Effort : S

- **[Majeur] 45 à 75 IA tickent toujours**
  - Constat : 36–66 villageois et 9 gardiens gardent tick, CharacterMovement et animation même loin des joueurs ; les gardiens rôdent sans pause (2 tracés d'eau par pas) ; `RoutineActivationRadius` ne coupe que la routine. Ni URO, ni Significance Manager, ni intervalle (BP non lus). Plafonds hostiles par joueur : +60 la nuit à 4 (calculé). Budget de 2 ms : 27 µs par créature (calculé), improbable (supposé).
  - Preuve : TikiCreature.cpp:35,154,177 ; TikiWarrior.cpp:154 ; TikiRules.h:356,415 ; villages-japonais.md:143.
  - Recommandation : bandes selon la distance au joueur le plus proche (pas à la caméra de l'hôte) : acteur et CharacterMovement à 10 Hz au-delà de 60 m, coupés au-delà de 150 m.
  - Effort : M

- **[Majeur] Pics synchronisés au début de chaque manche**
  - Constat : chaque machine lit le sol de la crue à 16 000 tracés par image (défaut du code ; 102 400 cellules, 7 images), sans plafond de temps, en même temps que 4 110 butins (≈ 3 s) et le balayage de 366 000 buissons. Gel possible d'environ 1 s chez l'hôte (supposé : 5–20 µs par tracé).
  - Preuve : FloodRules.h:60,102 ; FloodSimulationActor.cpp:355-402 ; T-108.
  - Recommandation : budget de 2 ms par image, ou grille de sol cuite par fragment.
  - Effort : S

- **[Majeur] La physique de l'hôte dépend de son FPS**
  - Constat : sous-pas de 1/120 s (6 au plus) : coût proportionnel au temps d'image ; sous 20 FPS la physique décroche (supposé). La barque reçoit ses forces dans `Tick` : à 25 FPS l'hôte la simule autrement qu'un client à 60 (supposé), d'où des corrections. Plus 80 cadavres permanents (ragdoll simulé partout).
  - Preuve : DefaultEngine.ini:118-122 ; BoatActor.cpp:379-421 ; TikiRules.h:163,187.
  - Recommandation : PIE à 2 avec `t.MaxFPS 20` puis 30 ; flottaison en rappel de sous-pas ; cadavres 60–120 s ; dormance (34).
  - Effort : M

- **[Majeur] Rendu non adaptatif**
  - Constat : Lumen, VSM, RayTracing, `DefaultGraphicsPerformance=Maximum` ; ni `r.DynamicRes`, ni `sg.*`, aucun repli selon le nombre de clients. Un hôte limité par le GPU fixe le tick de tous (premier lancement probablement en Épic : 23, 26).
  - Preuve : DefaultEngine.ini:22-36,114 ; grep `DynamicRes` : 0.
  - Recommandation : `UHostLoadGovernor` : dès 2 joueurs, résolution dynamique, puis Lumen, VSM, végétation, IA lointaine, fréquence des props ; jamais joueurs, dégâts ni horloge ; hystérésis 5 s / 30 s.
  - Effort : M

- **[Majeur] Pause et alt-tab ignorent l'hôte**
  - Constat : Échap gèle le monde si `PlayerArray.Num() <= 1`, testé une fois : l'hôte seul au lobby (§24.2) fige l'ami qui se connecte (pause répliquée : moteur, supposé). Aucun code ne lit le focus : en solo l'horloge court pendant l'alt-tab (23) ; en multi l'hôte rend une fenêtre cachée et son pion reste exposé (§16).
  - Preuve : TapaoUISubsystem.cpp:428-430 ; ADR-005:89 ; grep `LostFocus` : 0.
  - Recommandation : pause en `NM_Standalone` seulement, levée à `PostLogin` ; hors focus : solo pause auto, multi `SetEnableWorldRendering(false)` et son coupé.
  - Effort : S

## Manques
1. Hôte allégé (rendu minimal) ou serveur dédié de secours (60).
2. Indicateur « hôte lent » : le §18 prévoit un onglet Diagnostic (FPS, ping), non codé ; un client ne distingue pas hôte lent et mauvais réseau.
3. Spécifications minimales de l'hôte sur la page Steam.

## Quick wins
1. Pause : `NM_Standalone` seulement, levée à `PostLogin`.
2. Lecture du sol de la crue : budget de 2 ms par image.
3. Intervalle de 1 s pour les villageois cachés ; 10 Hz au-delà de 60 m.
4. Premier relevé `stat unit` : hôte + 3 clients `-nullrhi`.

## Questions qui challengent l'équipe
1. Quelle machine d'hôte doit tenir 4 joueurs en 1080p ? Un portable à 700 € : coupez-vous alors Lumen matériel et les instances ?
2. Quand l'hôte alt-tab pendant la tempête finale, son pion est-il vulnérable (l'équipe perd un joueur) ou protégé (exploit) ?
3. Si la mesure dit non, réduisez-vous le monde ou vendez-vous un serveur dédié ?
