# Recette de la partie : ADR-007 et campagne des dix jours

- **Ticket** : T-116. Lot **L4** (boucle, sauvegarde, menu) ; L5 pour RUN13 (quatre joueurs) ; L6 pour le passage en build. Jalon J1 de l'ADR-008 (proposée).
- **Statut** : réécrite le 2026-10-10 par `ue-qa-tester`, **non passée**. Aucun cas de ce document n'a été joué pour l'écrire : ni en éditeur, ni en build, ni sur plusieurs machines.
- **Remplace** :
  - la recette RUN01–RUN14 du cahier §24.9 (partie en manches de l'ADR-005), transposée ci-dessous sous les mêmes numéros ;
  - les cas DAY01–DAY06 de l'ADR-006 (remplacée par l'ADR-007), retirés (section 2).
- **Sources** :
  - cahier §12 (n°4, 7, 9, 11, 21), §16, §17, §19, §23 (UI01–UI04, UI09, UI10), §24.9 ;
  - `docs/adr/ADR-007-boucle-galion-contrats.md`, `docs/features/campagne-dix-jours.md`, `docs/adr/ADR-008-mvp-v0-1.md` (proposée) ;
  - `docs/features/execution-equipage.md`, `death-spectator.md`, `galleon.md`, `hud-flood-timer.md` ;
  - code au 2026-10-10 : `Run/RunContract.cpp` (calcul des jours), `Run/RunRules.cpp` (valeurs par défaut), `Run/RunSubsystem.cpp` (journal).
- **Plans liés** (même vague de recettes) :
  - `plan-T-265.md` : campagne en éditeur (submersion, fin du déluge, jours débloqués, HUD, cérémonie) ;
  - `plan-T-267.md` : package v1.7 (menus, pause, écrans de fin, Créer / Rejoindre en local, achat, Continuer) ;
  - `plan-T-266.md` : barque après T-182.

  Un cas déjà couvert par l'un de ces plans y renvoie au lieu de le recopier.

## 1. Ce qui change entre le §24.9 et la partie jouée aujourd'hui

| Cahier §24 (ADR-005) | Partie ADR-007 + campagne (code du 2026-10-10) | Cas |
|---|---|---|
| Manche = une expédition puis une vente | **Jour** = une expédition sur une île tirée au hasard. La valeur rapportée à bord du galion est comptée à la fin du jour. | tous |
| Quota de manche 250 + 120 (n−1) + 30 (n−1)², × (1 + 0,25 (joueurs − 1)) | **Paliers de contrat cumulés** : 5 500 sur les jours 1–3, 10 500 sur les jours 4–6, 20 500 sur les jours 7–10 (`DA_RunRules.Zones`). **Aucune échelle par joueur** : `PlayerQuotaScale` ne sert qu'à l'ancien `FRunCalculator` (écart E2). | RUN09, RUN14 |
| Zone sûre, Balance du Percepteur, registre, taxe 35 %, bonus de contrat, signature | **Pas de vente** : la valeur à bord s'ajoute au contrat en fin de jour. Pas de taxe (ADR-008 D13). À la fin d'un contrat, l'excédent sur le quota est versé à la trésorerie (`ExcessPayoutFraction` 1,0). Frais médicaux : 50 par mort (`RecruitFee`), jamais sous zéro. | RUN08, RUN09, RUN14 |
| Lobby en zone sûre, Prêt / Pas prêt, lever l'ancre 2 s | Menu **SOLO / CRÉER UNE PARTIE / REJOINDRE**. En solo : arrivée du galion en cinématique, puis apparition sur le pont. Lobby à bord « PRÊTS POUR LE DÉPART » : **à faire** (T-172). | RUN01–RUN03 |
| Palan : barque amarrée → cale | **Pas de palan** : on monte un objet tenu par l'échelle (il suit le joueur), ou dans la caisse de transport (T-258). | RUN04 |
| Départ à la barre, vote, préavis 5 ou 10 s | **« Terminer la journée »** au gouvernail : un seul joueur décide, départ 10 s plus tard (`HelmEndDaySeconds`). Pas de vote (ADR-008 D12). | RUN05 |
| Fin de submersion : le galion part seul | **Fin du déluge** : qui n'est pas à bord meurt, qui est à bord survit, la valeur à bord est comptée (T-142). | RUN06 |
| Équipage entier mort : zone sûre après 8 s, frais | **Jour perdu** : rien n'est rapporté ce jour, le jour est consommé, frais par mort. Écran « jour perdu » au lieu de « fin de partie » : T-173 (en cours) et T-135 (review). | RUN07 |
| Quota manqué à la signature : frégate, défaite | Quota manqué **au jour jalon** (3, 6, 10) : **exécution** de l'équipage (planche, peloton français, écran EXÉCUTÉS), campagne perdue (`bExecuteOnFailedContract`, T-221). | RUN10 |
| Objet gardé en cale pour la manche suivante | Les objets ne passent pas d'une île à l'autre. Seul l'**inventaire** de chaque joueur passe (`FRunLoadout` : armes, munitions restantes). Un mort repart avec le kit de base. | RUN11 |
| Quitter en expédition → zone sûre, expédition rentrée vide | Sauvegarde `URunSaveGame` (slot `Run`) en fin de jour, au choix de cupidité et à l'achat. Marqueur `ExpeditionInProgress` : **à faire** (T-115). | RUN12 |
| Quatre joueurs, deux manches puis défaite | Quatre joueurs, jours 1 à 3, puis exécution au jour 3 | RUN13 |
| Exemple chiffré de la manche 2 | Exemple chiffré du contrat 1 | RUN14 |

## 2. Cas DAY01–DAY06 retirés

L'ADR-006 (5 jours × 10 périodes, vente au jour 5) est remplacée par l'ADR-007. Ses cas ne sont plus passés sous ces numéros.

| ID | Ce qu'il vérifiait | Pourquoi il est caduc | Où va ce qui reste vrai |
|---|---|---|---|
| DAY01 | Fin du déluge : un joueur dehors meurt, un joueur à bord survit | La règle reste vraie ; elle est déjà le §12 n°7 | RUN06 |
| DAY02 | Jour N → N+1 sur une autre île ; la cale garde les mêmes objets, à la même valeur | La cale n'est plus transportée : sa valeur est comptée chaque soir | RUN11 (inventaire conservé, autre île) |
| DAY03 | Pas de vente des jours 1 à 4 ; vente au jour 5 en zone sûre | Plus de vente, plus de jour 5 | — |
| DAY04 | Quota manqué au jour 5 → défaite ; quota atteint → période 2 avec plus de mobs | Les périodes de 5 jours sont remplacées par les paliers de 3, 3 et 4 jours | RUN10 (exécution aux jours 3, 6, 10) ; difficulté par jour : `plan-T-265.md` |
| DAY05 | 10e période passée = victoire | 10 périodes (50 jours) remplacées par 10 jours | CMP02 (victoire au jour 10) |
| DAY06 | Record affiché au menu après victoire ou défaite, conservé au redémarrage | Le record n'existe pas encore (T-115, T-106) | CMP02, ligne « record » marquée « à faire » |
