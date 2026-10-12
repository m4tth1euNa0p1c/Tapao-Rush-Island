# 26 — Compatibilité manette et Steam Deck
Domaine : C · Auditeur : spécialiste manette et Steam Deck

## Verdict
Le personnage marche, regarde, saute et sprinte à la manette, rien de plus ; combat, interaction, rames, menus et pause restent clavier-souris, sans glyphes adaptatifs. Sur Steam Deck rien n'est mesuré, et la configuration par défaut probable (Epic, Lumen matériel, 816 000 instances) est loin des 30 FPS à 800p de Valve. Risque : pas de «Deck Verified» ni «Steam Machine», donc un canal de découverte perdu.

## Constats

- **[Bloquant commercial] 4 actions sur 28 seulement ont une touche manette**
  - Constat : marche, regard, saut, sprint (IMC du gabarit). Attaque, garde, tir, interaction F, lâcher, chute, vue, inventaire, trompette : clavier-souris. Rames : quatre actions booléennes ZQSD/WASD/flèches créées en code, sans stick.
  - Preuve : clés des .uasset `IMC_Islander_Actions`, `IMC_Inventory`, `IMC_Musket/Cutlass/Torch/TrumpetEmote` (aucun `Gamepad_*`) ; `BoatRiderComponent.cpp:99-116`.
  - Recommandation : `IMC_Gamepad` créé en code, comme `IMC_BoatRowing` : stick gauche aux rames, gâchettes attaque/garde, X interagir.
  - Effort : M

- **[Bloquant commercial] Pause, retour et options inatteignables à la manette**
  - Constat : Échap ouvre la pause et sert de retour, F10 ouvre les réglages ; Start et B ne sont lus nulle part. Un joueur manette ne peut ni pauser, ni quitter.
  - Preuve : `TapaoUISubsystem.cpp:47`, `STapaoScreens.cpp:266,495`, `SettingsSubsystem.cpp:43` ; «Gamepad» dans `Source/` : un commentaire.
  - Recommandation : Start ouvre la pause, B (`Virtual_Gamepad_Back`) revient, dans ces trois traitements.
  - Effort : S

- **[Majeur] Menus : focus sur la racine, focus à peine visible**
  - Constat : `FocusUI` focalise le conteneur plein écran, pas un bouton ; la navigation directionnelle du moteur cherche au-delà du bord du widget focalisé, donc D-pad, stick et flèches n'atteignent aucun bouton (supposé, non testé). Le focus n'a que le liseré pointillé du moteur, aucun sprite TAPAO (§16).
  - Preuve : `TapaoUISubsystem.cpp:259-264`, `TapaoStyle.cpp:67-82`, moteur `HittestGrid.cpp`, `CoreStyle.cpp:305`.
  - Recommandation : focus initial sur «Jouer» ; sprite de survol aussi au focus.
  - Effort : S

- **[Majeur] Glyphes en dur, aucune détection du périphérique**
  - Constat : invite «F», ligne «Clic gauche… Molette», badges 1-4 de la barre d'objets ; ni CommonInput ni suivi du dernier périphérique. Valve : glyphes conformes à l'entrée active (Xbox acceptés), jamais clavier-souris avec une manette.
  - Preuve : `IslanderHUD.cpp:140,192`, `STapaoHUD.cpp:218`, `Build.cs:11-22`.
  - Recommandation : CommonUI (CommonInput), glyphes Xbox puis PlayStation ; Steam Input facultatif.
  - Effort : M

- **[Majeur] Réglages manette absents, curseurs bloqués au D-pad, regard lié au FPS**
  - Constat : pas d'onglet Manette (§18) ; la sensibilité «Souris» règle aussi le stick. `SSlider` garde son pas de 0,01 : le champ de vision, arrondi au degré, ne bouge jamais. Le regard au stick n'a pas de `ScaleByDeltaTime` (supposé) : deux fois plus lent à 30 FPS qu'à 60.
  - Preuve : `SSettingsMenu.cpp:80-83,374`, `IslanderCharacter.cpp:160-161,225-231`, modificateurs de `IMC_Default`, moteur `SSlider.h:37`.
  - Recommandation : pas = (max−min)/20 ; regard au stick × delta ; onglet Manette.
  - Effort : M

