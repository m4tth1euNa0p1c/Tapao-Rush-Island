Les quatre tâches sont en **review**.

- **T-128**, `dev/T-128`, `6c12f05` : mémoire configurable des îles réellement jouées, tirage déterministe, petits catalogues, six specs.
- **T-129**, `dev/T-129`, `eac5a42` (inclut `31b747a`, `a018676`) : causes répliquées, journaux enrichis, hash des règles, export CSV et résumé ; cinq specs.
- **T-130**, main, `33e1740` : brouillons légaux, crédits, inventaire, huit fiches ArcGIS et onze textes de licence archivés.
- **T-127**, `dev/T-127`, `29d8239` : quatre joueurs, version journalisée, configuration Crash Reporter, scripts d’archivage PDB ; MCP déjà réservé à l’éditeur.

**Preuves :** 15/15 tests Python ; syntaxe PowerShell valide ; 19 SHA-256 vérifiés. UHT passe, mais les builds jeu échouent sur des dépendances préexistantes manquantes. Onze specs Unreal non exécutées.

**Reste :** intégration, compilation éditeur, Automation, réseau, package et archivage réel des PDB. Éditeur tiers préservé. Risques : conflits GameState/joueur/configuration ; droits DAG, Maxar et raster IRD non établis. Relecture juridique indispensable. Aucune publication.
