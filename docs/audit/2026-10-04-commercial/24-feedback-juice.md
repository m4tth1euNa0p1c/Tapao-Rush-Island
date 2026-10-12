# 24 — Retours et « juice » sur les actions clés
Domaine : C · Auditeur : spécialiste du game feel

## Verdict
Les armes ont de vrais retours (mousquet : platine, détonation, flamme, fumée, recul ; sabre : 7 sons, chiffres), pas le reste : huit systèmes jouent un son, zéro Niagara, CameraShake, haptique ou ralenti. Butin, +2:00, naufrage et mort sont les moments les plus pauvres, et aucune recette ne mesure le ressenti. Risque commercial : un jeu qui « marche » sans se ressentir.

## Constats
- **[Bloquant commercial] Boucle butin muette**
  - Constat : soulever, porter, lâcher, cogner : ni son, ni particule. La perte de valeur, cœur du jeu façon R.E.P.O., ne change qu'un nombre du prompt : `OnValueChanged` et `OnBroken` n'ont aucun abonné. Seules les noix sonnent à l'impact ; le choc n'est lu que par le serveur.
  - Preuve : ValuableComponent.cpp:28-34,80-93 ; FallingCoconutActor.cpp:135 ; IslanderHUD.cpp:137-157 ; grep abonnés (C++, Blueprints, maps) : 0
  - Recommandation : cue d'impact (son selon masse et vitesse), « −35 » flottant, craquement à la casse, sur chaque machine.
  - Effort : M
- **[Majeur] Aucun instrument de retour**
  - Constat : 8 systèmes jouent un son (pas, sabre, mousquet, voix tiki, noix, trompette, rivage, clics). Niagara est absent du `Build.cs` ; la fumée du mousquet est 7 `UStaticMeshComponent` animés à la main ; seul le mousquet bouge la vue, hors réglage §18.
  - Preuve : grep Source Niagara|CameraShake|ForceFeedback|TimeDilation : 0 ; Build.cs:11-23 ; MusketWeapon.cpp:545-586,686
  - Recommandation : `DA_FeedbackCues` (tag → son, Niagara, secousse) et `UFeedbackSubsystem::Play(Tag, Lieu, Intensité)` ; recette « 3 canaux par action clé ».
  - Effort : M
- **[Majeur] Retours de combat tardifs et perdables**
  - Constat : sons de touche et de kill, chiffres : RPC serveur non fiables ; le coup est prédit, sa confirmation arrive après l'aller-retour réseau (100–200 ms, supposé), ou jamais. Découpe sans éclats, sève ni hit-stop ; mousquet sans impact au point touché ; sabre « pas testé en jeu » (joueur touché : voir 04).
  - Preuve : CutlassWeapon.h:143-148 ; CutlassWeapon.cpp:287,453-465 ; MusketWeapon.cpp:316-343 ; board T-070
  - Recommandation : touche prédite chez l'auteur, kill fiable, éclats et hit-stop de 60 ms sur « TRANCHÉ ».
  - Effort : M
- **[Majeur] Récompenses sous-dimensionnées**
  - Constat : le +2:00, gain du sermon, est un texte de 18 pt, 3 s, sans son (voir 15). Le compteur d'argent est un nombre brut rafraîchi toutes les 0,5 s, sans défilement ni son de pièces.
  - Preuve : HudTimerRules.h (`NotificationFontSize`, `ZoneSecuredDuration`) ; STapaoHUD.cpp:93,255-262
  - Recommandation : bannière centrale, cloche d'équipe, compteur qui défile.
  - Effort : S
- **[Majeur] Naufrage, mort, victoire : texte de debug ou écran, pas un moment**
  - Constat : le naufrage s'annonce par `AddOnScreenDebugMessage`, sans effet en Shipping (corps du moteur sous `#if`, vérifié ; 13 le supposait), comme les erreurs de tirage d'île ; `OnSunk` n'a aucun abonné. La mort donne un ragdoll puis l'écran à 1,5 s, sans son ni ralenti ; « ÎLE CONQUISE » est muet ; `OnTeamWiped` n'a aucun abonné.
  - Preuve : BoatActor.cpp (`PlaySinking`) ; ExpeditionBootGameMode.cpp:121 ; UnrealEngine.cpp:12468 (UE_5.8) ; TapaoUISubsystem.cpp (`PollGame`)
  - Recommandation : message HUD réel, cue de naufrage (craquement, cloche), stingers de victoire et de défaite, ralenti bref.
  - Effort : M
- **[Majeur] Mixage à demi faux, aucune musique**
  - Constat : 98 sons sur 100 sont en `SC_Effects`, 2 en `SC_Ambience` : les curseurs Musique, Dialogues et Interface ne commandent rien. Hors rivage, ni vent, ni nuit, ni village, ni galion (sockets `bell`, `anchor_*` prêts).
  - Preuve : SSettingsMenu.cpp:250-255 ; classe de chaque `SW_*` (grep)
  - Recommandation : classer voix et clics ; boucles par phase, musique par couches.
  - Effort : S (classes), L (musique)
- **[Majeur] Sermon, arrimage, palan, vente, départ : retours non spécifiés**
  - Constat : ces moments sont absents de `main` ; `dev/T-054` et `dev/T-055` n'ajoutent que des délégués, 0 son, 0 effet ; T-112 et T-114 `a-faire`. Le cahier ne cite qu'un « son bref » (§19).
  - Preuve : git diff main...dev/T-054 et T-055 ; board.md:119,121
  - Recommandation : fiche de retours avant le code : chant et drapeau hissé, claquement d'arrimage, poulie du palan, tampon et pièces, cloche du départ.
  - Effort : S

## Manques
1. Cues d'impact des objets (valeur perdue comprise).
2. Musique, ambiances de phase, tempête, nuit.
3. Niagara de base : éclaboussures, sillage, poussière, éclats, lueur des butins.
4. Secousses, FOV de sprint, vibration manette, voix du joueur, filtre sous l'eau.
5. Options de confort §18.

## Quick wins
1. Trois sons dans `DA_HudTimerRules` (le widget les joue déjà).
2. `SC_Dialogue` pour les voix, `SC_Interface` pour les clics.
3. Abonner `OnValueChanged`/`OnBroken` : son et chiffre flottant.
4. Remplacer les deux `AddOnScreenDebugMessage` par un message HUD et un son.
5. Une secousse unique (coup reçu, atterrissage) par `ClientStartCameraShake`.

## Questions qui challengent l'équipe
1. Qui juge « ça sonne et ça frappe bien » ? Seuls les pas ont reçu un « ça marche » du propriétaire (T-071) ; un sound designer externe est-il prévu ?
2. Que voit-on quand on tranche un tiki « sans gore » : éclats de bois, sève, confettis ?
3. Quel moment ouvre la bande-annonce ? Si c'est le naufrage ou le +2:00, pourquoi sont-ils les plus muets ?
