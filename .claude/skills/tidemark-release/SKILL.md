---
name: tidemark-release
description: Checklist de livraison et de publication du plugin Tidemark — documentation, CHANGELOG, EULA et textes de licences, PROVENANCE.json, mention non désactivable, vérifications de contenu interdit (Maxar, DAG, Google, toponymes habités), extraction dans un dépôt propre, package de la map de démo, script vidéo de 30 s, page Fab, test d'installation externe, lancement et suivi J+30. À utiliser pour « prépare la release Tidemark », « page Fab », « on publie ».
argument-hint: "[checklist | extract | video | fab-page | launch]"
---

# Livraison Tidemark

Arguments : `$ARGUMENTS`. Rôle : `tidemark-release-engineer`. Cahier §7–§9, §10 S7–S8, §11. Conventions §7.

## `checklist` (rien ne part sans tout cocher)
**Code et tests**
- [ ] CI verte : build, `Tidemark.*` (dont `Determinism`, `Replay`, `Lint.VerifiedByReplay`), greps (zéro include du jeu, zéro hasard non seedé, crédits).
- [ ] Porte S4 : `gate-S4.md` verdict vert, `testé multi-machines`.
- [ ] Package Development avec la map de démo : 60 FPS médian 1080p pendant la montée (`perf-S6.md`).
- [ ] Version `.uplugin` et `CHANGELOG` cohérents ; `EngineVersion` 5.8 ; `Tidemark.uplugin` sans dépendance à un autre plugin.

**Contenu et licences**
- [ ] `grep -rni "maxar\|digitalglobe\|cocoteraie\|DAG\|3D Tiles\|google" Plugins/Tidemark/` → vide (hors `Docs/LICENSES` si une mention explicative est nécessaire).
- [ ] Aucun toponyme habité réel dans `Content/` ni dans la doc ; l'île de démo est « inspired by an atoll », côte Te Fenua créditée (Etalab 2.0, producteur, date).
- [ ] `PROVENANCE.json` : chaque asset de `Content/` (outil, date, licence, SHA-256), y compris ceux produits par des agents.
- [ ] `Docs/LICENSES/` : Epic EULA relue, CC0 / OFL / Etalab selon les assets ; `credits[]` de tout `.tmk` livré.
- [ ] Mention non désactivable présente dans : panneau de lint, JSON de lint, README, page Fab.
- [ ] EULA du plugin (licence standard Fab, ou la tienne) relue ; dossier pour le juriste (questions, pièces) prêt ; **une heure de juriste faite** avant la première vente.
- [ ] Nom vérifié (INPI, EUIPO, Fab) ; aucune mention « Tuamotu », « Raraka », « Motutapu » dans la communication.

**Utilisabilité**
- [ ] QUICKSTART : un dev externe installe et obtient un lint en < 15 min, deux testeurs chronométrés (`onboarding-test.md`).
- [ ] Projet d'exemple ouvert sans erreur sur un UE 5.8 vierge.

## `extract`
Nouveau dépôt Git (`tidemark/`) : copie propre de `Plugins/Tidemark/` (ou `git filter-repo` si l'historique vaut d'être gardé, décision `tidemark-lead`), CI reprise, LFS pour `Content/`. Tapao consomme ensuite une copie versionnée (`Plugins/Tidemark` = tag `vX.Y.Z`) ; `/ue-vcs` pour les conventions. Aucun secret, aucune donnée du jeu dans le nouveau dépôt.

## `video`
`docs/produit/tidemark/video-script.md` : les 4 plans du cahier §7 avec durées, caméra, texte à l'écran, et la liste des captures/`CaptureViewport` à produire. Hôte et client côte à côte avec hashes affichés ; curseur en ouverture ; pont rehaussé en fermeture. Montage sur la machine du propriétaire.

## `fab-page`
`docs/produit/tidemark/fab-page.md` : titre, trois premières lignes (API `TimeToFlood`/`FloodOrder` ; même marée sur toutes les machines ; lint), fonctionnalités, ce que ce n'est pas (pas une prévision, pas de données réelles), prérequis, prix (89 $ ⚠ estimé, décision du propriétaire), tags, captures, lien doc. Mention non désactivable.

## `launch`
Publication Fab ; posts r/unrealengine et Discord UE (texte court, vidéo, lien) ; `launch.md` avec un tableau hebdomadaire (ventes, retours, bugs, questions récurrentes) pendant 30 jours, pour la décision J+30 (`tidemark-lead`, TM-31).