- **[Majeur] Textes sous le seuil Valve à 800p**
  - Constat : Valve : 9 px de haut minimum à 1280×800. L'interface est réduite à ×0,74 (courbe DPI par défaut) : la police 11 pt (aide des Réglages) fait ≈ 8 px de capitale (calcul, supposé). Le HUD canvas applique ClipY/1080.
  - Preuve : `SSettingsMenu.cpp:132,339`, `IslanderHUD.cpp:57`, moteur `SGameLayerManager.cpp:113-142,467-495`.
  - Recommandation : plancher 14 pt ; option «Échelle de l'interface» ; capture en 1280×800.
  - Effort : S

- **[Bloquant commercial] Performance Deck jamais mesurée, première configuration en Epic**
  - Constat : Valve : 30 FPS à 800p par défaut. Sans `DefaultScalability.ini`, le premier lancement garde le niveau moteur 3 (Epic), pas «Moyen» (supposé). `r.RayTracing=True` : Lumen matériel quand disponible, au coût signalé par le moteur au-delà de 100 000 instances (816 000 ici) ; VSM, Substrate, DX12 SM6. Deck : 1,6 TFLOPS, 16 Go partagés. Aucune capture ; T-091 `a-faire`.
  - Preuve : `DefaultEngine.ini:24,28,32,36,45`, `SettingsSubsystem.cpp:137,145`, moteur `Scalability.cpp:1038-1043`, `LumenHardwareRayTracingCommon.cpp:16-22`.
  - Recommandation : Deck = machine de référence (33 ms) ; préréglage Deck (Lumen logiciel, VSM off, végétation réduite) via `GetSteamHardwareDefaultConfig` (Steamworks 1.65).
  - Effort : L

## Manques
1. Navigation, retour et page Commandes avec remappage manette (CommonUI).
2. Clavier à l'écran Steam (nom de campagne, code d'invitation §16) : exigé par Valve ; Steamworks absent (`Build.cs:49`).
3. Retours haptiques : aucun `ForceFeedback` dans `Source/`.
4. Préréglage Deck, upscaling TSR/FSR, test sur un vrai Deck.

## Quick wins
1. Start = pause, B = retour (½ jour).
2. Focus initial sur «Jouer» et sprite de focus (½ jour).
3. Pas des curseurs, police ≥ 14 pt dans les Réglages (2 h).
4. «Moyen» forcé au premier lancement, `USettingsSubsystem::Load` (1 h).
5. Banc Deck sur PC : `-ResX=1280 -ResY=800`, `sg.*=0`, CSV Profiler, cible 33 ms (1 j).

## Questions qui challengent l'équipe
1. Deck et Steam Machine sont-ils des cibles assumées ? Qui possède un Deck, et quel rendu (Lumen, VSM, végétation) sacrifiez-vous pour 33 ms ?
2. Le combat est figé sur clics, molette et 1-4 : sermon immobile de 20 s avec dérive du stick, portage et rames restent-ils plaisants à deux sticks ?
3. Si la cible est le PC clavier-souris, acceptez-vous «manette partielle» sur Steam ? Sinon, quel lot est bloqué tant que la manette n'est pas complète ?

Sources : https://partner.steamgames.com/doc/steamdeck/compat, https://www.gamingonlinux.com/2026/07/valves-steamworks-sdk-v1-65-brings-changes-for-steam-machine-steam-frame-and-steam-deck/, https://gamingbolt.com/steam-deck-full-specs-revealed-features-custom-rdna-2-gpu-claims-1-6-tflops-of-power
