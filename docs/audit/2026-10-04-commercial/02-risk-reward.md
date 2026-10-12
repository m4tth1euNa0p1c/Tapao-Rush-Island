# 02 — Tension risque/récompense
Domaine : A · Auditeur : designer systèmes

## Verdict
Les règles de vente sont saines et testées et deux décisions sont réelles (quoi garder en cale, comment manier la perle), mais le reste se résout de lui-même : garder domine, le danger est plat puis plafonné, la valeur abîmée n'a aucune preuve en jeu. Le joueur ne choisit jamais son risque. Risque commercial : on optimise un tableur au lieu de pousser sa chance.

## Constats
*Déjà traités : mort et taxe (09, 05), perle (05), trajet (06), sermon (03).*
- **[Majeur] La valeur abîmée n'est prouvée qu'en formule, et le respawn la blanchirait**
  - Constat : perte jusqu'à 35 % de la valeur de base par choc lourd (≥ 12 m/s) ; la perle, aux défauts (seuil 2,5 m/s, détruite à 0), meurt en trois ; chute de 1 m : −7 % ; un assommé lâche l'objet. `HandleHit` lit la vitesse après le choc et ignore `NormalImpulse` : sous-évaluation possible. Aucun écouteur de `OnValueChanged` (C++ ni .uasset) : perte invisible. `Value` privée repart à la base au `BeginPlay` : sans restauration serveur, le respawn depuis la cale remet tout à neuf. Commun, outils et armes lâchées (seuil réglé, jamais détruits) sont quasi insensibles : seuls marae et totems sont fragiles.
  - Preuve : ValuableComponent.cpp:27,48-69 ; .h:86-88 ; IslanderMovementSpec.cpp:146-156 (formule seule testée) ; BP_Marae_*, BP_Totem : seule `BaseValue` surchargée (.uasset) ; IslanderCharacter.cpp:353-364.
  - Recommandation : vitesse = `NormalImpulse`/masse ; `RestoreValue` serveur ; chiffre flottant ; test PIE.
  - Effort : S
- **[Majeur] Garder domine : passe-manche gratuit, ruine composée**
  - Constat : les invendus gardent « la même valeur », sans usure ni saisie ; le quota se juge sur les ventes de la manche, pas sur l'apport. Avec une réserve ≥ quota suivant, lever l'ancre dès l'arrivée (barre 2 s, préavis 5 s) passe la manche sans risque. Une perle indivisible vendue tôt gaspille son surplus : les rares dorment en cale. L'élimination compose les probabilités : à 90 % de réussite par manche, 35 % des parties réussissent dix manches ; à 80 %, 11 %. La manche 1 n'a aucune réserve, quand le joueur est le plus novice.
  - Preuve : cahier §24.2 étapes 7 et 9, RUN11 ; RunCalculator.cpp:75-104 ; calcul 0,9^10.
  - Recommandation : le Percepteur saisit 10 % des invendus par manche ; cale de départ ou quota réduit en manche 1 ; « jeter par-dessus bord » si la cale est pleine.
  - Effort : S
- **[Majeur] Le pont contourne le plafond de cale**
  - Constat : 40 emplacements et 600 kg ne visent que la cale. Le volume « à bord » (pont compris, ≈ 6 × 22 × 4 m) n'a aucun plafond : `GetCargoAboard` compte tout objet de valeur qui y repose, et le manifeste gèle « ce qui est dans le `CargoVolume` ». Poser ou lancer sur le pont évite palan et refus de cale pleine ; le gel au-delà de 40 n'est pas spécifié.
  - Preuve : GalleonActor.cpp:155-172 ; GalleonRules.h:90-96 ; cahier §24.1, §24.3 ; RunRules.h:70 (`HoldSlots` sans consommateur).
  - Recommandation : appliquer le plafond au gel (surplus perdu, avec message) ou compter le pont dans la cale ; test de débordement.
  - Effort : S
- **[Majeur] Le danger est plat dans la manche, plafonné entre les manches**
  - Constat : avant l'échéance rien ne s'aggrave (embuscades à la proximité, nuit toutes les 3 min), puis tempête et montée en falaise : partir tôt n'a guère de raison. Entre manches, la difficulté gagne 10 % jusqu'à ×1,5 (manche 6), le quota solo passe de 250 à 3 760 en manche 10 (×15) : la pression devient la rareté (94 $ par emplacement), sans que le danger rapporte davantage. `DifficultyForRound` n'a aucun appelant hors tests.
  - Preuve : characters-tiki.md:140-147 ; day-night-cycle.md:7 ; RunRules.h:76-82 ; RunCalculator.cpp:40-52.
  - Recommandation : rampe dans la manche (garde renforcée après 2 min, nuit) ; butin de rang 2–3 gardé ; nouveaux dangers par palier.
  - Effort : M
- **[Majeur] Le joueur ne choisit jamais son risque**
  - Constat : le contrat (fragment tiré, quota, commandes) n'offre aucun risque optionnel. Les commandes paient sans exposer, la trésorerie n'est jamais en jeu. Le genre vit du risque choisi et mieux payé.
  - Preuve : cahier §24.2 étape 2, §24.4 ; RunCalculator.cpp:89-96.
  - Recommandation : au tableau du Percepteur, 1–2 contrats à risque (nuit, gardiens ×1,5) : quota ×1,3, prime ×2 ; option « miser » une part de la trésorerie.
  - Effort : M

## Manques
1. Simulation tableur : probabilité de ruine par manche, butin par minute.
2. Contrats à risque et mise de la trésorerie.
3. Coût de détention ; jeter.
4. Retour sonore et visuel de la perte de valeur.

## Quick wins
1. Test PIE : lâcher la perle de 1 m et 3 m, lire « loses N ».
2. `NormalImpulse` pour la vitesse d'impact ; `RestoreValue` serveur, spec RUN08/11.
3. Plafond appliqué au gel du pont.
4. Chiffre flottant sur `OnValueChanged`.

## Questions qui challengent l'équipe
1. Avec une réserve, qu'est-ce qui oblige à jouer la manche ?
2. Quelle réussite par manche visez-vous ? À 90 %, une partie sur trois va au bout de dix manches.
3. La valeur abîmée a-t-elle déjà fait perdre une couronne en jeu ?
