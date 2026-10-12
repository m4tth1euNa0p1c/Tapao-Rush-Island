# Textes du jeu contraires à la bible (relevé T-137)

| Champ | Valeur |
|---|---|
| Tâche | T-137 (audit T-117 : 71, 74, 75, 76) · lots L3, L4, menus §16 (UI12) |
| Référence | `docs/narrative/BIBLE.md` (proposition à valider) |
| Date | 2026-10-10 |
| Méthode | recherche de `LOCTEXT`, `NSLOCTEXT` et des chaînes `TEXT("…")` affichées dans `monke_island/Source/monke_island/` (UI, HUD, invites, règles) ; textes posés par script dans les DataAssets (`scripts/ui/import_tapao_ui.py`, `scripts/editor/campaign_setup_ue.py`) ; sprites à texte de `art/ui/tapao/` |
| Limites | aucun `.uasset` relu dans l'éditeur (pas d'éditeur dans cette tâche) : un DataAsset peut garder une valeur différente du défaut C++ ou du script |
| Statut | relevé seulement : **aucun fichier de code, de script ni d'asset modifié**. Application : `ue-ui-developer` (T-138), en gardant les clés (namespace, clé) et en ne changeant que le texte source |

Codes d'écart : **M** monnaie (« $ » au lieu de couronnes) · **F** fiction (contredit le lore) · **T** ton (charte Bureau / Bord / Équipage, menus au vouvoiement) · **P** périmé (mécanique changée : ADR-005, ADR-007, cérémonie E-08) · **L** lexique public (T-139, décision du propriétaire en attente) · **N** nom du jeu (E-04, non tranché).

## 1. Textes affichés, dans le code

