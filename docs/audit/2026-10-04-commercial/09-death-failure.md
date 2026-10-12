# 09 — Mort, échec et frustration
Domaine : A · Auditeur : designer de l'échec

## Verdict
La mort est une expulsion, pas une punition : NAUFRAGÉ s'affiche 1,5 s après la mort, sans spectateur ni retour (cahier §4). Le vrai coût est du temps (expédition de 6 à 12 min, ADR-001 E-07), pas les 50 couronnes ; le quota ne se juge qu'après le point de non-retour. Risque commercial : un ami mort à la minute 1 quitte, la coop se vide.

## Constats
- **[Bloquant commercial] Le mort est chassé du jeu**
  - Constat : lu dans le code, non joué (T-106) : même si des coéquipiers vivent, TAPAO affiche NAUFRAGÉ 1,5 s après la mort du joueur local : « Menu principal » seul (client) ou « Recommencer » (hôte), qui appelle `OpenLevel` sans vote et ferme la session des vivants (supposé). Entrées coupées, caméra sur le cadavre, ni spectateur ni cause de mort ; aucune tâche au chantier.
  - Preuve : `TapaoUISubsystem.cpp:497-551, 633-645`, `STapaoScreens.cpp:494-541`, `IslanderCharacter.cpp:418-431`, journal.md:266 ; cahier §4, §19.
  - Recommandation : NAUFRAGÉ seulement si `IsTeamWiped()`, « Recommencer » masqué tant qu'un coéquipier vit ; sinon caméra de suivi d'un vivant, cause de mort, temps avant le retour.
  - Effort : M

- **[Majeur] Mort instantanée à 0 d'endurance dans l'eau**
  - Constat : `bExhaustionKillsInWater = true` tue dès que l'endurance atteint 0. Nager coûte 4/s sans régénération : 25 s, soit 30 m ; avec un coffre de 20 kg, ≈ 6 s (calculé). L'alerte « Tu te noies ! » ne s'affiche qu'à 0, trop tard. Contredit cahier §4 (« pas mort instantanée ») et ADR-003 B-04 (≈ 11 s) ; non testé en PIE (T-068). Barque coulée : mort de masse probable.
  - Preuve : `IslanderMovementComponent.h:117-120`, `.cpp:198-216, 265-290` ; `BoatRiderComponent.cpp:330-337` ; journal.md:165.
  - Recommandation : défaut `false` (11 s de grâce) et alerte à 25 % d'endurance.
  - Effort : S

- **[Majeur] Punition inversée : temps lourd, couronnes légères, cale bimodale**
  - Constat : 50 couronnes (une demi-hache) n'affectent jamais le quota, jugé sur les ventes brutes, alors que le mort attend des minutes. Une équipe anéantie garde sa cale : sans conséquence si elle couvre le quota, défaite certaine sinon (manche 1 : cale vide au départ), après un rituel forcé (retour, signature, frégate).
  - Preuve : `RunCalculator.cpp:75-104` ; cahier §24.2 étapes 7 et 10, §24.4 ; ADR-005:124.
  - Recommandation : `WipeHoldLossFraction` (ex. 30 %) dans `DA_RunRules` ; bouton « Rendre le contrat » si la défaite est certaine.
  - Effort : S

- **[Majeur] Aucun retour en jeu après la mort**
  - Constat : ni relevé, ni recrutement sur l'île (§3), ni corps ramené (§4, post-V1), alors que ragdoll relevable, cadavres tiki portables à deux et volume « à bord » du galion existent.
  - Preuve : cahier §3 l.50, §4 l.61 ; `TikiCorpse.h:44-48` ; `IslanderCharacter.cpp:607-649` ; « revive/downed » : 0 résultat.
  - Recommandation : prototyper en DataAsset : (a) corps du bean portable, déposé sur le pont = recrutement immédiat pour 50 ; (b) KO de 20 s relevable ; (c) une zone pacifiée rappelle un mort au drapeau.
  - Effort : M

- **[Majeur] À la barre, ni quota visible ni abandon praticable**
  - Constat : le départ est le vrai point de non-retour, mais l'« avertissement » promis à la barre (§6.5) ne dit pas sur quelle valeur, et la confirmation n'existe qu'à la signature (§24.2 étape 10), trop tard. Le vote laisse sur l'île quiconque s'y trouve : 10 s ramènent à 12 m à la nage, 16 m en barque. Bon point : abandon facturé comme un mort.
  - Preuve : cahier §6.5 l.148, §24.2 étapes 7 et 10, §24.4 ; `RunRules.h:52-54` ; boat-rowing.md:35 ; T-112 `a-faire`.
  - Recommandation : à la barre, « à bord + cale / quota » (estimation `UValuableComponent`) avec confirmation si insuffisant ; préavis du vote = temps de retour du dernier absent, plafonné.
  - Effort : M

- **[Mineur] Anéantissement mal détecté (T-104)**
  - Constat : `CheckTeamWiped` ne s'exécute que sur mort : si le dernier vivant se déconnecte, aucun game over, les morts restent coincés. Tests : `IsWipe` seulement ; PIE solo et hôte + client non faits.
  - Preuve : `ExpeditionGameState.cpp:315-351`, board T-104.
  - Recommandation : recompter à chaque `RemovePlayerState`.
  - Effort : S

## Manques
1. Spectateur et panneau de mort (§21).
2. Un retour en jeu (corps ramené, relevé, recrutement au drapeau).
3. Jauge « cale + bord / quota » et avertissement à la barre.
4. Facture nominative du Percepteur (nom, cause de mort, 50 couronnes) : drôle, partageable.

## Quick wins
1. NAUFRAGÉ conditionnel à `IsTeamWiped()`, bouton hôte masqué si des vivants.
2. `bExhaustionKillsInWater = false` et alerte de basse endurance.
3. `WipeHoldLossFraction` dans `DA_RunRules` (0 par défaut) et test `RunSpec`.
4. Spectateur brut (suivi d'un vivant ; `IslanderCharacter` est partagé).

## Questions qui challengent l'équipe
1. Un coéquipier meurt à la minute 1 d'une expédition de 6 à 12 min : que fait-il les 5 à 11 minutes suivantes ?
2. La mort à 0 d'endurance contredit votre cahier §4 : l'avez-vous voulue, et jouée avec un coffre de 20 kg ?
3. Un crash, un hôte qui part ou une barque coulée doivent-ils coûter la partie (§24.7) ? Un sursis unique (dette royale, §7) ?
