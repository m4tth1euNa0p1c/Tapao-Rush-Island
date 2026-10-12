# 19 — Endgame : que fait-on après 10 manches ?
Domaine : B · Auditeur : designer endgame

## Verdict
Après la manche 10, rien n'est prévu : le cahier §24 n'a qu'une issue, la défaite. Score, classement, seed, mode, prestige et fin d'histoire sont absents. Le quota est quadratique (pas cubique) : l'offre de butin et le stockage en cale, non la courbe, font varier la partie de 5 à 16 manches (1,5 à 5 h, supposé). Risque : qui perd à la manche 6 repart sans rien de persistant.

## Constats
- **[Bloquant commercial] Aucune fin, aucune victoire**
  - Constat : la partie ne finit que par défaite ou abandon, sans `Victory` ; l'écran « Île conquise » de TAPAO existe (`NotifyVictory`) mais rien ne l'appelle.
  - Preuve : RunTypes.h:11-17, TapaoUISubsystem.h:57-59, cahier §24.1.
  - Recommandation : `Victory` et `FinalRound` dans DA_RunRules, manche finale « Gouverneur disparu », épilogue satirique, puis « Contrat illimité ». Viser ~2 h.
  - Effort : M (L pour la manche finale).

- **[Majeur] L'offre et le stockage fixent la durée, pas la courbe**
  - Constat : Quota(n) = 250+120(n−1)+30(n−1)² : 1 210 à la manche 5, 3 760 à la 10 (×1,75 à 4 joueurs). Offre par île : 2 marae de 1 900 $ chacun (perle 1 500), butin commun à 16,5 $ en moyenne, cale de 40 emplacements. Sans stockage, le mur tombe à la manche 5 (4 joueurs) à 7 (solo) pour un équipage moyen (2 300 $/manche, supposé), 7 à 10 pour un parfait (3 800 $). Avec stockage de perles (permis) : 8 à 12, puis 11 à 16, donc peu de pression pendant 5 à 10 manches, puis une falaise.
  - Preuve : RunRules.h:30-38,70, RunCalculator.cpp:40-46, marae-des-perles.md:16-18, MaraeRules.h:83, import_loot.py:27-33, cahier §24.2 étape 9 ; non mesuré (palan et vente non codés).
  - Recommandation : borner le stockage par une règle satirique (droits de douane : −10 % des invendus par traversée), fixer la durée cible puis déduire la courbe, faire croître l'offre avec l'équipe.
  - Effort : M.

- **[Majeur] Après la manche 6, seul le chiffre change**
  - Constat : difficulté plafonnée à ×1,5 dès la manche 6. Le tirage n'exclut que l'île précédente : sur 20 fragments, 44 % de revoir un fragment en 6 manches, 90 % en 10.
  - Preuve : RunRules.h:76-82, RunCalculator.cpp:48-52, ExpeditionIslandSubsystem.cpp:493, calcul (tirage uniforme).
  - Recommandation : « mutateurs de contrat » cumulables dès la manche 4 (nuit permanente, requin ×2) : monter par la friction, pas par le quota.
  - Effort : M.

- **[Majeur] Aucun score ni statistique persistante**
  - Constat : le bilan promis (ventes totales, meilleur objet, morts) n'est stocké nulle part : `FRunProgress` garde trésorerie, manches gagnées, clos. Un invité n'importe aucune progression, Steam n'est pas intégré. « Standard » flotte (120 s au cahier, 300 s au DataAsset) : sans version des règles, aucun score n'est comparable.
  - Preuve : RunTypes.h:132-226, cahier §24.2 étape 10 et §16, monke_island.Build.cs:48-51, ADR-001:45.
  - Recommandation : `FRunStats` (ventes totales, meilleur objet, morts, durée, seed, hash des règles) dans SG_Run, records et titres dans SG_LocalProfile, classement par ventes totales plutôt que par manches (ex æquo au mur).
  - Effort : S (champs), M (profil), L (classements Steam, après L7).

- **[Majeur] Seed partagé impossible aujourd'hui**
  - Constat : le cahier promet un « seed optionnel », mais chaque générateur tire `FMath::Rand()` quand sa seed vaut 0, seule l'arrivée accepte `?Seed=`, et `FRunProgress` n'a pas de seed.
  - Preuve : LootScatterSpawner.cpp:128, CampSpawner.cpp:120, TikiDirector.cpp:951, BoatStart.h:17-18, cahier §16.
  - Recommandation : un `RunSeed`, `RoundSeed = Hash(RunSeed, manche)` pour île, butin, camps, marae, tiki, code de contrat copiable au lobby, test « même seed, mêmes positions ». Même carte suffit à un défi hebdo (seed = semaine ISO, sans serveur).
  - Effort : M.

- **[Majeur] La trésorerie n'a pas de puits**
  - Constat : la boutique connue coûte 550 $ (hache, lanterne, +4 places de barque), la trésorerie atteint 2 679 après la manche 5 et 11 695 après la 10 (solo, quota juste atteint, sans mort), puis disparaît à la défaite.
  - Preuve : cahier §7 et §24.4, calcul sur RunCalculator.cpp:75-104.
  - Recommandation : améliorations à paliers et prix croissants ; à la défaite, « indemnité de licenciement » qui nourrit grades royaux et cosmétiques (prestige satirique).
  - Effort : M.

## Manques
1. Fin de campagne puis mode illimité.
2. Score de partie, profil, grades et succès persistants.
3. Seed de contrat, contrat de la semaine, classements Steam (après L7).
4. Niveaux de « Surveillance de la Couronne » (mutateurs, score multiplié).

## Quick wins
1. `RunSeed`, hash des règles et date dans l'état de partie et SG_Run (T-113, T-115) avant leur écriture.
2. `FRunStats` alimenté par `ResolveRound`.
3. `Victory` et `FinalRound` (0 = illimité), relier « Île conquise ».
4. Console `monke.RunQuotaTable` : quotas 1 à 20 × 1 à 4 joueurs face à l'offre.
5. Exclure les 5 derniers fragments du tirage.

## Questions qui challengent l'équipe
1. Quelle durée cible, et à quelle manche un équipage moyen de 4 doit-il perdre ? Aujourd'hui la cale et le nombre de marae décident.
2. Que voit-on après une victoire, ou une défaite à la manche 6 : quel écran, quel chiffre, quelle récompense donne envie de relancer ?
3. Trois joueurs sur quatre n'enregistrent rien (§16) : pourquoi reviendraient-ils sans héberger ?