| # | Fichier:ligne | Namespace, clé | Texte actuel | Écart | Proposition |
|---|---|---|---|---|---|
| 1 | `UI/Tapao/STapaoHUD.cpp:267` | TapaoUI, StageQuota | « Quota {0} / {1} $ » | M | « Quota {0} / {1} c » |
| 2 | `UI/Tapao/STapaoHUD.cpp:432` | TapaoUI, Cargo | « {0} $ » | M | « {0} c » |
| 3 | `UI/Tapao/STapaoDayEnd.cpp:78` | TapaoDayEnd, QuotaBig | « QUOTA : {0} / {1} $ » | M | « QUOTA : {0} / {1} couronnes » |
| 4 | `Ceremony/FlagCeremonyRules.h:137` | FlagCeremony, Reward | « +{0} $ » | M | « Prime de possession : +{0} couronnes » |
| 5 | `Player/IslanderHUD.cpp:162` et `:180` | aucune (texte en dur) | « %.0f kg · %d $ » | M ; texte hors LOCTEXT | « {0} kg · {1} c », en LOCTEXT |
| 6 | `UI/Tapao/STapaoShop.cpp:189` | TapaoShop, Money | « Pas assez d'argent dans la trésorerie. » | M (mineur) | « Pas assez de couronnes dans la trésorerie. » |
| 7 | `Player/IslanderHUD.cpp:346` | aucune (texte en dur) | « -{dégâts}  TRANCHÉ ! » | F, L : célébration de la découpe, villageois découpables (T-139) | le chiffre seul, « -{0} » : aucun mot |
| 8 | `UI/Tapao/TapaoUISubsystem.cpp:636` | TapaoUI, StatIslands | « Îles conquises : {0} » | F : une île ne se conquiert pas, on la quitte à temps | « Îles quittées à temps : {0} » |
| 9 | `UI/Tapao/TapaoUISubsystem.cpp:635` | TapaoUI, StatWrecks | « Naufrages : {0} » | F : compte des morts ; le galion ne fait pas naufrage | « Recrues perdues : {0} » |
| 10 | `UI/Tapao/STapaoDayEnd.cpp:55` | TapaoDayEnd, Victory | « VICTOIRE : l'Île interdite est vaincue ! » | F ; P : le palier « Île interdite » n'existe plus dans `DA_RunRules` | « CAMPAGNE HONORÉE : la Couronne est satisfaite. Provisoirement. » |
| 11 | `UI/Tapao/TapaoUISubsystem.cpp:783` | TapaoUI, RunNew | « Nouvelle partie : Lagons extérieurs, jour 1 » | P : le premier palier s'appelle « Villages et marae » (`Run/RunRules.cpp:25`) | « Nouvelle partie : {0}, jour 1 », nom lu dans `DA_RunRules`, jamais en dur |
| 12 | `UI/Tapao/STapaoDayEnd.cpp:89` | TapaoDayEnd, Dead | « Morts : {0} · frais médicaux : {1} » | L : cahier §24.4 « frais de recrutement », ADR-007 « frais médicaux » | « Recrues perdues : {0} · frais de recrutement : {1} » (décision 5) |
| 13 | `UI/FloodTimerWidget.cpp:188` | MonkeFloodTimer, Over | « Île submergée : l'eau monte encore, montez dans la barque ! » | P : depuis l'ADR-005, le refuge est le galion ; T : Bord, six mots, sans « ! » | « Île submergée : regagner le galion. » |
| 14 | `Boats/BoatRiderComponent.cpp:335` | aucune (texte en dur) | « Tu te noies ! Regagne la barque ou la rive » | T : Bord ; texte hors LOCTEXT | « Noyade : regagner la barque ou la rive. » |
| 15 | `Boats/BoatRiderComponent.cpp:360` | aucune (texte en dur) | « Surcharge : la barque prend l'eau ! Allégez-la » | T : « tu » et « vous » mêlés dans le même HUD | « Surcharge : délester la barque. » |
| 16 | `Boats/BoatActor.cpp:1037` | aucune (message de débogage à l'écran) | « La barque a coulé : surcharge ! Tout ce qu'elle portait est perdu. » | T : la chute vient après le danger, par le Bureau | notification : « Barque perdue par surcharge. La Couronne déplore la perte de la marchandise. » |
| 17 | `Settings/SSettingsMenu.cpp:405` et `:408` | SettingsMenu, VoiceHelpShipping et VoiceHelp | « … la nuit, les mobs proches vous entendent. » | L : jargon « mob » | « … la nuit, les créatures proches vous entendent. » ; les « 25 m » du même texte lus dans les règles de la voix (`{0}`) |
| 18 | `UI/Tapao/STapaoPlayMenu.cpp:60` | TapaoPlayMenu, HostHint | « Multijoueur : jusqu'à 4 joueurs, qui rejoignent ton adresse IP » | T : menus au vouvoiement | « Multijoueur : jusqu'à {0} joueurs, qui rejoignent votre adresse IP. » (nombre lu dans la configuration) |
| 19 | `UI/Tapao/TapaoUISubsystem.cpp:895` | TapaoUI, ConfirmQuit | « Quitter TAPAO : Rush Island ? » | N | « Quitter le jeu ? » tant que E-04 n'est pas tranché |
| 20 | `Run/RunRules.cpp:25` (défaut C++) et `scripts/editor/campaign_setup_ue.py:35` (valeur du DA) | RunRules, Villages | « Villages et marae » | F (mineur) : le marae n'est pas un objectif de contrat | « Villages et lagons » |
| 21 | `Creatures/GiantCrabRules.cpp:49` | GiantCrab, BossName | « Tupa Nui, Gardien du récif » | L : mot réel pris pour nom propre (« tupa » : crabe de terre en Polynésie française ; non sacré) | décision 4 : garder après relecture par un locuteur pa'umotu, sinon « Le Vieux-Crabe, Gardien du récif » |

### Variantes facultatives (textes conformes, ton du Bureau proposé)
| Fichier:ligne | Namespace, clé | Texte actuel | Pourquoi il est conforme | Variante |
|---|---|---|---|---|
| `UI/FloodTimerWidget.cpp:279` | MonkeFloodTimer, ZoneSecured | « Zone sécurisée : +{0} » | texte prescrit par le cahier §6.1 | « Parcelle proclamée : sursis +{0} » (amendement du cahier) |
| `Ceremony/FlagCeremonyRules.h:133` | FlagCeremony, Done | « La Couronne britannique revendique ces terres » | demande du propriétaire (2026-10-07) | garder, et ajouter une ligne : « Consentement des habitants : réputé acquis. » |
| `UI/Tapao/STapaoDayEnd.cpp:58` | TapaoDayEnd, Executed | « QUOTA NON ATTEINT : l'équipage est passé par les armes » | conforme à l'exécution (2026-10-07) | « AVIS DE DÉCHÉANCE : l'équipage est déclaré traître et remis au peloton français. » (relie le cahier §24.2 à la scène) |
| `Execution/ExecutionScene.cpp:206` | ExecutionScene, QuotaMissed | « Quota non atteint » | idem | « Avis de déchéance » |

## 2. Textes dans les images (sprites TAPAO)
| Sprite | Utilisé par | Texte | Écart | Proposition |
|---|---|---|---|---|
| `art/ui/tapao/sprites/raw/logo.png` | menu principal, chargement | « TAPAO / RUSH ISLAND » | N ; « tapao » est aussi un mot tahitien (« signe, marque », Tregear 1891, à confirmer) | logo au nom retenu par E-04 |
| `ribbon_ile_conquise.png` | `UI/Tapao/TapaoUISubsystem.cpp:640` (victoire) | « ÎLE CONQUISE ! » | F | `ribbon_blank` et texte localisé « CONTRAT HONORÉ » : le mécanisme existe déjà pour « EXÉCUTÉS » (`:641`) |
| `ribbon_naufrage.png` | `UI/Tapao/TapaoUISubsystem.cpp:640` (fin sans victoire) | « NAUFRAGÉ ! » | F | `ribbon_blank` et « ÉQUIPAGE PERDU » (équipage entier mort) ou « RECRUE PERDUE » (un joueur) |

Les sprites à texte ne se traduisent pas : T-138 prévoit déjà de les remplacer par les sprites vierges.

## 3. Textes posés par script dans `DA_TapaoUISkin`
Source : `scripts/ui/import_tapao_ui.py`. Les crédits (menu Crédits, `UI/Tapao/STapaoScreens.cpp:207`) et la version (`:179`) sont affichés. Les astuces de chargement ne sont lues par aucun code aujourd'hui (`LoadingTips` n'est que déclaré, `UI/Tapao/TapaoUISkin.h:133`) ; la bible en fait des circulaires du Bureau.

