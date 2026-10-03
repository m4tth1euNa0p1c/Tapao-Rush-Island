---
name: ue-pcg-scatter
description: Disperse végétation et props sur l'atoll avec PCG via le MCP (PCGToolset) — cocoteraies, arbustes de lisière, débris coralliens — pilotés par les masques du pipeline carto, avec densités et exclusions réalistes.
argument-hint: "<quoi disperser> <où>"
---

# Dispersion PCG

Demande : $ARGUMENTS

1. `describe_toolset` sur `PCGToolset.PCGToolset` et `PCGToolset.PCGSpatialToolset` (création de graphes, nœuds, paramètres).
2. Données d'entrée :
   - surfaces : tuiles de terrain (`/Game/MonkeIsland/Map/<Ile>/Meshes`) ;
   - masques : texture de masques du terrain (végétation, sable, platier) ou volumes ;
   - référence réelle : `cocoteraies.geojson` (zones plantées), image satellite (densité).
3. Graphe `PCG_<Nom>` dans `/Game/MonkeIsland/Environment/PCG/` :
   - échantillonnage de surface → filtre par masque / pente / altitude (pas de cocotier sous +0,5 m) ;
   - densité et espacement minimal réalistes (cocoteraie de plantation : alignements ≈ 8–9 m ; naturelle : irrégulière) ;
   - exclusions : bâtiments, chemins, plages actives (bande de 5–15 m depuis l'eau) ;
   - variations : échelle, rotation, inclinaison des cocotiers vers le lagon (alizés).
4. Composant PCG dans le niveau, génération, puis captures joueur + aérienne.
5. Perf : nombre d'instances et coût avec `ue-performance-engineer` sur la zone la plus dense.

## Règles
- Les meshes dispersés sont Nanite quand c'est possible.
- Seeds fixées pour un résultat reproductible.
