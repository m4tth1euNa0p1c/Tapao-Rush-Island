# TAPAO: Rush Island : kit d'interface Unreal (UMG)

Tous les éléments sont modélisés et rendus dans Blender. Le style : bois flotté, corde, laiton doré, velours bordeaux et parchemin.

Les sprites sont des PNG transparents **@2x** (200 px par unité). Dans UMG, affiche-les à 50 %, ou garde-les @2x pour la 4K.

| Dossier | Contenu |
|---|---|
| `renders/sprites/raw/` | 85 éléments :<br>• logo ;<br>• 14 boutons larges et 4 onglets ;<br>• 16 boutons-icônes ;<br>• 9 icônes HUD ;<br>• 4 cadres de jauge ;<br>• plaques de compteur ;<br>• 5 emplacements de barre d'objets ;<br>• touches et badges ;<br>• curseur, cases à cocher, boussole, minicarte ;<br>• panneaux parchemin, bannières de titre. |
| `renders/sprites/items/` | 30 vignettes 3/4 d'objets du jeu (inventaire, barre d'objets). |
| `renders/sprites/states/` | États `hover`, `pressed` et `disabled` des boutons, onglets, icônes, emplacements et touches. |
| `renders/sprites/fills/` | Remplissages des jauges (vie, endurance, combustible, or) et leur fond. |
| `renders/screens/` | Maquettes 1920×1080 : menu principal, HUD de jour, HUD de nuit, pause, options, inventaire, naufragé, île conquise, chargement. |
| `ue/ui_manifest.json` | Pour chaque sprite : fichier, taille, type, marges 9-slice (`unreal_margin`), états, fenêtre de remplissage des jauges, rayon de la minicarte. Contient aussi les couleurs et les polices. |
| `factory/assets/fonts/` | Pirata One (titres), Alegreya SC Black / Bold (boutons, compteurs, textes). Licence OFL : on peut les embarquer dans le jeu. |

## Import
- **Textures** : importe-les dans `/Game/TAPAO/UI/`. Réglages : *Texture Group* = `UI`, *Mip Gen* = `NoMipmaps`, *Compression* = `UserInterface2D`, *sRGB* activé.
- **Polices** : crée `Font_PirataOne` et `Font_AlegreyaSC` (*Font Face*), avec les 3 TTF.
- **Panneaux, boutons et bannières vierges** : *Draw As* = **Box**, avec la marge du manifeste (`unreal_margin`, exemple : panneau parchemin 0,07 / 0,1). Le texte est un `TextBlock` par-dessus.

## Widgets
| Widget | Contenu |
|---|---|
| `WBP_Button_Wood` | `Button` avec style Normal / Hovered / Pressed / Disabled (`btn_wide_blank` + `states/`). Texte : Alegreya SC Black 30, couleur #FAE8BE, contour 2 px #28160C. Son au survol : bois. Les sprites `btn_jouer`, `btn_options`… sont des versions pré-écrites, pour les maquettes ou un usage direct. |
| `WBP_IconButton` | `icon_*` + états (pause, réglages, fermer, retour, carte, sac, son, +/−, flèches, valider, info) |
| `WBP_Bar` | `Overlay` : `fill_background`, puis une `ProgressBar` dont le *Fill Image* est `fill_health`, `fill_stamina` ou `fill_fuel`, placée dans la fenêtre `fill_window_px` du manifeste, puis le cadre `bar_health`, `bar_stamina` ou `bar_fuel`. Valeur affichée à droite. |
| `WBP_Counter` | `counter_plate` + icône (`hud_coin`, `hud_pearl`, `hud_ball`, `hud_hourglass`, `hud_sun`) + `TextBlock` |
| `WBP_Hotbar` | 4 × `slot_0` (état sélectionné : `slot_0_hover`) + vignette d'objet + `badge_1` à `badge_4` + nombre de munitions |
| `WBP_Compass` | `compass_bar` + bande de lettres qui défile selon le cap de la caméra |
| `WBP_Minimap` | `RetainerBox` circulaire (rayon `window_radius_px`) avec un *RenderTarget* vu de dessus, puis `minimap_frame`, puis la flèche du joueur |
| `WBP_Prompt` | `key_e`, `key_f`… + action : « Ramasser · Poisson », « Allumer le feu de camp » |
| `WBP_Toast` | `panel_tooltip` (Box) + texte : notifications de gain et de quête |

## Écrans
| Écran | Contenu |
|---|---|
| `WBP_MainMenu` | Logo + JOUER, CONTINUER, NOUVELLE PARTIE, OPTIONS, CRÉDITS, QUITTER ; icônes info, réglages et son |
| `WBP_HUD` | En haut à gauche : vie, endurance, combustible de la torche (seulement si elle est en main). En haut au centre : boussole, minuteur du rush, jour ou nuit. En haut à droite : pièces, perles noires, balles. En bas : barre d'objets 1-4 au centre, minicarte à droite. Plus : invite d'interaction, notifications. |
| `WBP_Pause` | Touche Échap : voile sombre, panneau parchemin, bannière PAUSE, REPRENDRE, OPTIONS, MENU PRINCIPAL, QUITTER |
| `WBP_Options` | Onglets AUDIO / VIDÉO / COMMANDES, curseurs (`slider_track` + `slider_knob`), cases (`checkbox_on` / `off`), APPLIQUER / RETOUR |
| `WBP_Inventory` | Touche Tab : grille 6×4 d'emplacements, infobulle (`panel_tooltip`) avec nom, poids, prix et usage, charge totale, pièces |
| `WBP_GameOver` / `WBP_Victory` | Bannière NAUFRAGÉ ! ou ÎLE CONQUISE !, panneau des statistiques, RECOMMENCER ou CONTINUER, MENU PRINCIPAL |
| `WBP_Loading` | Logo, jauge or (`bar_blank` + `fill_gold`), astuce sur parchemin |

## Liens avec le gameplay
- **Endurance** : chaque coup de sabre coûte 11 %. La jauge clignote (animation UMG) en dessous de 11 %.
- **Combustible de la torche** : 300 s ; la jauge passe à l'orange vif sous 30 s.
- **Munitions** : `balles en sacoche / capacité` (23 / 23).
- **Minuteur du rush** : `hud_hourglass`, au format mm:ss.

## Régénérer
```
uv run factory/tools/maquette.py build ui-tapao --headless --force
uv run factory/tools/ui_compose.py
```
Les libellés des boutons, des icônes et des objets se trouvent en tête de `factory/kits/ui_kit.py` (`LABELS_WIDE`, `ICONS`, `ITEM_ICONS`).
