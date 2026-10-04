# Inventaire des licences — T-130

**Brouillon à faire relire par un avocat. Non publié. Vérification du 4 octobre 2026.**

Périmètre : sources courantes, polices locales, configuration et DLL présentes
dans `build/rec-T124/Windows`. Aucun nouveau package ni contrôle des uassets en
éditeur. Un fichier présent dans le dépôt n'est pas une preuve de présence dans
le dernier package ; une licence affichée aujourd'hui ne prouve pas la version
applicable au téléchargement historique.

## Moteur, plugins, polices et ressources artistiques

| Élément / producteur | Statut vérifié | Preuves et reste à obtenir |
|---|---|---|
| Unreal Engine 5.8, Epic Games, template et Starter Content | Licence propriétaire Unreal, pas CC0 | [EULA officielle](https://www.unrealengine.com/eula/unreal), installation locale. Version acceptée par le titulaire, droits de distribution, avis et obligations financières à valider par le conseil ; aucun taux simplifié repris des audits |
| StateTree, GameplayStateTree, ProceduralMeshComponent | Plugins de l'installation Epic activés dans le .uproject | Version du moteur et reçu de build final à archiver ; contrôler les dépendances tierces |
| ModelingToolsEditorMode et outils MCP | `TargetAllowList=[Editor]` déjà présent pour ModelContextProtocol et les Toolsets | Configuration vérifiée, correction T-120 existante. Ne pas conclure à leur absence du package sans lire son reçu/manifeste |
| Pirata One — Rodrigo Fuenzalida, Nicolas Massi | OFL 1.1, copyright 2012 ; nom réservé Pirata | [Texte exact local](Licenses/OFL_PirataOne.txt), présent avec la source ; police importée Unreal, modification de dessin non attestée. Vérifier présence du texte dans le package |
| Alegreya SC — The Alegreya Project Authors | OFL 1.1, copyright 2011 | [Texte exact local](Licenses/OFL_AlegreyaSC.txt) ; mêmes limites de staging. [Texte OFL officiel](https://openfontlicense.org/open-font-license-official-text/) |
| Poly Haven, textures, HDRI et modèles de art/SOURCES.md | Politique publique CC0 pour les assets | [Licence](https://polyhaven.com/license), URLs individuelles dans SOURCES. Dates de téléchargement, auteurs par asset, SHA-256 et copie historique de la licence encore manquants |
| ambientCG, textures dont Fabric034 | Politique publique CC0 | [Licence](https://docs.ambientcg.com/license/), références individuelles dans SOURCES ; mêmes preuves historiques manquantes |
| Kenney Impact Sounds, rubberduck | CC0 déclaré par les scripts et SOURCES ; pas validé fichier par fichier | Archives d'origine, page de chaque lot, licence jointe et sommes manquantes ; ne pas assimiler tous les sons à une unique preuve CC0 |
| Modèles procéduraux barque/galion/camps, kit TAPAO, sons synthétisés, planches sprite | Production interne déclarée ; aucune licence tierce générale inférée | Auteurs, outils, dates, apports humains, conditions d'utilisation IA et éventuelles cessions à documenter. « Synthétisé » ne signifie pas absence de droits |

## Données Te Fenua : une licence par couche

Les huit réponses JSON intégrales sont conservées avec URL, horodatage de lecture
et SHA-256 dans [le manifeste](evidence/2026-10-04/manifest.json).
Les dates ci-dessous sont les **dates de modification des fiches ArcGIS**, pas
des dates de mise à jour garanties des données livrées. Celles-ci restent à
demander au producteur pour les crédits définitifs.

| Couche / producteur indiqué dans le dépôt | Licence affichée dans la fiche consultée | Fiche modifiée (UTC) / preuve locale | Transformation et statut commercial |
|---|---|---|---|
| Parcelles — DAF / Te Fenua | Etalab 2.0 | 2026-05-19 ; [JSON](evidence/2026-10-04/parcelles.json) | Conversion coordonnées/index et calque de référence ; rattacher au téléchargement utilisé et vérifier exclusion des données inutiles du package |
| Rivage — DAF / Te Fenua | Etalab 2.0 | 2026-02-28 ; [JSON](evidence/2026-10-04/rivage.json) | Découpe terrain, fragments, placements ; attribution par source et date effective à finaliser |
| Bâtiments BD TOPO — Te Fenua | Lien explicite Licence Ouverte Etalab 2.0 | 2024-10-28 ; [JSON](evidence/2026-10-04/batiments.json) | Emprises utilisées pour exclusions ; producteur précis et date des données à confirmer |
| Occupation du sol BD TOPO — Te Fenua | Lien explicite Etalab 2.0 | 2024-10-28 ; [JSON](evidence/2026-10-04/occupation_sol.json) | Couche téléchargée ; dépendances finales et transformations à tracer |
| Toponymes — Te Fenua | Lien explicite Etalab 2.0 | 2022-10-27 ; [JSON](evidence/2026-10-04/toponymes.json) | Références et textes ; ne pas déduire une caution du producteur |
| Cocoteraies — DAG, inventaire 2023 | Recherche/éducation ; aucun usage commercial prévu dans la fiche | 2025-05-07 ; [JSON](evidence/2026-10-04/cocoteraies.json) | `place_palms.py` lit toujours les polygones/attributs. **Autorisation commerciale non établie** ; accord écrit DAG nécessaire ou remplacement indépendant. L'étiquette Etalab du README existant est erronée pour cette couche |
| Emprises orthomosaïques — Te Fenua | `licenseInfo` absent | 2024-10-28 ; [JSON](evidence/2026-10-04/ortho_emprises.json) | L'index des prises de vue n'accorde aucun droit sur les pixels satellite |
| Délimitations vectorielles des récifs — DSI / Te Fenua | Lien explicite Etalab 2.0 | 2023-07-07 ; [JSON](evidence/2026-10-04/recifs_vectoriels.json) | Alternative candidate ; **ne remplace pas actuellement le raster** lu par le pipeline. Couverture, filiation et licence de la version employée à confirmer |

[Licence Ouverte officielle](https://www.etalab.gouv.fr/licence-ouverte-open-licence/) :
préparer attribution, source et date des données, sans laisser croire à une
validation des transformations par le producteur. La page générale DAF citée
par l'audit 73 n'a pas pu être relue via l'outil web ; aucune généralisation CC BY
ou Etalab n'est faite à partir d'elle.

## IRD et dérivés Maxar

**Raster IRD 2005** : Andréfouët et al., *Atlas des récifs coralliens de Polynésie
française*, via WMTS Te Fenua. `build_terrain.py` et `place_vegetation.py` lisent
encore `atlas_recif_z15.png` et classent ses couleurs. Aucune licence du raster
source n'a été retrouvée dans les pièces locales examinées. La licence du
vecteur ci-dessus ne s'étend pas automatiquement à ce raster. Obtenir une preuve
du droit de redistribution/transformation ou remplacer la source.

**Imagerie © DigitalGlobe/Maxar, orthophotographie 2018 via Te Fenua** :
`build_terrain.py` lit toujours `raw_sat_z15.png` ; luminance/contraste alimentent
hauts-fonds et masques, puis terrain et végétation. Les commentaires « référence
uniquement » et l'absence des pixels bruts dans le jeu ne prouvent pas le droit
sur ces dérivés. Aucun accord commercial trouvé : demander à DAF/Te Fenua la
chaîne de droits Maxar, la permission sur les dérivés et les obligations de
crédit. Cette tâche ne change ni ne régénère ces données.

## DLL réellement observées et notices

Le package T-124 contient seulement une courte mention FreeType dans NOTICES.txt.
Les copies dans [Licenses/](Licenses/README.md) constituent un dossier documentaire,
**pas un staging dans le package**, et ne prouvent pas leur adéquation de version.

| DLL ou famille observée | Texte local candidat / statut |
|---|---|
| libogg_64, libvorbis_64, libvorbisfile_64 | Copies libOgg 1.3.5 et libvorbis 1.3.7 ; rattacher versions/avis au reçu |
| msquic | MsQuic_License.txt copié ; vérifier version et dépendances |
| tbb12, tbbmalloc | oneTBB_LICENSE.txt copié ; vérifier notices associées |
| onnxruntime | Onnxruntime_v1.24.1.license copié ; plusieurs versions présentes dans UE, correspondance au binaire non prouvée |
| DirectML | DirectML_v1.15.4.license copié ; licence Microsoft et version distribuée à vérifier |
| GFSDK_Aftermath_Lib.x64 | Aftermath_NVAPI_License.txt copié, candidat seulement ; autre SDK récent également présent dans UE |
| VkLayer_api_dump, crash_diagnostic, gfxreconstruct, khronos_profiles, shader_object, synchronization2, validation | Vulkan_License.txt copié ; inventorier chaque couche, sa version, licence et notices spécifiques |
| WinPixEventRuntime, xaudio2_9redist, dbghelp, D3D12Core, d3d12SDKLayers | Notices et droits de redistribution Microsoft à rattacher ; aucun texte générique substitué |
| FreeType (notice du package, pas de DLL séparée observée) | Freetype2.14.1_License.TXT copié ; prouver version/options réellement liées |

Le manifeste des textes donne provenance locale et SHA-256. Il reste à établir
un manifeste package ↔ binaires ↔ notices exactes, puis à vérifier le dossier
Licenses dans le package de recette. Rien ici ne certifie une conformité commerciale.
