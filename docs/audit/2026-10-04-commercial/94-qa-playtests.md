# 94 — Playtests internes et externes
Domaine : J · Auditeur : lead QA (playtests)

## Verdict
Aucun tiers n'a joué : seules trois parties de l'auteur (supposé) existent, connues par leurs journaux. Un test sur deux machines est faisable cette semaine, mais « 4 amis sur internet » exige quatre portes fermées (G1–G4). Risque commercial : une bêta dont survie, quota, causes de mort et abandon restent immesurables.

## Constats

- **[Bloquant commercial] Aucun protocole, aucun testeur tiers**
  - Constat : `docs/playtests/` n'existe pas et `/ue-playtest` ne simule aucune entrée joueur. Contrairement au brief, trois sessions de ≈ 11 min ont eu lieu sur packages, visibles dans les seuls journaux.
  - Preuve : `build/rec-investisseurs/smoke-menu.log`, `build/T-122/*.log`.
  - Recommandation : quatre cercles, un build par vague. A auteur, 3 sessions chronométrées. B novices en présentiel, 5 par vague, facilitateur muet, aide après 3 min de blocage. C 2 groupes de 4 amis, 90 min, Discord. D bêta de 24 joueurs du genre. Session : installation 10 min, jeu libre 15, trois manches, débrief 10. Mesures anonymes : iGEQ (14 questions) par manche, puis SUS (seuil 68), Social Presence du GEQ et NPS (indicatif). OBS local (micro séparé), appel Discord enregistré, accord écrit, 90 jours (97), `Saved/Logs` renvoyé.
  - Effort : M

- **[Bloquant commercial] Multijoueur : LAN prêt, amis non**
  - Constat : package de 1,89 Go, port 7777 et 2e PC existent. Mais ni « Rejoindre » ni gestionnaire d'échec réseau : un ami ne se connecte que par console. n°2, n°3, RUN03 : non testés.
  - Preuve : `v1-rec-investisseurs.md:53`, journaux (port 7777), grep `Rejoindre|PreLogin|NetworkFailure` : 0.
  - Recommandation : LAN hôte + client sur le package T-124 cette semaine (58). Portes : G1 build versionné (`ProjectVersion`, SHA affiché). G2 joignable : champ IP, puis relais Steam L7a (56) ou ZeroTier/Tailscale en pont (supposé). G3 manche qui s'achève (T-112 à T-114). G4 smoke et `nettest` (58) verts.
  - Effort : M

- **[Majeur] Métriques non mesurables**
  - Constat : chute et noyade appellent `ApplyHealthDamage(…, nullptr)` : « killer: None ». Rien ne journalise abandon ni issue : `EExpeditionResolution` = `None` ou `Submerged`. Deux manches de T-122 continuent 3,5 min sans verdict. Durée nominale : 120 s (code), 300 s (DA), 540 s (cahier v1.4), donc n°13–16 échoueraient sur le build.
  - Preuve : `IslanderHealthComponent.cpp:74,113`, `IslanderMovementComponent.cpp:214`, `ExpeditionTypes.h:24-30`, `ExpeditionRules.h:25`, journaux « flood at 300.00 s ».
  - Recommandation : `EDeathCause`, trois lignes `[PLAYTEST]` (début, mort, fin avec motif), `scripts/playtest_digest.py` lisant aussi `ExpeditionGameState.cpp:114,260,341`. Durée Preparing → Resolved (cible 11–13 min, 01). Survie = vivants à bord / équipage. Quota atteint = ventes brutes ≥ quota (cibles 07 : 90 % M1, 50 % M5). Abandon = session sans `Resolved`. Premier butin < 6 min (30).
  - Effort : M

- **[Majeur] Aucun smoke test de build**
  - Constat : la validation de package contrôle menu, carte, début d'expédition. Le journal T-124 contient un ensure (« Leader pose … BeanOutfit ») et 2 234 avertissements `hand_r`, contre 0 erreur en T-120 et T-122 : régression non vue.
  - Preuve : `build/T-124/game-validation.log:1110-1113`.
  - Recommandation : `scripts/smoke.ps1` (≤ 5 min) : exe, voyage automatique, 90 s, puis hôte + client 127.0.0.1. Échec sur `Error:|ensure|Fatal`, avertissements hors budget, L_Boot → horloge > 25 s (17–20 s mesurées), `SeaRise frame times` hors seuil (`-novsync`).
  - Effort : S

- **[Majeur] Recette sans matrice ni pyramide**
  - Constat : §12 : 21 critères, §23 : 17, RUN : 14. Classification (supposé) : 12 en spec, 14 à deux processus, 22 sur build, 4 hors périmètre (ED). Sermon et arrimage (T-054, T-055) restent hors `main` : n°5, 6, 18, 20, UI11 non exécutables. Tests : 296 → 326 en texte libre, un run crashé au 215e (exit 3), 8 crashs en 25 h.
  - Preuve : `git branch --no-merged`, `build/codex-demo/20261004-040506/demo-summary.json`, `Saved/Crashes`.
  - Recommandation : `docs/production/recette.csv` (niveau, build, preuve). Fusionner T-054/055 avant tout playtest du §6. Porte INT = `monke.py report --min-tests`. Pyramide de 36, prolongée par smoke et playtests.
  - Effort : S

- **[Majeur] Plan de playtests sur 3 mois**
  - Constat : lots sans dates de test.
  - Preuve : cahier §11, §15.
  - Recommandation (dates glissantes si une porte reste fermée) :
    - 5–18 oct. : smoke, digest, `EDeathCause`, LAN 2 PC, cercle A.
    - 19 oct.–15 nov. : B1 « île seule » (build courant), puis B2 après T-112 à T-115. Sortie : 4 novices sur 5 au premier butin en < 6 min.
    - 16 nov.–6 déc. : C, G1–G4, RUN13, soak 60 min, profils réseau (58). Sortie : 0 crash, digests égaux.
    - 7 déc.–3 janv. : D, 2 cycles de 7 jours, télémétrie (95). Sortie : SUS ≥ 68, réussite M1/M5 (07), crashs/h, go/no-go démo ou accès anticipé (84, 89).
  - Effort : L

## Manques
1. `docs/playtests/`, consentement, formulaire anonyme.
2. `smoke.ps1`, `playtest_digest.py`, `recette.csv`.
3. Bilan de fin de manche, « Rejoindre par IP », `EDeathCause`.

## Quick wins
1. Script de session et questionnaires (0,5 j).
2. `playtest_digest.py` (1 j).
3. Test LAN hôte + client sur `build/rec-T124` (0,5 j).
4. `smoke.ps1` (1 j).
5. Corriger l'ensure BeanOutfit (≤ 1 j).

## Questions qui challengent l'équipe
1. Qui sont les trois premiers testeurs non développeurs, et quand ?
2. 120, 300 ou 540 s : quelle durée le playtest doit-il valider ?
3. Quel consentement pour enregistrer la voix d'amis ?
