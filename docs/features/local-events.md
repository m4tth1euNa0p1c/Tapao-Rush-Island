# Journaux locaux d'expédition — T-129

Rapports 28 et 95 relus en entier. Contrat avant intégration :

- `EDeathCause` : Unknown, Fall, Drowning, Coconut, Tiki, Shark. Le coup fatal
  serveur fixe la cause répliquée avant notification ; une remise en vie l'efface.
  Les appels C++ existants restent valides grâce au paramètre optionnel. Recompiler
  les Blueprints appelant ApplyHealthDamage lors de la gate éditeur.
- Les types de dégâts tiki/requin vivent dans Core, sans dépendance de la santé
  aux classes concrètes des ennemis. Aucun montant de dégâts modifié.
- Mort : `Expedition <guid>: died schema=1 cause=<token> t=<secondes écoulées>
  phase=<phase> alive=<vivants après la mort> entity=player|creature`.
  Sans expédition active : identifiant nul, temps zéro. Pas de nom d'acteur/joueur.
- Début : ligne existante enrichie de `schema=1 build=<ProjectVersion>
  rules_hash=<32 hex>`. Hash MD5 local des propriétés éditables effectives du DA,
  triées, sans identité UObject ; les floats sont représentés par leurs bits.
  Il identifie des réglages, sans constituer une signature de sécurité.
- `scripts/log_events.py` lit les `.log` et écrit CSV + résumé JSON sous
  `Saved/Telemetry`. Aucune connexion réseau. `--logs`, `--csv`, `--summary`
  permettent une destination explicite. Il ne conserve que des champs validés,
  exclut User/Computer, chemins, noms d'acteurs et lignes inconnues.
- La durée est début → dernière fin observée (submersion/équipage perdu), pas la
  durée de session. Sans fin, la manche reste incomplète. Les anciennes morts
  restent « inconnues/non classées », hors compteur des morts joueurs par cause.
  Les copies de lignes horodatées entre logs sont dédupliquées, en préservant
  plusieurs morts simultanées dans un même fichier.

SEC-01 : résolution serveur ; SEC-09 : export en liste blanche ; ARC-01 : types
communs dans Core ; MEM-05 : travail seulement au début et à la mort ; ERR-09 :
specs de hash et tokens, tests Python de parsing, durées et confidentialité.

Les logs Shipping restent désactivés par le moteur. Ce lot ne livre ni collecte
distante, ni consentement à un envoi, ni écran spectateur. T-127 fournit le script
de version. À l'intégration, conserver la parade et MaxHealth répliqué déjà
modifiés dans main, ainsi que les changements nage et requin des autres sessions.

Validation du 2026-10-04 : 7/7 tests Python réussis ; export des logs existants :
8 expéditions sans fin observée, 2 morts anciennes non classables. Cet export ne
valide pas le nouveau C++. Cinq Automation Specs écrites, non exécutées.
Compilation en attente : UBT T-142 occupe la machine ; base 383d923 déjà connue
incomplète par le build T-128. Aucun PIE ni éditeur arrêté par cette tâche.

Après libération de T-142 : build cible jeu tenté deux fois. Deux diagnostics
C4456 de la nouvelle spec corrigés ; seconde compilation sans diagnostic sur
T-129, mais échec global sur les trois dépendances de base déjà relevées T-128
(SetForceThirdPerson, bExhaustionKillsInWater, EAnchorageRole::Galleon).
UHT passe. Preuve : code écrit, tests Python 7/7 ; C++ non validé en build complet,
non testé headless/éditeur/réseau. Galleon a depuis été commité sur main (eea515e),
les branches T-128/T-129 restent sur leur base 383d923 pour une intégration ciblée.
