# 15 — Événements et dangers
Domaine : B · Auditeur : designer d'événements

## Verdict
Les aléas sont bien bâtis (serveur-autoritaires, pilotés par DA, testés en headless), mais ils sont quatre, rejoués à l'identique à chaque manche, et le requin est absent de la boucle réelle. Le finale est muet et presque inoffensif, et la noix visée contredit l'équité du cahier §8. Risque commercial : aucune rejouabilité côté aléas, morts perçues comme injustes.

## Constats
- **[Majeur] Noix visées : danger sans signal, quasi inévitable**
  - Constat : 50 % des chutes (couronne à ≤ 6 m) sont un tir balistique sur la tête, qui anticipe la vitesse du joueur. Aucun signal avant la chute (son seulement à l'impact) ; 15 PV, ragdoll 2,5 s, pas de régénération. PIE du 03/10 : 3 KO, −45 PV en 50 s de tempête. Contredit §8.
  - Preuve : CoconutFallDirector.cpp:105-111, 223-232 ; DA_CoconutFallRules.uasset (AimedFallChance 0,5) ; storm-flood.md:57. Esquive non playtestée.
  - Recommandation : viser une zone, pas une trajectoire ; froissement de palmes et ombre 1,2 s avant ; une noix visée par 20 s et par joueur.
  - Effort : S

- **[Majeur] La tempête finale est un décor**
  - Constat : DA_WindRules divise le vent par ~9 (dérive 25 cm/s, pression 40 Pa) ; houle statique de ~40 cm ; ni pluie, ni tonnerre, ni son de vent. Rester dans la barque suffit pour survivre : seule la surcharge la coule.
  - Preuve : DA_WindRules.uasset ; WaveRules.cpp:9 ; aucun Sound ni Niagara dans Storm/ et Flood/ ; storm-flood.md:104.
  - Recommandation : prélude de 25 s, vent sonore, pluie, tonnerre avec option flashs (§8), houle liée à la tempête.
  - Effort : M

- **[Majeur] Aucun directeur d'événements ; hasard figé**
  - Constat : §8 exige budget, seed et délais minimums. Seed du vent figée (1337) : même calendrier à chaque manche. Première rafale à 300 s, 300 s d'écart : 0 à 1 rafale par expédition, à ~8 cm/s ; l'escalade n'est jamais atteinte. Noix et tiki : `FRand` sans seed.
  - Preuve : WindRules.h:44,56,64,68 ; TikiDirector.cpp:672-673 ; ExpeditionIslandDefinition.h:71 (`VariantSeeds`, aucun lecteur).
  - Recommandation : `UEventDirector` serveur (seed RunId+RoundId, budget de menace, délai minimal) pilotant rafales, noix, nuit.
  - Effort : M

- **[Majeur] Le requin n'existe pas dans la boucle réelle**
  - Constat : seule la console `monke.SpawnShark` en crée un. L'unique `Shark_Lagoon` patrouille sur 30 m près de Motutapu, à ≥ 1,3 km des 20 fragments (calculé) ; rien autour du galion (§24.3) ; morsure jamais testée en PIE.
  - Preuve : SharkActor.cpp:369-419 ; shark.md:9,26 ; fragments.json.
  - Recommandation : spawner par fragment (eau profonde près du mouillage et du galion), +1 requin dès la manche 3.
  - Effort : M

- **[Majeur] Signalement lacunaire : alertes muettes, nuit sans préavis**
  - Constat : DA_HudTimerRules ne référence aucun son : alertes 60/30/10 s, évacuation et zone sécurisée sont muettes. Crépuscule de 15 s, `OnPhaseChanged` sans abonné C++. Départ aléatoire (6 h 30-18 h) : ~50 % des expéditions de 6 min (~83 % de 10 min) croisent la nuit (calcul).
  - Preuve : DA_HudTimerRules.uasset (seulement SC_Effects) ; hud-flood-timer.md:88 ; DayNightRules.cpp:7-15.
  - Recommandation : cloche, corne, tambours ; bannière « La nuit tombe » 30 s avant.
  - Effort : S

- **[Majeur] Ni escalade ni variété des aléas entre manches**
  - Constat : le +10 %/manche (plafond +50 %) ne vise que créatures, gardiens et embuscades, et personne ne le lit. Flood, tempête, noix, nuit : identiques de la manche 1 à N.
  - Preuve : RunRules.h:76-82 ; `DifficultyForRound` sans appelant hors tests.
  - Recommandation : « bulletin météo » du contrat (calme, grain, brume, houle), tiré par seed, affiché au tableau, modulant vent, houle, visibilité.
  - Effort : M

- **[Mineur] Nuit : budget global, lisibilité non validée**
  - Constat : 4 tiki de nuit au total (4 pour un solo, 1 chacun à quatre), remplacés toutes les 6 s sans budget. Critère « la nuit reste lisible » ouvert.
  - Preuve : TikiDirector.cpp:654-680 ; TikiRules.h:308-322 ; storm-flood.md:115.
  - Recommandation : budget par joueur × difficulté ; capture de nuit avec torche.
  - Effort : S

- **[Mineur] Fiches périmées**
  - Constat : day-night-cycle.md et ADR-001 E-06 annoncent 3 min de boucle (réel : 12 min de jour, 6 de nuit) ; storm-flood.md cite 2,2 m/s de dérive (réel 0,25) et une montée à 4:50 (réel 300 s).
  - Preuve : DayNightRules.cpp:7-15 ; DA_WindRules.uasset ; DA_ExpeditionRules.uasset.
  - Recommandation : table de valeurs générée depuis les DA.
  - Effort : S

## Manques
1. Directeur d'événements et météo de contrat.
2. Événements secondaires : brume, grain, marée de vives-eaux, banc de requins.
3. Cyclone et tsunami (L8+) : variantes du flood, front de vague annoncé.
4. Danger sur la barque et au galion.
5. Lien lumière-danger ; gros butins sans risque (marae, camps : zones calmes).

## Quick wins
1. `AimedFallChance` 0,5 → 0,15.
2. Brancher 3 sons dans DA_HudTimerRules.
3. `ExpeditionStormLeadSeconds` = 25 s.
4. Bannière et son sur `OnPhaseChanged`.
5. Seed du vent dérivée du RunId.

## Questions qui challengent l'équipe
1. Une noix visée tue un joueur à 40 s de l'extraction : l'assumez-vous devant un streamer ?
2. Qu'est-ce qui distingue la manche 6 de la manche 1, hors quota ?
3. Quel aléa un joueur citera-t-il dans un avis Steam, sachant que le requin est absent et la tempête muette ?
