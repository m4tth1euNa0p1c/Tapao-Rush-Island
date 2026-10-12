# 95 — Télémétrie, analytique et RGPD
Domaine : J · Auditeur : data analyst jeu vidéo

## Verdict
Le jeu ne mesure rien : ses journaux sont en texte libre, absents d'un build Shipping, et 1 seul des 22 fichiers contient des expéditions. Risque commercial : régler quota, taxe et manche de 10 min à l'intuition, puis décider démo et page Steam sans entonnoir. Voie réaliste : événements structurés en local, envoi avec consentement.

## Constats
- **[Bloquant commercial] Aucune télémétrie, et les journaux taisent l'essentiel**
  - Constat : 0 occurrence d'Analytics ou Telemetry ; 232 `UE_LOG` en texte libre (dont 28 `LogMonkeIslands`, 7 `LogMonkeEconomy`), tous absents d'un build Shipping (`bUseLoggingInShipping` faux). `Saved/Logs` : 1 fichier sur 22 contient des expéditions (2 démarrages, 0 zone pacifiée). Chute et noyade passent `nullptr` à `ApplyHealthDamage` : même ligne « died (killer: None) », sans `ExpeditionId` ni position (ERR-07), donc la noyade, mort voulue par la montée des eaux, se confond avec la chute. Déluge à 300 s (log du 04/10), 540 s (DA, T-126), 120 s (défaut C++) : sans `rules_hash`, deux playtests ne se comparent pas.
  - Preuve : `monke_island.Target.cs`, moteur `TargetRules.cs:1611`, `monke_island-backup-2026.10.04-02.54.07.log:3875`, `IslanderHealthComponent.cpp:74,113`, `IslanderMovementComponent.cpp:214`, `ExpeditionRules.h:25`, `journal.md:327`, `error-handling.md:57,86`.
  - Recommandation : `UTelemetrySubsystem` derrière `IAnalyticsProvider`, branché sur les délégués existants (`OnPhaseChanged`, `OnTeamWiped`, `OnDeath`, `OnValueChanged`), sans Tick. `EDeathCause` aux sites d'appel (cahier §19). Attributs par défaut `build`, `rules_hash`, `seed`, `players`.
  - Effort : M
- **[Majeur] Aucun entonnoir menu → manche 1 → manche 3**
  - Constat : sites connus mais sans événement (`OnPlay`, `QuitNow`, tirage du fragment, démarrage d'expédition).
  - Preuve : `TapaoUISubsystem.cpp:573-662`, `ExpeditionGameState.cpp:114`.
  - Recommandation : 12 événements : `session`, `menu_step`, `load`, `milestone` (1re rame, 1er butin à bord, 1re zone ; seuils de l'audit 30), `expedition` (déluge 540 s + 120 s/zone, fin, départ barre ou automatique), `death` (cause, t, phase, zone), `item` (base, finale, chocs, sort : vendu, gardé, laissé, brisé), `boat` (trajets, kg, naufrage), `sale`, `round` (quota, brut, taxe, net, issue), `run_end`, `abandon` (où, phase, t). Alt-F4 via `FCoreDelegates::OnPreExit`, crash via le marqueur §17 (absent du code). Un seul `FRunStats` alimente le bilan et `round`.
  - Effort : M
- **[Majeur] Économie : cinq vues, seuils écrits avant la mesure**
  - Constat : quota et taxe se règlent à la main (audit 05 : dérive ×79) ; `FRunCalculator` étant pur, des ventes enregistrées se rejouent hors ligne avec d'autres paramètres.
  - Preuve : cahier §7, `RunCalculator.h:21-36`, RUN14.
  - Recommandation : (1) marge brut/quota par manche et joueurs ; (2) passage M1→M2→M3, avec cause d'échec ; (3) valeur par source (marae, camp, village, mob) et par minute, pertes par chocs déduites ; (4) taxe : net et trésorerie non dépensée ; (5) départ vs déluge, zones, durée réelle. Fixer d'abord les seuils (ex. « M1 < 50 % : quota −10 % »), 30 manches par palier (règle empirique).
  - Effort : M
- **[Majeur] Outils : local d'abord ; Steam stats ne suffit pas**
  - Constat : le moteur 5.8 livre `AnalyticsCSV` (un CSV par événement) mais : 16 noms maximum, aucun échappement (une virgule décale les colonnes), fichier recréé à chaque lancement. Steam stats : compteurs et totaux globaux, AppID réel requis, donc succès oui, entonnoir non (inféré). GameAnalytics : plugin C++ via `IAnalyticsProvider`, gratuité et consentement non vérifiés.
  - Preuve : moteur `AnalyticsProviderCSV.cpp:22,173,224-228` et `.h:48`, https://partner.steamgames.com/doc/features/achievements, http://docs.gameanalytics.com/event-tracking-and-integrations/sdks-and-collection-api/game-engine-sdks/unreal/.
  - Recommandation : sans serveur, un puits JSONL maison derrière `IAnalyticsProvider` (un fichier par session dans `Saved/Telemetry`) ; le testeur envoie le dossier par Discord, `scripts/telemetry_report.py` agrège en HTML. Collecteur UE (`AnalyticsET` vers `APIServerET`) après un playtest fermé de 20 personnes, Steam stats avec L7a. Insights : `TRACE_BOOKMARK` aux mêmes points.
  - Effort : M
- **[Majeur] RGPD : la norme existe, pas le consentement**
  - Constat : `security.md` §6 exige consentement explicite, sans IP, chemin ni pseudo, mais `Source/` ne contient ni « consent » ni « Diagnostic ». Les `.log` bruts portent `Computer: OMEN` et `User: matth` : ne jamais les demander aux testeurs. L'exemption CNIL se limite à la mesure d'audience anonyme sans tiers ni croisement : SteamID ou données non anonymes confiées à un tiers en sortent (inféré).
  - Preuve : `security.md:81-87`, `monke_island.log:867-868`, https://www.cnil.fr/fr/mesurer-la-frequentation-de-vos-sites-web-et-de-vos-applications.
  - Recommandation : collecte locale seule, lisible, jamais envoyée sans geste du joueur ; envoi automatique en opt-in au premier lancement (refus aussi simple qu'acceptation), interrupteur dans Paramètres > Diagnostic (§18). Identifiant aléatoire réinitialisable ; ni SteamID, pseudo, IP ni saisie libre ; matériel en classes. Politique de confidentialité, conservation de 12 mois (proposée), contact d'effacement ; avis : audit 97. Pseudonymisé ≠ anonyme (supposé).
  - Effort : M

## Manques
1. Sous-système, schéma versionné, consentement, onglet Diagnostic, `EDeathCause`, `FRunStats`.
2. Rapport local, seuils pré-enregistrés, politique de confidentialité.

## Quick wins
1. `scripts/log_events.py` : relit `Saved/Logs` (lignes `Expedition`, `died`, `loses`), retire `Computer:` et `User:`, sort CSV et résumé (½ j).
2. Ligne de mort enrichie (`cause`, `t`, `phase`, `vivants`), `build` et `rules_hash` dans la ligne `Expedition … started` (½ j).
3. Seuils de décision avant le premier test, `docs/playtests/seuils.md` (1 h).

## Questions qui challengent l'équipe
1. Que décidez-vous si M1 est réussie à 30 % ou à 90 % ? Qui fixe les seuils avant le test ?
2. Combien d'inconnus auront fini trois manches avant la page Steam ? Que prouve n = 4 ?
3. Qui est responsable de traitement et répond aux demandes d'effacement ?
