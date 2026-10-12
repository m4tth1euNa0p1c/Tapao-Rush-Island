# 49 — Budgets de contenu par catégorie
Domaine : E · Auditeur : tech artist

## Verdict
Seuls trois budgets de contenu sont chiffrés (perso, texture de prop, horizon), sans machine de référence ni contrôle automatique : plusieurs catégories les dépassent. La végétation Nanite (33,4 G de triangles source, 73 % pour le Naupaka), la foule de PNJ et les collisions par instance menacent les 60 FPS, sans mesure en build. Risque commercial : exclure le PC Steam médian.

## Constats
- **[Majeur] Budgets non chiffrés ni contrôlés**
  - Constat : 22 JSON hétérogènes, 3 seulement donnent les triangles (`ue_manifest.json` : `verts`) ; « machine de référence » jamais définie. Mon scan de 320 FBX recoupe les docs (minka 37 746).
  - Preuve : performance.md:33,75 ; CAHIER:268.
  - Recommandation : table ci-dessous (à valider au L0) ; `scripts/check_budgets.py` en échec, modèle : horizon (fragments-tuamotu.md:270).
  - Effort : S

| Poste | Cible | Mesuré |
|---|---|---|
| Bean habillé | ≤ 40 k tris | 76,6 k |
| PNJ de foule | ≤ 12 k, ≤ 3 mat., 3 LOD | 35–52 k, 7–20 mat., sans LOD (supposé) |
| Arbuste / cocotier Nanite | ≤ 20 k / ≤ 50 k | Naupaka 88 k, Tahinu 170 k / 72–98 k |
| Végétation par fragment | ≤ 0,6 G tris | 0,4–2,8 G |
| Maison | ≤ 25 k, ≤ 8 mat., ≤ 15 formes | 37,7 k, 36 mat., 67 UCX |
| Texture de prop | ≤ 1K, jamais NPOT | 2K, 2048×4096, 13 NPOT |
| Audio | ≤ 120 Mo, ≤ 64 voix | 15,1 Mo ✓ |

- **[Majeur] Végétation : le Naupaka pèse 73 % des triangles**
  - Constat : 3 Naupaka (85–91 k tris, 278 619 instances) totalisent 24,4 G sur 33,4 G ; cocotiers adultes 4,2 G. Un fragment : 15–62 k instances, moyenne 1,18 G. Vent (WPO) sur Nanite : « mesure GPU à faire » (T-062), jamais faite.
  - Preuve : scan FBX `art/export/Vegetation`, `vegetation_ue.json`, `palms_ue.json`.
  - Recommandation : Naupaka ≤ 20 k (fragment moyen 0,51 G, −56 %) ; mesurer FRG_20 avant/après.
  - Effort : M

- **[Majeur] Collisions par instance : 183 k corps, ~428 k convexes**
  - Constat : chaque instance « Hard » a sa collision (4–8 UCX par cocotier) ; un fragment en touche 4,4–11,5 k, soit ~97 % chargés pour rien (carte monolithique). Maisons : 67 UCX, 3 264–5 027 par fragment. Coût au chargement non mesuré (supposé élevé).
  - Preuve : README raraka:151-153, `VillageSpawner.cpp:326`.
  - Recommandation : une capsule `UCP_` par tronc, cocos au sol sans collision, maison en ≤ 15 `UBX_`.
  - Effort : M

- **[Majeur] Personnages hors budget §10, sans LOD**
  - Constat : tiki 35–52 k tris, 7–20 emplacements (villageois 20 ; la doc dit 8–14), 67 `M_*` non instanciés, sans URO (grep), LOD absents (supposé) ; un fragment aligne 36–66 villageois (≈ 3 M tris, > 1 000 sections) plus 80 cadavres. Bean : 14,2 + 50,0 + 12,4 = 76,6 k (cahier : 30–60 k), torche 21,6 k.
  - Preuve : scan FBX, characters-tiki.md:28, villages-japonais.md:146, CAHIER:220.
  - Recommandation : foule ≤ 12 k avec LOD, 1 master + MI, atlas 1K, URO ; habit du bean ≤ 20 k.
  - Effort : L

- **[Majeur] Village : 36 matériaux sur un mesh, textures NPOT et 8×8**
  - Constat : `SM_Minka` 37,7 k tris, 36 slots, intérieur meublé ; 43–66 maisons par fragment, 1,8–2,7 M tris. Sur 120 textures, 53 font 8×8 ; 13 sont NPOT, dont le chaume 1024×922 : sans mips ni streaming si « Power of Two » = None (supposé).
  - Preuve : `village_kit.json`, `ue_materials.json`, scan PNG.
  - Recommandation : ≤ 8 matériaux (atlas), couleurs unies en paramètres, 13 textures en puissance de 2.
  - Effort : M

- **[Majeur] Textures : les plus lourdes pour les plus petits objets**
  - Constat : totems de 55 cm = 9 textures 2048² (41 Mo) ; butin de 10–20 cm = 27 textures de 1024 px (43 Mo) ; corail 2048² (21 Mo) ; troncs 2048×4096 sans ADR. VRAM estimée 244 Mo (acceptable), mais 517 Mo des 1,2 Go de Content sont des textures.
  - Preuve : scan PNG, memory.md:50.
  - Recommandation : ~512 px/m : totems 512, butin 256–512, corail 1K (≈ −85 Mo).
  - Effort : S

## Manques
- Machine de référence, configuration minimale, rapport `docs/perf/` (absent).
- Paliers de qualité (pas de `DefaultScalability.ini`, densité PCG non pilotée).
- LOD (camps 46,5 k tris, galion 77 k, aucun LOD généré : supposé) ; musique (aucune), ambiances de jour, voix.

## Quick wins
1. `scripts/check_budgets.py` (FBX, PNG, manifestes).
2. Ré-échantillonner totems, butin, corail, 13 NPOT.
3. Ombres des lampes de maison (66 par fragment, portée 90 m : `VillageRules.h:161`) coupées au-delà de 25 m, cible ≤ 8 ; `AudioMaxChannels` à 64 (0 = défaut).
4. Nanite `KeepPercentTriangles` 25 % sur le Naupaka, mesurer FRG_20.
5. Vérifier si les 48 `DT_*` (173 Mo, génération PCG) peuvent sortir du cook.

## Questions qui challengent l'équipe
1. Quelle machine de référence fixe ces budgets, et coupez-vous 56 % des triangles de végétation si c'est une carte de 2018 ?
2. Pourquoi charger 816 000 instances pour en jouer ~28 000 (3,5 %) ?
3. Qui refuse un asset du 2e PC hors budget (Naupaka 90 k, totem 2K) ?