| Ligne | Texte actuel | Écart | Proposition |
|---|---|---|---|
| 29 | « Pacifie une zone : plante le drapeau et tiens ton sermon 20 secondes, sans bouger. » | P : cérémonie des couleurs, durée dans `DA_FlagCeremonyRules` ; T | « Circulaire n° 1 : l'officiant hisse les couleurs sans bouger. Toute agitation annule l'Acte. » |
| 30 | « Chaque zone pacifiée repousse la montée des eaux de deux minutes. » | P : durée écrite en dur, bonus à arbitrer (T-126) | « Circulaire n° 2 : toute parcelle proclamée obtient de la mer un sursis de {0}. » |
| 31 | « Arrime ton butin dans la barque : ce qui reste à terre est perdu à la submersion. » | P : seul compte ce qui est à bord du galion (ADR-005) | « Circulaire n° 3 : seul compte ce qui est à bord du galion au départ. Le reste est réputé épave. » |
| 32 | « La torche s'éteint dans l'eau ; garde-la au sec et rallume-la au silex. » | T | « Circulaire n° 4 : la torche s'éteint dans l'eau. Le Bureau décline toute responsabilité. » |
| 33 | « Les campements de pêcheurs cachent des outils de valeur et du poisson pour reprendre des forces. » | F : promet de piller les habitants (`islands.md` §1.7) ; L (T-139) | « Circulaire n° 5 : le poisson des camps de pêcheurs rend des forces. Les pêcheurs, eux, ne se montrent pas. » |
| 34 | « Le Marae des Perles garde un artefact rare : approche-toi avec prudence. » | F : fait du marae une cible | « Circulaire n° 6 : la Perle du Gouverneur aurait été perdue près d'un marae. Le marae, lui, n'est pas à vendre. » |
| 35 | « Touche V : sonne la fanfare de la trompette. » | T : Bord, infinitif | « V : sonner la fanfare. » |
| 36 | « Une arme lâchée par un mob se revend : ramasse-la et range-la dans la barque. » | L : « mob » ; P : barque au lieu du galion | « Circulaire n° 7 : une arme de Veilleur se revend. Encore faut-il la rapporter à bord. » |
| 39 | « TAPAO : Rush Island » (crédits) | N | nom retenu par E-04 |
| 44 | « Atoll de Raraka : données Te Fenua / DAF, Licence Ouverte Etalab 2.0 » | L : toponyme réel dans un texte public ; licence à vérifier (T-130) | décision 1 : « Données géographiques : Te Fenua / DAF ({licence vérifiée par T-130}), transformées. » |
| 45 | « Le village de Motutapu est réel ; les personnages et événements du jeu sont fictifs. » | L : nomme le village réel | décision 1 : « Le décor s'inspire d'un lieu réel dont les habitants ne sont pas représentés. Personnages et événements sont fictifs. » |
| 194 | « TAPAO : Rush Island · v0.1 » (version du menu) | N | « v0.1 » seul jusqu'à E-04 |

