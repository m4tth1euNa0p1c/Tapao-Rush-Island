---
name: tidemark-netcheck
description: Recette multijoueur du plugin Tidemark — hôte et client sur deux PC réels (ou PIE hôte + client en répétition), trois scénarios de 10 minutes avec éditions côté client, comparaison des hashes par bloc, injection d'une divergence et mesure du temps de correction, bande passante. Produit le rapport de la porte S4. À utiliser pour « porte S4 », « teste le multi Tidemark », « ça désync ».
argument-hint: "[pie | two-pc --host <ip> | inject <cell> <cm> | report]"
---

# Recette réseau Tidemark

Arguments : `$ARGUMENTS`. Rôles : `tidemark-network-engineer`, `tidemark-qa`. Cahier §10 S4. Outils existants : `scripts/editor/mp_pie_check.py` (actions `cmd`, `launch`, `dormancy`), `scripts/editor/pie_session.py`.

## `pie` (répétition, ne vaut pas la porte)
1. Map de test avec `UTidemarkTerrainAsset` et échéancier `Surge2m90s`. PIE « Play as Listen Server », 2 joueurs, `net.PktLag=120 net.PktLoss=2` pour un réseau dégradé.
2. Toutes les 30 s : `tidemark.Hash` sur l'hôte et le client (`LogTidemarkNet` les journalise aussi à 1 Hz). 20 éditions côté client (`tidemark.Edit Dig <x> <y> 50`), validées par le serveur.
3. 10 min ; 0 écart attendu. Un écart → `/tidemark-determinism diff` sur les JSON de blocs.

## `two-pc --host <ip>` (la porte)
1. Package Development (`/ue-package`) installé sur les deux PC, même binaire, même version du plugin (hash du `.uplugin` et des DLL notés).
2. Hôte : `monke_island.exe <Map>?listen -log`. Client : `monke_island.exe <ip> -log`. Les logs sont copiés dans `build/netcheck/<date>/` avec leur SHA-256, comme la recette du 2026-10-11.
3. Trois scénarios × 10 min : (a) montée simple, aucune édition ; (b) 50 éditions client réparties ; (c) client qui rejoint à t = 3 min (rattrapage ou instantané).
4. Relevés : hashes hôte/client à 1 Hz (0 écart), bande passante (`stat net` : journal ≤ 1 ko/s, hash 16 o/s), temps de rattrapage du scénario (c).
5. `inject` (ci-dessous) dans le scénario (b) : correction < 2 s.

## `inject <cell> <cm>`
Console client `tidemark.InjectDivergence <x> <y> <cm>` : modifie une profondeur localement. Attendu : `LogTidemarkNet` montre 3 hashes en écart, puis l'instantané (taille en octets journalisée), puis hash égal ; temps total < 2 s.

## `report`
`docs/produit/tidemark/gate-S4.md` : machines (OS, GPU, version), binaire (hash), commandes exactes, pour chaque scénario les écarts (0 ou liste de blocs), bande passante, temps de correction, chemins des logs, captures côte à côte avec les hashes affichés. **Verdict binaire.** Niveau de preuve `testé multi-machines` seulement pour `two-pc` ; `pie` donne `testé éditeur`.

## Règles
- On ne « règle » jamais une désync en élargissant un seuil ou en envoyant la grille plus souvent : on trouve le bloc, puis la cause (`/tidemark-determinism audit`).
- Deux fenêtres PIE sur une machine ne prouvent rien contre les différences de binaire ou de pilote : la porte, c'est deux PC.
