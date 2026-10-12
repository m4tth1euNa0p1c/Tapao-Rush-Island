# 28 — Expérience du mort et du spectateur, retour en zone sûre
Domaine : C · Auditeur : designer UX de la mort et du spectateur

## Verdict
Le mort n'a qu'un écran d'expulsion pour 8 à 11 min d'attente (calculé), alors que le cahier §4, §19 et §21 promet un spectateur ; aucune tâche ne le porte. Cause, noms et morts par joueur n'existent pas : les ajouter est bon marché avant T-113/T-114. Risque commercial : l'ami mort à la minute 1 quitte, et chaque zone pacifiée allonge de 120 s sa peine maximale.

## Constats
- **[Bloquant commercial] Attente du mort : 8 à 11 min, décidée par les autres**
  - Constat : mort définitive jusqu'à la traversée (§3, §24.2 étape 8). Mort à 1:00 : 8:00 d'attente avec 3 zones (120 + 3×120 + 60 s), 11:00 avec les 300 s du DataAsset. Il ne vote pas (§24.5), ne voit aucun retour estimé. Deux morts à 2:00 sur 4 joueurs, expédition de 9:00 : 14 des 36 minutes-joueur au repos (calculé).
  - Preuve : cahier §6.1 ; ExpeditionRules.h:25-37 ; ADR-001 E-07.
  - Recommandation : `DeadRecallDelay` en DataAsset (ex. 150 s) ; afficher « retour à bord au plus tard dans m:ss » (`GetSubmersionEndTime` existe).
  - Effort : S l'affichage, M le rappel

- **[Majeur] Caméra du mort : suivi borné, pas caméra libre**
  - Constat : à la mort, la caméra se colle au bassin du ragdoll et `DisableInput` coupe le regard (supposé). La voix externe est tolérée en V1 (§8) : le mort parlera aux vivants ; une caméra libre en ferait un éclaireur gratuit.
  - Preuve : IslanderCharacter.cpp:424-427, :556 ; cahier §4, §8.
  - Recommandation : suivre un vivant (←/→, orbite ≤ 30 m), filtre sourd ; libre seulement après anéantissement. Valider `SetViewTarget` sur bras de ressort en PIE à 2.
  - Effort : M

- **[Majeur] Panneau du mort (§19) impossible : ni cause, ni noms, ni morts par joueur**
  - Constat : chute et noyade appellent `ApplyHealthDamage(…, nullptr)` : « cause courte » illisible pour les morts probables d'un jeu d'inondation (supposé). Aucune classe PlayerState ; `Deaths` n'est qu'un entier.
  - Preuve : IslanderHealthComponent.cpp:113 ; IslanderMovementComponent.cpp:214 ; RunTypes.h:123,174.
  - Recommandation : `EDeathCause` répliqué ; `FRunDeathRecord` (joueur, cause, instant, abandonné) remplace l'entier, tant que T-113/T-114 sont `a-faire`.
  - Effort : S

- **[Majeur] Rôle du mort : le rappeler sur le pont plutôt que le faire regarder**
  - Constat : §4 interdit au mort d'influencer l'expédition. Classement (supposé) : 1) chaque zone pacifiée rappelle un mort sur le pont du galion, pas sur l'île (§3 respecté) : il regarde le sermon avec un enjeu, puis tient le treuil (§24.2 : un joueur doit le maintenir) ; 2) fantôme visible des vivants ; 3) pari sur le quota, gain en titre, jamais en couronnes ; 4) caméra libre.
  - Preuve : cahier §3, §4, §24.2 étape 6 ; galleon.md:22-23 (`bell`, `bean_spawn`, `PlaceOnDeck`).
  - Recommandation : ADR « rôle du mort », `DeadRole` en DataAsset (SpectatorOnly / DeckHand), rappelés sans échelle ni vote ; frais de recrutement indexés sur la manche.
  - Effort : L

- **[Majeur] Retour en zone sûre : une phrase de cahier, aucun moment conçu**
  - Constat : §24.2 étape 8 dit seulement « réapparaissent à bord ». Les 50 couronnes n'apparaissent qu'au registre, après la vente, et réduisent le net, jamais le quota (§24.4) : le mort peut se croire fautif d'une défaite (supposé).
  - Preuve : cahier §24.2, §24.4 ; RunCalculator.cpp:97-99.
  - Recommandation : à l'arrivée sur `bean_spawn`, carte du Percepteur : nom, cause, « 50 couronnes sur le net, pas sur le quota ».
  - Effort : S

- **[Majeur] Tué, abandonné, équipage perdu : un écran, un bouton**
  - Constat : NAUFRAGÉ tombe 1,5 s après la mort du pion local ; abandon par vote (§24.2 étape 7) et anéantissement (8 s) n'ont aucun écran. Le client mort n'a qu'un bouton actif, « Menu principal », sans confirmation (contraire à §16) ; revenir n'est possible qu'en zone sûre (§24.2).
  - Preuve : TapaoUISubsystem.cpp:501-506, 523-551 ; STapaoScreens.cpp:541-546 ; voir 09.
  - Recommandation : trois textes (« Tombé », « Laissé à terre par <votants> », « Équipage perdu : la cale rentre seule ») ; « Quitter » derrière confirmation.
  - Effort : S

## Manques
1. Écran spectateur (`WBP_Spectator`, §21) et suivi d'un équipier.
2. PlayerState : noms, vivant, mort, à bord.
3. Rôle du mort tranché par ADR ; arrivée « recrue ».
4. Mesure : attente, départs après mort.

## Quick wins
1. `EDeathCause` renseignée (chute, noyade, noix, tiki).
2. `Deaths` entier → tableau `FRunDeathRecord` (RUN14 compris).
3. NAUFRAGÉ retiré tant qu'un équipier vit (09) ; compteur d'inondation gardé, avec « retour au plus tard dans m:ss ».
4. Journal `LogMonkeDeath` (instant, cause, départ après mort) pour régler `DeadRecallDelay`.
5. Remplacer le réglage par réflexion de `RespawnDelay` (ExpeditionGameState.cpp:299-308) : renommé, les morts réapparaissent au PlayerStart après 6 s (contraire à §3).

## Questions qui challengent l'équipe
1. Le mort parlera aux vivants sur Discord (§8) : que doit-il voir ?
2. Quelle attente maximale acceptez-vous ? Le cahier n'en fixe aucune, l'horloge en impose 8 à 11 min.
3. Un mort rappelé tient le treuil pour 50 couronnes : qu'est-ce qui empêche de se faire tuer exprès ?