## 4. Noms techniques destinés à l'écran (non affichés aujourd'hui)
- **Artefacts** (`/Game/MonkeIsland/Items/PolynesianArtifacts/`, liste dans `scripts/editor/campaign_setup_ue.py:31`) :
  - `FishGodFragment` (objet de culte) et `CrownOfTheLastAriki` (titre réel « ariki ») : à renommer et à revoir visuellement avec T-139. La bible en fait des trésors de naufrage : par exemple « Poisson de nacre » et « Couronne du Gouverneur noyé » ;
  - `RedPearlOfRaraka` : toponyme réel → « Perle rouge » ;
  - `HeartOfTheReef`, `EclipseStone`, `MoonPearl` : sans objection (« Cœur du Récif », « Pierre d'éclipse », « Perle de lune »).
- **Relique des marées** (`SM_TidalRelic`, `scripts/props/tidal_relic.py:1`) : « petit tiki en basalte » à spirales koru (motif maori), remis au joueur pendant la cérémonie. Motif sacré réel : T-139.
- **Totems** (`BP_Totem`, maquette `totem-tiki`, 150 c) : T-139.
- **Temple reliquaire** : modules nommés d'après le marae réel (ahu, fare ia manaha, tiki). Ces mots restent hors des textes.
- `BP_Tiki*`, `TikiDirector`, `FRG_Raraka_NN` : identifiants techniques, jamais affichés. Aucun changement demandé.

## 5. Documents internes en contradiction (hors jeu, non modifiés)
- `.claude/agents/ue-narrative-designer.md:12` : « toponymes réels (… village de Motutapu, passe Manureva…) » à utiliser. Cela contredit E-03 et `islands.md` §1.2. Proposition : les toponymes réels servent à se repérer (identifiants, données) ; les noms affichés viennent d'`islands.md`.
- `docs/templates/narrative-quest.md:4` : « toponymes réels vérifiés » → « noms d'`islands.md` ».
- `docs/features/character-bean-soldier.md:66` : skin « indigène fictif » → à retirer (audits 72 et 76 ; lexique de la bible).
- ADR-007 (« frais médicaux », « chaloupe ») face au cahier §24.4 (« frais de recrutement ») et au code (« barque »).
- Cahier §6.2 (« sermon », 20 s) et cérémonie des couleurs (60 s, ADR-001 E-08) : la bible les réunit sous le nom « Acte de possession », lu pendant la montée des couleurs.

## 6. Décisions en attente (propriétaire)
1. **Lexique public et contenu sacré (T-139)** : « Veilleurs » à la place de « tiki » ; Raraka et Motutapu hors des textes publics, crédits compris (§3, lignes 44 et 45) ; marae et temples non pillables ; villageois non létaux et non découpables (§1, n° 7) ; artefacts, reliques et totems aux motifs réels (§4). La bible applique ces points à titre de proposition.
2. **Couronne** : « fictive » (cahier §1) ou britannique (Union Jack, cérémonie du 2026-10-07). La bible dit « britannique de fantaisie » : aucun souverain, ministre, navire ou fait réel.
3. **Défaite** : le cahier §24.2 (frégate, « déclaré traître », sans gore) et l'exécution par le peloton français (2026-10-07). La bible les relie par la « Convention de réciprocité ».
4. **Noms** : nom du jeu (E-04 ; « Crownfall » est déjà pris d'après l'audit 78 ; « TAPAO » est un mot tahitien) ; Tupa Nui ; noms proposés : l'Anneau, l'Acte de possession, « le Sept », le Percepteur de 3e classe.
5. **Frais par mort** : « médicaux » (ADR-007) ou « de recrutement » (cahier §24.4).
6. **Villageois pendant le déluge** : la bible dit qu'aucun ne se noie à l'écran. Or `ATikiVillager` ne réagit pas à l'eau aujourd'hui (aucune règle de départ dans `Creatures/TikiVillager.cpp`). Si la règle est validée, une tâche de design est à prévoir (T-139).


## Corrigé par le lead (2026-10-10)
Lignes 1 à 6 (monnaie en couronnes : HUD, quota, cargaison, prime de la cérémonie, détails d'objet, refus de la boutique), 10 (victoire de campagne sans palier disparu) et 11 (« Nouvelle partie : jour 1 », sans nom de palier en dur). Les autres lignes attendent les décisions du propriétaire (ton, T-139, frais).
