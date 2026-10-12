# 42 — RAM et VRAM
Domaine : E · Auditeur : ingénieur mémoire

## Verdict
Aucune mesure mémoire n'existe : les journaux viennent d'un PC à 31,7 Go de RAM et 16 Go de VRAM, en 1280×720, tout en Epic. Chaque partie charge l'atoll entier pour jouer environ 4 % de la terre. J'estime 4 à 5 Go de RAM et 4,5 à 6 Go de VRAM (supposé) : risque de paging ou de crash sur un PC 8 Go / 6 Go.

## Constats

- **[Bloquant commercial] L'atoll entier est chargé pour jouer environ 4 % de la terre**
  - Constat : les 20 `DA_Island_*` pointent `Lvl_Raraka`. FRG_Raraka_01 n'a que 36 766 instances (4,5 %) et 1,63 M triangles de terrain, contre ~816 k et 14,9 M.
  - Preuve : fragments-tuamotu.md:363,391; board T-089 (en cours, FRG_01 seul), T-091; umap 110 060 907 o ÷ 815 993 instances; 53 tuiles OBJ = 14 918 784 triangles; `fragments/FRG_Raraka_01/*.json`.
  - Recommandation : finir T-089 pour les 20 fragments (ADR-004), puis retirer `Lvl_Raraka` du cook. Gain estimé : −0,8 à −1,5 Go de RAM (supposé).
  - Effort : L

- **[Majeur] Budgets incompatibles avec 8 Go / 6 Go, jamais mesurés, rendu Epic non borné**
  - Constat : memory.md autorise « processus ≤ 8 Go »; sur 8 Go, Windows en prend 3 à 4 : limite réelle ~4,5 Go (supposé). Postes RAM actuels en Go (supposé) : moteur et shaders ~2,2, collision 0,6–1,5, instances 0,2, contenu 0,8, gameplay 0,2. Aucun palier ni CVar de budget; Epic = VSM 4 096 pages (~256 Mo), atlas Lumen 4 096², TSR à 200 %.
  - Preuve : memory.md:46-47; DefaultEngine.ini:114; journal éditeur; GameUserSettings.ini (`sg.*=3`).
  - Recommandation : RAM ≤ 4,0 Go stable, pic 4,5. VRAM ≤ 4,6 Go à 1080p : textures 1,0, Nanite 0,5, Lumen 0,6, VSM 0,15, cibles de rendu 0,6, géométrie et skin cache 0,4, divers 0,3, marge 1,0. Palier « 6 Go » (VSM 2 048 pages, atlas Lumen 2 048, TSR 100 %) choisi selon la VRAM.
  - Effort : M

- **[Majeur] Collision : 14,9 M triangles de terrain et ~190 000 corps statiques**
  - Constat : tuiles en collision complexe : 0,4 à 1,2 Go selon l'arbre Chaos (25–80 o par triangle, supposé), plus ~0,4 Go si le fallback Nanite à 100 % survit au cook. Végétation dure : 147 048 + 43 057 palmiers = 190 105 corps, 0,2 à 0,3 Go (supposé). Dans les fragments, 48 % des triangles sont des tuiles entièrement sous −5 m.
  - Preuve : setup_fragment_map.py:210-213; raraka/README.md:31,100,136,151; `georef.json` des 20 fragments.
  - Recommandation : collision complexe seulement si le sommet de la tuile dépasse −5 m; fallback 5–10 %; collision de végétation activée par bloc PCG à moins de 150 m des joueurs.
  - Effort : M

- **[Majeur] Végétation : meshes très lourds, « dégagement » qui ne libère rien**
  - Constat : naupaka 73–79 k sommets (255 k instances), palmiers 62–84 k. Sous villages, camps et marae, les plantes sont déplacées de 1 km, pas supprimées : RAM, corps et GPU scene inchangés. Culling par distance non vérifié.
  - Preuve : art/export/ue_manifest.json (`verts`); board T-036; VillageSpawner.cpp:294-301; SiteClearing.cpp:49-50.
  - Recommandation : DataTables de fragment filtrées à la source; `InstanceEndCullDistance` par catégorie (fougères 40 m, props 60 m, naupaka 200 m); WPO coupé au loin; naupaka et tahinu refaits à ≤ 20 k sommets.
  - Effort : M

- **[Majeur] Cadavres et PNJ : un coût GPU et physique, pas de RAM**
  - Constat : 80 cadavres ~4 à 8 Mo (supposé). Le coût : 35–52 k triangles sans LOD vu, 8–14 matériaux par créature, ragdoll complet simulé et permanent, skin cache ~110 Mo pour ~160 persos et cadavres, plafond 128 Mo (supposé). `CorpseMaxCount` compte aussi moitiés et membres.
  - Preuve : TikiRules.h:154,178; TikiCorpse.cpp:354-374; TikiRagdollCorpse.cpp:153; characters-tiki.md:28.
  - Recommandation : LOD1/2 (12 k, 4 k), 2 matériaux par bake, cadavre figé après 10 s (physique coupée), 12 ragdolls actifs au plus, URO au-delà de 30 m.
  - Effort : M

- **[Mineur] 769 à 4 074 butins = autant d'acteurs physiques répliqués**
  - Constat : nombre semé selon l'arrivée (plafond 5 000). Chaque acteur ~8–10 Ko (supposé), soit 6 à 40 Mo : modeste en RAM, mais autant de corps et d'objets GC.
  - Preuve : journal éditeur (LootScatterSpawner); PhysicsPropActor.cpp:12-17; board T-108.
  - Recommandation : butin virtuel : positions côté serveur, acteurs créés à moins de 70 m des joueurs et recyclés (≤ 1 500 vivants).
  - Effort : M

## Manques
- Mesures réelles (`memreport`, LLM, `stat Nanite`) sur build packagé, PC 8 Go, carte 6 Go.
- Test de fuite sur 3 cycles galion ↔ île (memory.md:49).
- Désactivation à distance (PNJ, physique, lampes); `LoadSynchronous` en jeu.

## Quick wins
1. `memreport -full` sur FRG_01 packagé.
2. `setup_fragment_map.py` : plus de collision complexe sous −5 m.
3. `CorpseMaxCount` 80 → 40; `DA_LootRules.Count` ≤ 1 500.
4. Fixer dans `DefaultEngine.ini` pool Nanite 384 Mo et skin cache 192 Mo.

## Questions qui challengent l'équipe
1. Quel PC 8 Go / 6 Go avez-vous ? Sans lui, le minimum annoncé n'est pas vérifié.
2. `Lvl_Raraka` est-il une promesse ou un outil ? Le garder jouable coûte ~1 Go de RAM (supposé).
3. 4 000 objets au sol, 80 cadavres permanents : design voulu ou réglage par défaut ?
