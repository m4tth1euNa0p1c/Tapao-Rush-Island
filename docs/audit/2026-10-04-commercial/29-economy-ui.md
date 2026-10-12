# 29 — Clarté de l'interface économique
Domaine : C · Auditeur : designer UI économique

## Verdict
Aucun écran d'argent (Balance, registre, boutique, bilan) n'existe (T-114 `a-faire`) : imposons « une question, un chiffre géant par écran ». Calculs exacts (RUN14), mais données piégées : défaite créditée, trois « or », compteur du HUD qui baissera à la vente. Risque : perdre sans comprendre pourquoi (supposé : remboursements).

## Constats
- **[Bloquant commercial] Défaite signée à l'aveugle**
  - Constat : la défaite est irréversible (§24.7), sa confirmation n'a aucun contenu défini, le HUD aucun quota. `ComputeRound` verse net et commandes même en défaite : 499 vendus pour 500 afficheraient « +325 » sous « traître à la Couronne ».
  - Preuve : §24.2 l.531 ; `RunCalculator.cpp:84-100` ; `RunSpec.cpp:91-97` ; `UI/` sans « quota ».
  - Recommandation : `bCredited` (net et commandes à 0 si défaite) + test ; confirmation chiffrée, défaut sûr :
    ```
    SIGNER 180 SOUS LE QUOTA ? Ventes 320 / 500 : partie close.
    Invendu en cale : 4 objets, 240. [Retourner vendre] [Signer la défaite]
    ```
  - Effort : S

- **[Majeur] Quota brut et argent net se lisent comme un seul nombre**
  - Constat : on juge 620 / 500, on reçoit 453 ; la liste du registre (§24.2) omet bonus et trésorerie : elle ne se somme pas ; le §7 dit encore « Le quota porte sur les quantités » ; l'écran de fin existant centre ses lignes ; un bois compte deux fois (ventes, commande) : non dit.
  - Preuve : §24.2 l.530, §7 l.156 ; `STapaoScreens.cpp:510-514` ; `RunCalculator.cpp:81-96`.
  - Recommandation : widget dédié, deux blocs (CONTRAT réussi/raté avant taxe ; ARGENT en addition dans l'ordre du calcul), total géant ; en vente, seulement ventes et quota (voir 01) ; `Deaths` est un entier : prévoir noms et causes (voir 09).
    ```
    REGISTRE · manche 2 · 2 marins
    CONTRAT   620 / 500 (400 x 1,25)   REMPLI
    ARGENT
    Ventes avant taxe               620
    - Taxe Couronne 35 %           -217
    + Bonus (non taxé)             +100
    + Commandes                     +40
    - Recrutement : Bob, noyé       -50
    VERSÉ À LA TRÉSORERIE          +493  (300 → 793)
    ```
  - Effort : M

- **[Majeur] Celui qui vend ne voit ni quota, ni neuf, ni trésorerie**
  - Constat : le compteur du HUD (« {0} $ ») somme la valeur de la cale, « compteur de pièces » pour la fiche du galion : chaque vente le diminuera alors que la trésorerie monte, et rien ne la montre. L'invite n'affiche que la valeur courante ; neuf, perte et erreurs de vente (3) sans texte.
  - Preuve : `STapaoHUD.cpp:143-150,260` ; `galleon.md:29` ; `IslanderHUD.cpp:139,157` ; vocabulaire : voir 13.
  - Recommandation : HUD « CALE » ; en zone sûre « VENTES 320 / QUOTA 500 » et « TRÉSORERIE » ; Balance :
    ```
    BALANCE · Bûche   70 (neuf 100, chocs -30) · verrouillée   [Vendre]
    Ventes 320 → 390 / quota 500 · Commande Bois 4/4 : +40
    ```
  - Effort : S

- **[Majeur] Boutique : trois joueurs sur quatre regardent**
  - Constat : seul l'hôte achète ; `EEconomyPurchaseError` n'a que 3 refus (rien pour « au maximum », « réservé à l'hôte ») ; `ShopPrices` ne porte que des prix, or le §19 exige avant/après (le prochain quota n'y figure pas).
  - Preuve : §7 l.171, §19 l.377 ; `EconomyTypes.h:44-55` ; `EconomyRules.h:48-50`.
  - Recommandation : DataAsset d'article (libellé, icône, avant/après) ; souhaits d'équipage visibles de l'hôte :
    ```
    BOUTIQUE · Solde 793 · prochain quota 763, en cale 240
    Barque +4 places 12 → 16   300   [Souhaiter] 2/3 · achat réservé à l'hôte
    ```
  - Effort : M

- **[Majeur] Trois « or », deux livres de comptes**
  - Constat : `FRunProgress.Treasury`, `FEconomyCampaignState.Gold` (débité par `TryPurchase`) et `UTapaoSaveGame.Gold` coexistent ; `FExpeditionLedger` (règles v1.2) double `FRunRoundResult` : un écran branché sur le mauvais montre des règles mortes (bois, vivres, réserve).
  - Preuve : `RunTypes.h:213` ; `EconomyCalculator.cpp:111,151,186` ; `TapaoSaveGame.h:44-46`.
  - Recommandation : une `Treasury` répliquée ; `FExpeditionLedger` obsolète ; `FRunRoundResult` seul contrat d'écran, plus `TaxBase`, `QuotaBase`, multiplicateur (ADR-005 §11 : widgets sans calcul).
  - Effort : S

- **[Mineur] Chiffres en colonne illisibles**
  - Constat : Alegreya SC (compteurs) : chiffres proportionnels et inégaux (« 1 » : 377, « 0 » : 467), ni U+202F (espace fine du français), ni U+269C, ni U+2713.
  - Preuve : analyse des TTF `art/ui/tapao/fonts/` ; supposé : Slate sans `tnum`, séparateur fr U+202F.
  - Recommandation : police OFL à chiffres tabulaires pour les montants ; formatage maison (U+00A0, moins U+2212 : présents) ; test en build packagé.
  - Effort : S

## Manques
1. Rapport d'expédition à l'arrivée en zone sûre (le bilan du §19 n'a pas d'étape dans le §24).
2. Invendus en cale listés avec valeur.

## Quick wins
1. HUD « CALE », une seule monnaie.
2. `bCredited` + test `RunSpec`.
3. Cahier : §24.2 étape 9 (bonus, trésorerie), §7 l.156 (quota en valeur).
4. Test des 3 secondes : maquettes imprimées, 5 non-joueurs, « Contrat rempli ? Combien reçoit-on ? ».

## Questions qui challengent l'équipe
1. L'hôte peut-il signer seul la défaite de trois autres joueurs ?
2. La taxe est-elle un gag subi ou un calcul à comprendre ? Pas les deux en 3 secondes.
3. Trois joueurs regardent un seul acheter avec l'argent de tous : l'assumez-vous ?
