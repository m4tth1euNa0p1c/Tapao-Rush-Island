# 70 — Licences d'assets et provenance
Domaine : G · Auditeur : juriste assets

## Verdict
Le socle CC0, les polices OFL et les plugins sont sains. Mais « tout Te Fenua est Etalab » est faux : les cocoteraies excluent l'usage commercial et des dérivés de l'imagerie Maxar sont embarqués. Risque : retrait ou réclamation visant le terrain réel de Raraka, qui pilote végétation, relief et masques.

## Constats

- **[Bloquant commercial] Cocoteraies DAG : « recherche/éducation », pas Etalab**
  - Constat : la fiche ArcGIS limite l'usage à la recherche et à l'éducatif, « pas d'utilisation prévue pour des opérations commerciales » (ambigu, sans licence commerciale). Nos docs la disent Etalab. Elle pilote au moins 22 431 des 43 057 cocotiers livrés.
  - Preuve : https://www.arcgis.com/sharing/rest/content/items/9ff5bedeb08640ae825356972c3297ce?f=json (`licenseInfo`, 2026-10-04), README raraka:16, `find_fragments.py:988`, 46 fichiers `DT_Raraka` dans `build/package`.
  - Recommandation : licence écrite de la DAG ; sinon cocoteraies procédurales (rivage Etalab + bruit) et purge de `cocoteraies.geojson`.
  - Effort : S (demande), M (remplacement)

- **[Majeur] Dérivés Maxar embarqués malgré ADR-004**
  - Constat : l'image n'est pas embarquée (ADR-004:30), mais ses dérivés oui : masque de végétation et hauts-fonds calculés sur la luminance de l'image © 2018 DigitalGlobe/Maxar alimentent `T_Raraka_TerrainMasks`, 51 tuiles de relief, jusqu'à ~348 000 fourrés, 20 fragments. Aucune licence ouverte trouvée (le WMTS annonce « AccessConstraints none », sans valeur de licence) ; la restriction DAG (photo-interprétation de ces images) suggère des conditions strictes (supposé).
  - Preuve : `build_terrain.py:238-284,374`, `place_vegetation.py:142-147`, `ortho_emprises.geojson`, fiche ArcGIS 00f2770b… sans licence.
  - Recommandation : demande écrite DAF/Te Fenua (dérivés, usage commercial), ou masques refaits sur Sentinel-2/Landsat (libres, à vérifier) ; sortir `raw_sat_z15.png` du dépôt avant tout remote.
  - Effort : M

- **[Majeur] Atlas IRD : raster sans licence, alternative Etalab ignorée**
  - Constat : les classes de relief lisent les couleurs de la tuile WMTS de l'atlas IRD 2005 (Andréfouët et al.), sans licence déclarée. Te Fenua publie une couche vectorielle Etalab 2.0 issue de l'atlas et couvrant Raraka (4 classes, plus grossière).
  - Preuve : `build_terrain.py:39-46,188`, https://www.arcgis.com/sharing/rest/content/items/e15a0ebdc2be45fb92d7ab1543a25f1b?f=json.
  - Recommandation : refaire `classify_atlas` sur cette couche ; créditer « Andréfouët et al., IRD 2005 ».
  - Effort : M

- **[Majeur] Crédits embarqués incomplets**
  - Constat : les 8 lignes omettent la date de mise à jour (exigée par Etalab), l'IRD, la DAG et les copyrights des polices. `DataCredits` n'est lue par aucune UI ; `NOTICES.txt` ne cite que FreeType ; textes OFL non livrés ; menu intitulé TAPAO. Obligatoires : Etalab (producteur + date, par couche), OFL (copyright + texte), IRD ; Epic à vérifier ; CC0 facultatif.
  - Preuve : chaînes de `DA_TapaoUISkin.uasset`, `ExpeditionIslandDefinition.cpp:66`, `DefaultGame.ini:3`, https://raw.githubusercontent.com/etalab/licence-ouverte/master/LO.md.
  - Recommandation : crédits générés d'un registre unique ; afficher `DataCredits` ; livrer `Licenses/` ; renseigner ProjectName, CopyrightNotice et titre.
  - Effort : S

