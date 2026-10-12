# 21 — Durée des transitions et temps morts
Domaine : C · Auditeur : designer UX de flux

## Verdict
Les transitions sont dominées par des planchers et des pauses, pas par le chargement : JOUER → prise en main coûte 26,7 s (calculé) pour 1,6 s de chargement réel (éditeur ; rien hors éditeur). Le plancher de 6,5 s cache le peuplement du monde, et l'ADR-005 le répéterait à chaque voyage. Risque commercial : ≈ 47 s entre le lancement et le premier geste, ≈ 28 s à chaque relance.

## Constats
- **[Majeur] Plancher de 6,5 s appliqué deux fois, pour cacher le peuplement du monde**
  - Constat : `MinLoadingSeconds` vaut 6,5 s dans l'asset (le script d'import dit 3,0). L'écran MoviePlayer dure au moins ce temps, même pour une carte vide (mesuré en `-game`, plancher alors 3,0 : 3,11 s pour L_MainMenu), puis le rideau reste 6,5 s de plus, horloge lancée. En PIE (12 à 17 essais), galion à +3,1–3,9 s, butin fini à +5,3–7,7 s : le plancher colle au peuplement.
  - Preuve : `DA_TapaoUISkin.uasset` ; `TapaoUISubsystem.cpp:172,199,365` ; `Saved/Logs/`.
  - Recommandation : signal « monde prêt » (galion posé au BeginPlay, butin semé derrière l'écran) levant le rideau, puis horloge.
  - Effort : M
- **[Majeur] Détour par L_Boot : 7,2 s mesurées pour des clients absents**
  - Constat : JOUER passe toujours `?listen`. Même seul, L_Boot attend 3,0 s, puis le moteur impose 4,000 s avant le changement de niveau (`ServerTravel` non seamless). Avec les planchers (écran, L_Boot, écran, rideau) : 6,5 + 7,2 + 6,5 + 6,5 = 26,7 s.
  - Preuve : `ExpeditionBootGameMode.cpp:75-88` ; `TapaoUISkin.h:121` ; `monke_island.log:4002-4008`.
  - Recommandation : solo sans `?listen` ; île tirée dans le menu et ouverte directement ; L_Boot supprimé en T-113.
  - Effort : S / M
- **[Majeur] L'ADR-005 reconduirait ces coûts à chaque voyage**
  - Constat : le repli non seamless (décision 4) coûterait 4 s par voyage ; `TransitionMap` est vide ; un seamless travel déclencherait `PreLoadMap` à chaque étape, donc deux planchers (supposé). Rien n'est préchargé, l'île étant tirée dès le lobby.
  - Preuve : ADR-005 §4 ; `DefaultEngine.ini:10`.
  - Recommandation : `RequestAsyncLoad` dès le tirage, pendant une mise à la voile de 5 s ; `L_Transit` habité.
  - Effort : M
- **[Majeur] Jauge décorative, aucun garde-fou de chargement**
  - Constat : la jauge suit 1 − e^(−t/4), sans lien avec un progrès, alors que le §16 n'admet un pourcentage que mesuré. `LoadTimeoutSeconds` (60 s) ne garde que le catalogue : aucun retrait d'un joueur bloqué, aucun « Réessayer ».
  - Preuve : `STapaoScreens.cpp:374-380` ; CAHIER §16 l.323-327 ; `ExpeditionIslandSubsystem.cpp:57`.
  - Recommandation : indicateur sans %, destination, joueurs chargés ; porte « tous prêts », retrait motivé à 60 s.
  - Effort : M
- **[Majeur] Relance après la mort : ≥ 28 s**
  - Constat : NAUFRAGÉ à 1,5 s, puis 26,7 s. Retour serveur à 30 s (cahier §24.2 : 8 s) ; « Recommencer » est réservé à l'hôte : un client attend ou quitte.
  - Preuve : `TapaoUISubsystem.cpp:503,547` ; `DA_ExpeditionRules.uasset`.
  - Recommandation : 8 s ; « Repartir » vers le lobby pour tous.
  - Effort : S
- **[Mineur] Démarrage à froid : ≥ 20 s avant un menu utilisable**
  - Constat : en `-game`, moteur 10,8 s, écran du menu 3,1 s (6,5 s aujourd'hui), écran-titre 2,5 s minimum (touche avalée) à 6 s. Build packagé jamais lancé.
  - Preuve : `TapaoGame.log` ; `TapaoUISubsystem.h:71` ; `STapaoScreens.cpp:424-452`.
  - Recommandation : logo ≤ 1,5 s, sautable, menu vivant derrière.
  - Effort : S

**Budget proposé**

| Étape | Constaté | Budget |
|---|---|---|
| Lancement → menu | ≥ 19,8 s | ≤ 10 s |
| JOUER → pont | 26,7 s | ≤ 8 s |
| Ancre levée → île jouable | non codé | ≤ 12 s, voile de 5 s comprise |
| Arrivée → horloge | 0 s | 10 s libres, 3 s si tous prêts |
| Palan ; préavis de départ | non codé | 3 s ; 5 à 10 s |
| Île → zone sûre | non codé | ≤ 10 s |
| Mort → relance | ≥ 28 s | ≤ 10 s |

Attentes forcées par manche, hors barque (voir 06) et vente : ≈ 45 s (6 % de 12 min).

## Manques
1. Journal de durées ; spec qui échoue si les planchers cumulés dépassent 4 s.
2. Signal « monde prêt », porte « tous chargés », « Réessayer » (§16).
3. Scènes sautables ; lobby « prêt » implicite, lancement forcé après 10 s.

## Quick wins
1. JOUER solo sans `?listen` : −7,0 s mesurées.
2. `GameOverRestartDelay` 30 → 8 s dans `DA_ExpeditionRules`.
3. `SplashSeconds=(X=0.3,Y=1.5)` dans `DefaultGame.ini` : −1 à −4,5 s.
4. Galion posé au BeginPlay (sans `SpawnDelay`) : plus de téléportation à +3,3 s.
5. `LogMonkeFlow` dans `TapaoUISubsystem`, puis mesure packagée, SSD et disque dur.

## Questions qui challengent l'équipe
1. Qui a fixé 6,5 s, pour cacher quoi, et pourquoi pas un signal « prêt » ?
2. Lvl_Raraka en build packagé sur disque dur : combien de secondes ? (seul chiffre : 1,6 s, éditeur à cache chaud)
3. À mort définitive, 28 s de relance : punition voulue ou défaut de flux ?
