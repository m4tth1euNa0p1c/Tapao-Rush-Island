# 41 — Empreinte disque du projet et du build
Domaine : E · Auditeur : ingénieur build (taille disque)

## Verdict
Le contenu tient sur Steam : build estimé à 0,9–1,5 Gio (supposé) ; IoStore et Oodle sont déjà les défauts du moteur. La place perdue est ailleurs : 63,5 Gio sur disque dont 45 de worktrees, C: à 98 %, aucun remote. Risque commercial : un premier cook non mesuré, et une carte de 105 Mio recopiée à chaque commit.

## Constats
- **[Majeur] Build jamais cooké, aucun budget**
  - Constat : ni `Saved/Cooked` ni section Packaging, aucune tâche chantier. Content/MonkeIsland = 1 071 Mio : textures 532, maillages 221, DataTables 173, carte 105. Installé : carte ≈ 65 (zlib/lzma mesuré) ; textures ≈ 330, shaders 100–300, exe 180–260 supposés.
  - Preuve : DefaultGame.ini ; BaseGame.ini:87-109, BaseEngine.ini:3741 ; memory.md:44-51 ; board.md.
  - Recommandation : cook de mesure, Size Map, budget ≤ 2 Gio, section Packaging (`MapsToCook`, `DirectoriesToNeverCook`, `IncludePrerequisites=False` : −19 Mio). Plan complet : ≈ 0,6–1,2 Gio (supposé).
  - Effort : S
- **[Majeur] Textures : UI non compressée, doublons, 2K**
  - Constat : 270 textures UI en `TC_EditorIcon` (BGRA8, sans mips, toutes référencées par le skin) = 220 Mio cookés, ≈ 55 compressés. 30 groupes (61 copies) partagent une source : 58 Mio. 34 textures ≥ 2K = 168 Mio cookés.
  - Preuve : scan de 595 uassets ; DA_TapaoUISkin.
  - Recommandation : UI 9-slice + BC7 (220 → 50 Mio en mémoire), textures partagées, 1K pour totems et sols (−60 à −90 Mio cookés), `bFinalUsesRDO=true` (−15 à −30 % BCn, supposé).
  - Effort : M
- **[Majeur] DataTables de végétation : 173 Mio**
  - Constat : 47 tables a priori utiles seulement pour générer des ISM déjà sauvés dans la carte ; elle ne cite ni `Shrubs`, `GroundCover` ni `Props` (50,7 Mio). Compressées ×5–6 : ≈ 35 Mio installés.
  - Preuve : références binaires ; Environment/Vegetation/Data.
  - Recommandation : supprimer ces 3 tables et leurs graphes, vider les défauts de `VegHard/VegSoft/Village`, puis `DirectoriesToNeverCook` sur le dossier après vérification au cook. −173 Mio dépôt.
  - Effort : S
- **[Majeur] Carte monolithique de 105 Mio**
  - Constat : ≈ 135 o par instance (816 000), l'essentiel du fichier (supposé). Les 20 `DA_Island_Raraka_NN` visent `Lvl_Raraka`, aucune `L_Frag_*`. Les fragments couvrent 722 ha sur 1 000 ha de terre : découper retire ≈ 28 % de la végétation (supposé ; charge : 1/20 par session).
  - Preuve : fragments.json ; raraka/README.md:24.
  - Recommandation : `L_Frag_*` en External Actors (≈ 2,6 Mio par commit, pas 105), instances compactes (≈ 16 o : 13 Mio, supposé), `Lvl_Raraka` hors cook.
  - Effort : L
- **[Majeur] Template résiduel : 148 Mio, encore câblé**
  - Constat : Characters 125 Mio, Weapons 14. `BP_Islander`, `BP_IslandGameMode`, `IMC_Islander_AZERTY` en dépendent ; `GlobalDefaultGameMode` est celui du template.
  - Preuve : DefaultEngine.ini:17 ; scan des références.
  - Recommandation : déplacer IA_*/IMC dans `/Game/MonkeIsland/Input`, changer le mode global, supprimer le reste, puis Mannequins avec le bean. −148 Mio dépôt, −30 à −60 installés (supposé).
  - Effort : M
- **[Majeur] Exécutable de 319 Mio, plugins et shaders gonflés**
  - Constat : 259 modules dont 148 de plugins ; `ModelContextProtocol` et `MCPClientToolset` partiraient en Shipping ; 103 `M_`, RT et Substrate actifs.
  - Preuve : monke_island.uproject:35 ; uhtmanifest ; DefaultEngine.ini:23-38.
  - Recommandation : couper ≈ 40 plugins, MCP en Editor seul, matériaux maîtres : −40 à −80 Mio d'exe, shaders −30 % (supposé).
  - Effort : M
- **[Majeur] Disque de développement saturé**
  - Constat : `.worktrees` = 45 Gio : 18 arbres, 14 avec build privé (2,8 Gio chacun) ; 14 branches déjà fusionnées dans main (33,6 Gio) ; T-118 : +3 Gio pendant l'audit. C: n'a plus que 39 Gio libres. Aussi `.vs` 2,2, `Saved/Autosaves` 0,6, `raraka_terrain_avant_pente` 0,9 (copie).
  - Preuve : du ; `git branch --merged` ; worktree.py:cmd_merge.
  - Recommandation : `remove` des 14 fusionnés, nettoyage dans `merge` : ≈ −37 Gio avec le reste.
  - Effort : S
- **[Majeur] LFS : 3,64 Gio pour 2,25 utiles, aucune sauvegarde**
  - Constat : `.git/lfs` = 0,34 Gio d'anciennes versions (3 cartes = 89 %) + 1,10 Gio sans commit (dont 7 cartes ; supposé : le filtre clean les stocke à chaque `git status`). `git remote -v` est vide. docs 2,2 Gio = `raw/` ignoré ; art 2,0 dont 1,04 suivis.
  - Preuve : `git lfs ls-files --all` ; `.git/lfs/objects`.
  - Recommandation : `git lfs prune` (−1,1 Gio) ; sortir du suivi `art/export`, `archives`, `_previews` (0,92 Gio) si scripts et .blend restent ; remote LFS ou Perforce.
  - Effort : S

## Manques
- Section Packaging, liste de cartes (cahier:433).
- Sauvegarde distante, `git lfs prune` planifié.
- Patch Steam (chunks IoStore), `.pdb` (377 Mio) hors build.
- Budget audio et musique (10 Mio aujourd'hui).

## Quick wins
1. `worktree.py remove` sur les 14 arbres fusionnés : −33,6 Gio.
2. Supprimer `.vs`, Autosaves, `avant_pente` ; `git lfs prune` : −4,8 Gio.
3. `TargetAllowList: ["Editor"]` sur les plugins MCP ; supprimer 3 DataTables et leurs graphes : −50,7 Mio.
4. Section Packaging, puis cook Development de mesure.

## Questions qui challengent l'équipe
1. Quel budget d'installation, mesuré sur quel cook ?
2. `Lvl_Raraka` : carte jouée ou source (ADR-004 §7) ? Pourquoi 20 îles la visent-elles ?
3. Si C: tombe ce soir, que reste-t-il du jeu ?