- **[Majeur] Planches IA, aucun registre de provenance**
  - Constat : les 8 planches `sprite/*.png` (2 examinées) paraissent générées par IA (CAHIER §10) et fondent bean (façon Fall Guys) et tiki. Aucun registre (outil, date, droits) : protection du design incertaine, déclaration IA Steam probable (supposé, voir 98, 76).
  - Preuve : `sprite/player-1.png`, `sprite/mob-1.png`, CAHIER:212.
  - Recommandation : `docs/legal/PROVENANCE.md` (origine, outil, date, apport humain) ; avocat PI avant Steam.
  - Effort : M

- **[Mineur] Contenu Epic du template livré**
  - Constat : `BP_Islander` référence `SKM_Manny_Simple` et des assets Variant_Horror ; 42 fichiers Mannequins dans le build. EULA Unreal non relue (HTTP 403) : redevance et avis tiers à vérifier. Plugins : tous Epic, outils MCP limités à l'éditeur.
  - Preuve : `BP_Islander.uasset`, `Manifest_UFSFiles_Win64.txt`, `monke_island.uproject`.
  - Recommandation : retirer Manny et Variant_* du livré ; faire lire l'EULA.
  - Effort : S

- **[Mineur] CC0 cohérent, sans preuve archivée**
  - Constat : les ~60 jeux Poly Haven, ambientCG, Kenney, rubberduck concordent avec SOURCES.md (CC0 non revérifié en ligne) ; sons d'armes, mer, trompette, UI synthétisés. Aucune trace de Sketchfab, Poly Pizza, CC-BY ou 3D générative (50 `.blend` scannés). Mais API vivantes sans checksum, zips rubberduck sans licence ni suivi git, SOURCES.md muet sur Camp, Galion, Barque, kit TAPAO, polices.
  - Preuve : `fetch_atoll_textures.py`, `.gitignore` (`art/audio_src/`), zips : 80 `.ogg`, aucun texte.
  - Recommandation : `art/PROVENANCE.json` (URL, licence, date, SHA-256) généré par les fetchers ; archiver zips et pages.
  - Effort : S

## Manques
1. Licences écrites DAG, DAF (Maxar), IRD, ou plan B procédural.
2. Registre de provenance unique, preuves comprises.
3. `Licenses/` livré, EULA du jeu, mentions légales (voir 97), avocat PI (ce rapport n'est pas un avis).
4. Licences VoIP (CAHIER:185) et Steamworks au lot L7 ; musique (aucune aujourd'hui).

## Quick wins
1. Marquer `cocoteraies` « recherche/éducatif » (README, CLAUDE.md, `fragments.json`) ; geler tout nouvel usage.
2. Deux courriers : DAG (cocoteraies) ; DAF/Te Fenua (imagerie dérivée, atlas).
3. Stager `Licenses/` (OFL ×2, Etalab, Epic) ; compléter les crédits (dates, IRD).
4. Retirer Manny et Variant_* ; remplacer TAPAO ; renseigner CopyrightNotice.
5. Archiver zips rubberduck, pages de licence, SHA-256.

## Questions qui challengent l'équipe
1. Si DAG et DAF refusent l'usage commercial, livrez-vous un atoll procédural « inspiré » ? Le 1:1 vaut-il ce risque ?
2. Pouvez-vous prouver, fichier par fichier, la provenance de chaque asset livré (planches IA, scripts d'agents) devant Steam ?
3. Qui est l'éditeur juridique et détient les droits sur le code et les assets écrits par des agents ? Un avocat l'a-t-il confirmé ?

Web : 5 requêtes ; métadonnées publiques ArcGIS/WMTS Te Fenua.
