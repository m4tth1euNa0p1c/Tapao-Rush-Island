# 58 — Tests réseau : stratégie concrète
Domaine : F · Auditeur : QA réseau

## Verdict
Rien n'a été testé sur deux machines : le seul PIE à deux (T-095) a tenu sur `Lvl_Raraka` mais perdu son client depuis `L_Boot`. Risque commercial : découvrir en bêta que la coop diverge, saccade ou perd les joueurs au voyage.

## Constats

- **[Bloquant commercial] Aucun protocole : réglages PIE par défaut, menu inadapté**
  - Constat : réglages PIE actuels par défaut : `PIE_Standalone`, 1 client, un processus, émulation coupée. JOUER fait `OpenLevel ?listen` : pas une entrée multi-client (supposé).
  - Preuve : monke_island/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini:141-158 ; TapaoUISubsystem.cpp:644.
  - Recommandation : toujours `Lvl_Raraka?Island=FRG_Raraka_01`. **N1** PIE Listen Server (éditeur réservé), 2 puis 4 joueurs, un processus par client, émulation cible Client. **N2** `-game` : hôte `UnrealEditor.exe monke_island.uproject "<carte>?listen?Island=…" -game -abslog=host.log`, clients `… 127.0.0.1 -game -nullrhi -nosound`. **N3** deux PC en LAN, build `ue.ps1 package -Config Development` (Shipping coupe l'émulation), UDP 7777 ouvert. Seul N3 vaut « testé multi-machines ».
  - Effort : S

- **[Bloquant commercial] Impossible de piloter un joueur ni de comparer des états**
  - Constat : 12 commandes console, aucune ne fait agir un joueur ; pas de clavier par MCP ; `ue.ps1 test` et `ue-test` n'ouvrent qu'un processus.
  - Preuve : grep `FAutoConsoleCommand` ; fragments-tuamotu.md:385 ; ue.ps1:86-97 ; SKILL.md:12-13.
  - Recommandation : `UNetTestSubsystem` hors Shipping (`-NetTest=<scénario> -NetRole=host|c1|c2`), vrais chemins : `RequestGrab`, siège. `NETDIGEST` : SHA, phase, échéances, équipage, `ItemId:valeur:état`, `OutBytesPerSecond`, restreint aux objets pertinents (< 80 m, 34) : comparer des intersections. `scripts/nettest.ps1` lance, applique le profil, tue ou quitte, compare, écrit `result.json`. Dès maintenant : S0 (hôte rame 60 s, client debout dans la barque prend et dépose un prop) et n°14 (`AddZoneBonus` à 75 s : échéances 240 puis 360 s identiques).
  - Effort : M

- **[Majeur] n°2 et n°3 : la règle testée n'est pas celle du code**
  - Constat : §5 impose une seule prise ; `bAllowSharedGrab = true` (ADR-002 P-06) admet deux porteurs ; le refus n'est qu'un log. n°3 : `EndPlay` libère l'objet, un arrêt brutal n'est vu qu'après `ConnectionTimeout` (supposé).
  - Preuve : IslanderGrabComponent.h:97, .cpp:48-55, 310 ; BaseEngine.ini:1855 (60 s).
  - Recommandation : n°2 en deux variantes (partage coupé : un gagnant, un refus reçu ; actif : un exemplaire, deux porteurs), deux clients, même prop, même tick. n°3 : `taskkill /F` du porteur ; objet libre, reprenable, unique en ≤ 65 s ; sortie propre ≤ 2 s.
  - Effort : S

- **[Majeur] n°10 : profils indéfinis, `PktLag` mal compris**
  - Constat : §12 dit « 100 puis 200 ms » sans RTT ni sens. `PktLag` et `PktLagVariance` ne retardent que le sortant ; les profils moteur retardent aussi l'entrant.
  - Preuve : cahier §12 n°10 ; NetDriver.h:462-535 ; BaseEngine.ini:3541-3557.
  - Recommandation : profils sur chaque client, hôte sain : P1 40–60 ms par sens, 1 % ; P2 90–110 ms par sens, 3 % ; P3 « Bad » + `PktDup=2`, puis `PktOrder=1` seul (incompatibles) ; `NetEmulation.DropAnyUnreliable 20`. Écrits en `[PacketSimulationProfile.Crownfall100|200|Hostile]`, appliqués par `-PktEmulationProfile=`.
  - Effort : S

- **[Majeur] n°12, RUN03, n°8, RUN02, RUN13, n°11 : code absent, tests rouges d'abord**
  - Constat : ni gestionnaire d'échec réseau (`NetworkFailure`, `PreLogin`, `?closed`) : un hôte perdu renverrait le client au menu sans motif (supposé). Ni lobby, ni « prêt », ni vote, ni `L_Galleon`.
  - Preuve : grep `Source/` (0 hors template) ; RunRules.h:54 ; Content/MonkeIsland/Maps (3 cartes).
  - Recommandation : chaque tâche L4 livre son scénario `-NetTest` (T-116). n°12 : hôte quitté puis tué ; chaque client rejoint le menu avec un motif (≈ 60 s attendu, supposé). RUN03 : client lancé en `Phase=Expedition`, refus lisible, puis accepté en zone sûre. RUN02 : refus nommant le « pas prêt ». n°8 : vote 3 sur 4, joueur à terre, préavis 10 s lu par tous à ±1 s. RUN13 et n°11 : 4 processus, 2 manches puis défaite, digests égaux à chaque phase. Voyage ×3 en `-game` (seamless limité en PIE, ADR-005) : mêmes joueurs et `FragmentId`, RAM stable.
  - Effort : L

- **[Majeur] Seuils de réussite et coût machine non fixés**
  - Constat : critères épars (budget réseau, gate INT), jamais mesurés ; un `-game` non cuit charge `Lvl_Raraka` (110 Mo) sur un PC de 31,7 Go (42).
  - Preuve : performance.md:31,80 ; error-handling.md:63.
  - Recommandation : réussite = digests égaux (échéances identiques, compteurs ±1 s), zéro `Error:`/`ensure`/`Fatal`, ≤ 16 Ko/s moyen et 64 Ko/s en pic par client (`stat net`), corrections de mouvement (`p.NetShowCorrections 1`) calibrées sur S0. `L_TestBed` (36) pour 90 % des scénarios ; `Lvl_Raraka` pour voyage et soak 60 min. Network Insights : `-trace=default,net -NetTrace=1`. Ordre : S0, n°2, n°3, n°14 sous P0, P1, P2 ; P3 par lot.
  - Effort : M

## Manques
1. `L_TestBed`, `UNetTestSubsystem`, `nettest.ps1`, profils : rien n'existe.
2. 2e PC prêt : build Development, même version, pare-feu.
3. Soak 60 min à 4.

## Quick wins
1. S0 aujourd'hui : PIE à 2, 100 ms côté client.
2. Trois profils dans DefaultEngine.ini.
3. `nettest.ps1` minimal : hôte + client `-nullrhi`, 90 s, grep `Error:|ensure|Fatal`.

## Questions qui challengent l'équipe
1. Quel PC, quel jour, quel build pour le premier test LAN ?
2. Prise partagée façon R.E.P.O. ou prise unique du §5 : lequel est le jeu ?
3. Un `nettest` rouge bloque-t-il la fusion, l'INT étant déjà manuel ?
