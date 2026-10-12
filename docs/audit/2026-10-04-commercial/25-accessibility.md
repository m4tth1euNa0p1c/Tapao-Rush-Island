# 25 — Accessibilité
Domaine : C · Auditeur : experte accessibilité jeux vidéo

## Verdict
L'accessibilité n'existe pas encore: 4 onglets de réglages (Confort réduit au FOV) pour 19 lignes au §18, et aucune tâche T-NNN. Les bases sont prudentes (caméra globalement stable, aucun flash d'écran, minuteur doublé de texte et d'icône) et les gros risques (orage, voix du sermon) ne sont pas codés: rattraper coûte le moins maintenant. Rien ne bloque Steam, mais chaque écran sans règle alourdit la dette.

## Constats
- **[Majeur] Aucun sous-titre ni signal visuel des sons**
  - Constat : aucun code de sous-titres, alors que le §6.2 les impose au sermon et que le §18 les active par défaut. Aucun signal de dégâts reçus (ni son, ni direction) dans le C++ du joueur (Blueprints non lus).
  - Preuve : grep "Subtitle|Caption" = 0 dans Source; "Sound|Pain" = 0 dans Player/; GAG intermédiaire: direction d'une attaque doublée en visuel.
  - Recommandation : sous-système et widget TAPAO avant toute voix enregistrée; légendes [rugissement]; flèche de dégâts optionnelle.
  - Effort : M
- **[Majeur] Remappage absent, touches écrites en dur**
  - Constat : pas d'onglet Commandes ni d'Enhanced Input User Settings; invites codées ("F", "R recharger", "Molette"); menus sans cadre de focus (supposé). Atout: ZQSD, WASD et manette coexistent.
  - Preuve : SSettingsMenu.h:32; DefaultInput.ini; IslanderHUD.cpp:140,192,261; TapaoStyle.cpp:73-84; player-islander.md:51; recette UI08.
  - Recommandation : profils de touches, onglet Commandes (conflits), glyphes lus dans les mappings, focus initial visible.
  - Effort : M
- **[Majeur] Tout se maintient, rien ne bascule**
  - Constat : sprint, garde, visée, parade, charge du sabre, torche et rames se maintiennent; seul le portage F est à bascule. Le sermon de 20 s prévoit un Basculer (§18, UI11), sans réglage pour le porter.
  - Preuve : IslanderCharacter.cpp:162,175,303; MusketWeapon.cpp:215; CutlassWeapon.cpp:207; TorchItem.cpp:212; BoatRiderComponent.cpp:118; GAG intermédiaire: alternatives au maintien.
  - Recommandation : Maintenir/Basculer par action (déclencheurs Enhanced Input), Basculer par défaut pour le sermon (audit 03).
  - Effort : M
- **[Majeur] Lisibilité : HUD, texte, couleurs**
  - Constat : jauges de 200×7 px sans chiffre, boutons de menu en images au libellé incorporé, aucune échelle 80–150 % (§18, UI12), HUD de visée en polices moteur (taille non mesurée, supposée <18 px), menu des réglages en polices fixes sans défilement. Rouge/vert/orange doublés d'icônes (conforme), mais sans palette ni simulation.
  - Preuve : STapaoHUD.cpp:29,115-189 ("Label" jamais affiché); TapaoStyle.cpp:76; IslanderHUD.cpp:196-208; SSettingsMenu.cpp:65,87; XAG 101 (PC ≥18 px à 1080p, échelle jusqu'à 200 %): learn.microsoft.com/gaming/accessibility/xbox-accessibility-guidelines/101
  - Recommandation : échelle d'interface globale, HUD canvas migré en Slate, valeurs et épaisseur en option, 3 palettes en DataAsset, "jamais la couleur seule".
  - Effort : M
- **[Majeur] Mal des transports : bonnes bases, trous réels**
  - Constat : le curseur FOV ne touche que la caméra FirstPerson, or l'assise à la barque force la 3e personne (80° fixes). Secousse de roulis du mousquet, pompage des bras, flou de mouvement (supposé actif) et lissage souris sans interrupteur. Points forts: pas de balancement de tête (supposé), galion ±1°, houle ~40 cm (audit 15).
  - Preuve : SettingsSubsystem.cpp:275-277; BoatRiderComponent.cpp:298; MusketRules.h:253; BeanFirstPersonArmsComponent.h:81; DefaultInput.ini:89; DefaultEngine.ini sans MotionBlur; GalleonRules.h:65; XAG 117: learn.microsoft.com/gaming/accessibility/xbox-accessibility-guidelines/117
  - Recommandation : Confort §18 (secousses, bras, flou, lissage, FOV en 3e personne), puis test de houle avec trois testeurs sensibles.
  - Effort : M
- **[Mineur] Flashs d'orage : budget à fixer avant le premier éclair**
  - Constat : aucun éclair dans le code, tempête muette (audit 15), mais le §8 exige des "flashs limités" et une option. Torche (±10 % à 1,3 Hz, bruit 9 Hz) et éclair de mousquet (0,08 s): sans doute sous les seuils (supposé), jamais mesurés.
  - Preuve : grep "Lightning|éclair" = 0; TorchRules.h:98-102; MusketRules.h:285-289; WCAG 2.3.1 (≤3 flashs/s): w3.org/WAI/WCAG21/Understanding/three-flashes-or-below-threshold.html
  - Recommandation : budget en DataAsset (≤2 flashs/s, aucun rouge saturé), option "Réduire les flashs", analyse de capture (type PEAT), mention sur la page Steam.
  - Effort : S
- **[Majeur] Aucune difficulté ni assistance**
  - Constat : Nouvelle partie crée ou écrase sans préréglage; le "Personnalisé" du §16 n'existe pas; le §18 interdit au Confort de donner du temps, donc l'assistance doit passer par la campagne.
  - Preuve : TapaoUISubsystem.cpp:574-592; RunRules.h:76-82; GAG basique (difficulté, vitesse de jeu): gameaccessibilityguidelines.com/full-list
  - Recommandation : préréglages et Personnalisé en DataAsset (sermon 5–40 s, horloge ×0,5–×3, dégâts ×0,25–×1), fixés par l'hôte au hub, visibles au lobby, sans pénalité.
  - Effort : M

## Manques
1. Onglet Accessibilité du §18 (sous-titres, contraste, palettes, réduction flashs/mouvement) et écran guidé au premier lancement.
2. Communication sans voix: roue de pings et messages.
3. Audio mono et plage dynamique "Nuit".
4. Menu narré (optionnel).

## Quick wins
1. Afficher les valeurs des jauges: le lambda `Label` est déjà fourni, jamais lu (STapaoHUD.cpp:165).
2. Défauts sûrs: `r.DefaultFeature.MotionBlur=0`, `bEnableMouseSmoothing=False`.
3. Curseur FOV aussi sur la caméra de 3e personne (SettingsSubsystem.cpp:275).
4. `ToggleSound` rend 100 % au lieu du volume réglé (STapaoScreens.cpp:260).
5. Écrire dans docs/conventions "jamais la couleur seule" et le budget de flashs.

## Questions qui challengent l'équipe
1. Qui possède l'accessibilité, sans aucune T-NNN au board, et à quel lot?
2. Un sermon de 20 s immobiles exclut-il volontairement qui ne peut tenir une touche?
3. Livrerez-vous tempête et houle sans interrupteur ni test de mal des transports, au risque d'avis négatifs et de remboursements?
