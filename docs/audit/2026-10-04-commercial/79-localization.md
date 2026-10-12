# 79 — Localisation
Domaine : H · Auditeur : chef de projet localisation

## Verdict
Le jeu est monolingue français et sans infrastructure : 89 clés `LOCTEXT`, une String Table, des textes invisibles pour le Gather, ~25 `FString` en dur, 22 sprites d'interface au français dessiné dans l'image. Risque commercial : le français seul ne vise que 2,1 % des clients Steam (EN + ZH-Hans : 65,6 %). Le volume (~1 000 mots, estimé) s'assainit en deux semaines.

## Constats
- **[Bloquant commercial] Aucune infrastructure de localisation**
  - Constat : ni cible Localization Dashboard, ni culture native (source française : sans `fr` natif, le Gather la croit anglaise), ni `CulturesToStage`, ni onglet Langue (§18). Aucune tâche au chantier.
  - Preuve : `Config/` (5 ini, zéro « Culture »), `DefaultGame.ini:25-36`, pas de `Content/Localization`.
  - Recommandation : cible « Game » (native `fr`, Gather source, assets, `ST_*`, hors `Variant_*`), `CulturesToStage=fr,en`, onglet Langue (défaut = langue Steam : `schinese`→`zh-Hans`).
  - Effort : M
- **[Bloquant commercial] 22 sprites TAPAO portent le français dans l'image**
  - Constat : 13 boutons, 3 onglets, 6 bandeaux (JOUER, PAUSE…) sont des PNG « pré-écrits ». Les vierges existent (`btn_wide_blank`, `tab_blank`, `ribbon_blank`) et `MakeTapaoTextButton` y pose déjà un `STextBlock`.
  - Preuve : `ui_manifest.json` (champ `label`), `TapaoStyle.cpp:76`, `STapaoScreens.cpp:143-148,297-307`.
  - Recommandation : tout basculer sur les vierges + `LOCTEXT`.
  - Effort : S
- **[Majeur] Textes invisibles au Gather ou en dur**
  - Constat : `DA_TapaoUISkin`, `DA_Item_Torch`, `DA_Consumable_Fish` viennent de Python `unreal.Text("…")` : FText culture-invariants, sans clé, donc français partout. HUD canvas, barque, réglages : ~25 littéraux (« Santé %.0f », « Tu te noies ! »). Le naufrage passe par `AddOnScreenDebugMessage` sans garde Shipping (invisible en build, supposé). Pluriel maison « expédition(s) » (russe : 3 formes).
  - Preuve : octets de `DA_TapaoUISkin.uasset` (CultureInvariant) contre clé GUID de `DA_Item_Cutlass` ; `import_tapao_ui.py:194-196` ; `IslanderHUD.cpp:69-328` ; `BoatActor.cpp:959` ; `TapaoUISubsystem.cpp:341`.
  - Recommandation : `ST_Text_UI` et `ST_Text_Items` par `LOCTABLE` (modèle `setup_fragments.py:92`), `{0}|plural(...)`, lint refusant `unreal.Text(` et `FText::FromString`.
  - Effort : S
- **[Majeur] Polices sans cyrillique pour les titres, sans CJK ; largeurs fixes**
  - Constat : mesuré sur les TTF, Pirata One (titres) a 0/256 cyrillique, Alegreya SC 178/256 ; aucune ne couvre CJK ni hangul, et la police composite TAPAO n'a pas de repli (supposé : carrés). Étiquettes des réglages fixées à 240 px sans retour à la ligne : allemand et russe (+30 %, supposé) rogneront.
  - Preuve : cmap de `art/ui/tapao/fonts/*.ttf`, `TapaoStyle.cpp:113-122`, `SSettingsMenu.cpp:170`.
  - Recommandation : polices par culture (Noto Sans CJK, OFL, sous-ensemblées), pseudo-localisation (PO ×1,4).
  - Effort : M
- **[Majeur] `ST_Islands` : bon modèle, partiel ; villages en FString**
  - Constat : 70 clés, 443 mots, liées à 8 îles sur 20 ; EN seulement dans `islands.md` ; `CrownName`, `Pitch`, `Rumor`, villages : lus par aucun code. `FVillageLayout::Name` est un `FString` ; le générateur grave 60 `…-mura` qui ne coïncident avec `ST_Islands` pour aucun des 24 villages des îles 01-08.
  - Preuve : `ST_Islands.csv`, `DA_Island_Raraka_01..20.uasset`, `VillageLayout.h:181`, `fragment_villages.py:60-66,210`, `asset-naming.md:27,32`.
  - Recommandation : `ST_Text_Villages` par `Id` ; renommer `ST_Islands` (`ST_` = StateTree) ; rōmaji en latin, kanji en JA/ZH, relus par un natif (mots réels : Kinko = coffre-fort ; Izumi, Minato = lieux réels).
  - Effort : M
- **[Majeur] Langues Steam : ordre et coût**
  - Constat : le §18 ne promet que FR puis EN. Steam, sept. 2026 : EN 34,2 %, ZH-Hans 31,4 %, RU 9,1 %, ES 4,1 %, PT-BR 3,7 %, DE 2,5 %, JA 2,4 %, FR 2,1 %, KO 1,4 % (très volatile). Tolérance à l'anglais : ZH 32 %, JA 23 %, KO 22 %, DE 61 %.
  - Preuve : https://store.steampowered.com/hwsurvey/ ; https://www.steampageanalyzer.com/blog/steam-localization-languages ; https://partner.steamgames.com/doc/store/localization/languages ; https://www.gamingonlinux.com/2026/03/steam-survey-for-february-2026-shows-a-big-swing-to-simplified-chinese
  - Recommandation : FR + EN à l'accès anticipé, puis ZH-Hans et JA, puis KO, PT-BR, ES, RU, DE. Texte : ~1 000 mots aujourd'hui, 6 000 à 8 000 en V1 ; 8 langues à ~0,10 €/mot + relecture ~8 k€ (tout supposé).
  - Effort : L
- **[Mineur] Voix et satire**
  - Constat : le sermon est « une voix préenregistrée ou provisoire » ; aujourd'hui, 47 sons de créatures sans parole. La satire (thé, formulaires, Bureau) est britannique, écrite en français.
  - Preuve : cahier §6.2 l.110, `characters-tiki.md:192`.
  - Recommandation : voix non verbales + sous-titres localisés ; EN-GB relu par un natif comme pivot ; glossaire (marae, tiki, hoa, motu jamais traduits) ; relecture native JA/KO/ZH des villages japonais pacifiés.
  - Effort : S

## Manques
1. Cible, culture, sélecteur de langue.
2. Glossaire, guide de style, contexte par clé.
3. Polices par script, pseudo-localisation.
4. Page Steam, capsule, EULA, sous-titres.
5. Processus fournisseur (PO, mémoire, relecture en jeu).

## Quick wins
1. Remplacer `unreal.Text(` par `LOCTABLE`.
2. Basculer les 22 sprites sur les vierges.
3. Afficher « Привет 你好 こんにちは 안녕 » dans TAPAO (repli des polices : supposé).
4. Clé `Game.Title` : « TAPAO : Rush Island » subsiste partout.
5. Convertir la colonne EN de `islands.md` en PO.

## Questions qui challengent l'équipe
1. Le français reste-t-il langue maîtresse, alors que la satire est britannique et l'anglais le pivot des traducteurs ?
2. Voix sans paroles (zéro doublage) ou sermon parlé : qui tranche avant l'enregistrement ?
3. Qui relira les villages japonais en JA, ZH, KO, et que faites-vous du verdict « caricature » ?
