# Feature : Chat vocal de proximité — micro, voix 3D, bruit pour les mobs

- **Statut** : review. Le code est écrit et compilé fichier par fichier (UBT `-SingleFile`, détail au §9). La spec `monke_island.Voice` est écrite, **pas encore exécutée**. **Rien n'est testé en jeu** : ni en PIE solo, ni à 2 joueurs, ni sur deux machines.
- **Assets** : l'action `voicesetup` a créé `IA_PushToTalk`, `IMC_Voice` et `SC_VoiceChat` le 2026-10-09. `DA_VoiceRules` et le canal `VoiceChat` de `DA_SettingsRules` attendent le build complet : il faut relancer l'action après.
- **Owner** : `ue-network-engineer`
- **Date** : 2026-10-09
- **Cahier des charges** : lot L7 (§8 « voix en ligne », §18 onglet « Voix L7 », `security.md` §6), tâche T-263, demandée par le propriétaire avant le lot. La recette L7 (§ lots : « deux machines sur réseaux distincts rejoignent et communiquent ; mute et départ propres ») n'est **pas** passée : ce chantier couvre le LAN et l'hôte d'écoute, pas l'internet. Le bruit de la voix alimente la furtivité T-262 (`docs/features/furtivite.md`).

## 1. Intention
Demande du propriétaire (2026-10-09) :
> « ajoute aussi le fait de pouvoir parler in game, activation micro etc… cela va permettre de pouvoir signaler présence aux mobs la nuit »

- On parle aux coéquipiers proches. Une voix se perd avec la distance : pleine à 3 m, éteinte à 25 m. Elle sort de la tête du personnage qui parle.
- Parler est un risque : un joueur qui parle fait du bruit. Les mobs proches l'entendent, et de nuit ils ne repèrent les joueurs qu'au bruit (T-262).
- Le micro est sous contrôle :
  - **appuyer pour parler** (touche T maintenue), par défaut ;
  - **activation vocale** (micro ouvert, avec un seuil) ;
  - **coupé**.
- Un mort ne parle plus aux vivants.

## 2. Règles et valeurs
Les valeurs sont dans `DA_VoiceRules` (`UVoiceRules`, `/Game/MonkeIsland/Voice/Data/`). Les choix du joueur sont des réglages (`DA_SettingsRules`, `USettingsSubsystem`).

| Paramètre | Défaut | Où |
|---|---|---|
| Mode du micro | appuyer pour parler | réglage (`DefaultMicMode` de `DA_SettingsRules`) |
| Touche | T (maintenir) | `IMC_Voice`, priorité 10 (`MappingPriority`) |
| Seuil d'activation vocale | 8 % (niveau 0,08) ; plage 1 % à 40 % | réglage (`DefaultMicThreshold`, `MicThresholdRange`) |
| Seuil en appuyer-pour-parler | 0,04 : seuls le silence et le souffle restent à la maison | `PushToTalkThreshold` |
| Voix pleine jusqu'à | 300 cm de la tête | `InnerRadiusCm` |
| Voix éteinte à partir de | 2 500 cm | `MaxDistanceCm` |
| Courbe | son naturel, -60 dB au bout, silence au-delà | `DistanceModel`, `AttenuationAtMaxDb` (ou un `ATT_` dans `AttenuationOverride`) |
| Point d'émission | os ou socket `head` du maillage, sinon racine + (0, 0, 60) | `HeadSocketName`, `HeadFallbackOffset` |
| Volume des voix | 80 % | réglage, canal `ESettingsVolume::VoiceChat` → `SC_VoiceChat` |
| Rayon du bruit d'une voix | 0 = celui de `DA_StealthRules` (1 800 cm) | `VoiceNoiseRadiusCm` |
| Cadence du bruit | un bruit tout de suite, puis toutes les 0,5 s tant que le joueur parle | `NoiseIntervalSeconds` |
| Rapport « je parle » du client | au plus un toutes les 0,25 s, le dernier état part toujours | `ClientMinSendIntervalSeconds` |
| Limite serveur des « je parle » | 6 d'affilée, 3 rendus par seconde ; « je me tais » toujours accepté | `ServerTalkBurst`, `ServerTalkRefillPerSecond` |
| Période des mises à jour | 0,1 s | `UpdateIntervalSeconds` |
| HUD | 3 noms au plus, 18 caractères | `MaxNamesShown`, `MaxNameChars` |

**Quand le micro part** (`VoiceMath::ShouldTransmit`, testé) :
- en appuyer-pour-parler : tant que la touche est tenue ;
- en activation vocale : toujours ;
- coupé : jamais ;
- mort : jamais ;
- pendant le test du micro : toujours (rien ne quitte la machine, voir §4).

Le moteur n'envoie que la voix plus forte que le seuil (sa détection de silence, `voice.SilenceDetectionThreshold`, et sa porte de bruit, réglées au même niveau).

**Quand le joueur « parle »** (`VoiceMath::IsTalking`) : le moteur entend sa voix pendant qu'elle part. C'est ce qui fait du bruit pour les mobs et affiche « Vous parlez ». Jamais pendant le test du micro, jamais mort.

**HUD**, en bas à gauche au-dessus de la ligne de furtivité :
- « Micro ouvert » dès que le micro part (`security.md` §6) ;
- « Vous parlez » (doré) quand on est entendu ;
- « Test du micro : vous seul vous entendez » pendant le test ;
- « Mort : votre voix ne porte plus » si un mort essaie de parler ;
- au-dessus, « Nom parle » pour chaque joueur entendu à moins de 25 m, le plus proche d'abord.

## 3. Réglages (onglet « Voix »)
Le menu Réglages (F10, ou OPTIONS du menu TAPAO) a un 5e onglet, « Voix » :
- **Micro** : appuyer pour parler (maintenir T), activation vocale (micro ouvert), micro coupé.
- **Seuil d'activation vocale** : actif en activation vocale seulement. On l'entend en direct pendant le test.
- **Volume des voix** : préécouté, comme les autres volumes.
- **Tester le micro** : la voix du joueur lui revient, personne d'autre ne l'entend, et les mobs non plus. Le test s'arrête en fermant le menu. Il est indisponible en Shipping (la boucle locale du moteur n'y existe pas).

Appliquer enregistre le mode et le seuil dans `GameUserSettings.ini` (`UMonkeUserSettings`, par le système existant), avec le volume dans le tableau des volumes. Annuler et « Restaurer cet onglet » marchent comme pour les autres onglets.

## 4. Réseau (hôte d'écoute, 1 à 4 joueurs)
Toute la voix passe par la **VoIP intégrée du moteur** :
- module `Voice` : capture, Opus et lecture ;
- OnlineSubsystem **NULL** ;
- canal voix du `NetDriver` : un client envoie à l'hôte, et l'hôte relaie aux autres.

Aucun son n'est traité à la main.

- **Session vocale locale.** La voix du moteur ne tourne que dans une session en ligne. Or le jeu héberge (`?listen`) et rejoint par adresse, sans session. `UVoiceChatSubsystem` (un par monde de jeu) crée donc sur chaque machine qui joue une session NULL `MonkeVoice` :
  - non annoncée, personne ne la rejoint ;
  - elle y inscrit chaque joueur de `PlayerArray`, d'où le talker distant et l'état « parle » ;
  - elle est détruite à la fin du monde, ce qui libère le micro.

  Elle est créée aussi en solo, pour le bruit de la voix et le test. Sur une autre plateforme (Steam ou EOS plus tard, L7), le chat vocal utilise la session de jeu de cette plateforme.
- **Voix 3D.** Pour chaque joueur distant, un `UVOIPTalker` et une ancre sont posés sur son `PlayerState`, côté local :
  - l'ancre suit l'os `head` de son pion ;
  - l'atténuation est celle de `DA_VoiceRules` ;
  - sans pion (disparu, ou trop loin pour être pertinent), l'ancre est garée à 10 km sous l'île et on ne l'entend plus.
- **Qui décide.**
  - **Le client** dit seulement « je parle / je me tais » : `ServerSetTalking(bool)`, fiable, sur un composant du `PlayerController` (`UVoiceChatComponent`, donc un acteur possédé). Il l'envoie à chaque changement, au plus toutes les 0,25 s.
  - **Le serveur** passe ce rapport dans la limite de débit (§5), puis décide :
    - tant que le joueur est vivant et parle (son rapport, ou la voix que l'hôte entend venir de lui), il appelle `UStealthSubsystem::ReportNoise(Pawn, Pawn->GetActorLocation(), rayon, EStealthNoise::Voice)` : une fois tout de suite, puis toutes les 0,5 s ;
    - le rayon et la position viennent du serveur, jamais du client.
- **Les morts.** Tant que le pion d'un joueur est mort (`UIslanderHealthComponent::IsDead`), le serveur le coupe pour tous les autres (`GameplayMutePlayer`) : il ne relaie plus sa voix, et chaque machine, l'hôte compris, le coupe aussi chez elle. Quand le joueur revit, la coupure est levée, et aussi quand il quitte la partie. Un mort ne fait pas de bruit.
- **Le moteur ne décide pas du micro.** L'appel du moteur à la connexion (`ClientEnableNetworkVoice`) et la commande `ToggleSpeaking` passent par `Amonke_islandPlayerController::ToggleSpeaking`, qui garde le mode du joueur. `bRequiresPushToTalk=true` (DefaultGame.ini).
- **Rien n'est répliqué en continu.** Le composant ne réplique aucune propriété : il est répliqué seulement pour porter son RPC. Le volume réseau de la voix vient du moteur : Opus à 16 kHz, quelques Ko/s par personne qui parle.
- **Dédié.** Un serveur dédié ne crée pas de session vocale et ne capte rien. Seule la moitié serveur des composants y tourne (le bruit d'après le rapport, la coupure des morts).

## 5. Sécurité (`security.md`)
- **RPC `ServerSetTalking`** :
  - appelable seulement par le client propriétaire du `PlayerController` ;
  - argument booléen, sans valeur impossible, donc pas de `_Validate` ;
  - **limite de débit** : seau de jetons par joueur (6, +3/s), avec un `Warning` au plus une fois par seconde. « Je me tais » est toujours accepté, donc un rapport refusé ne laisse jamais un joueur « parlant » ;
  - le bruit reste borné à un rapport toutes les 0,5 s par joueur, quoi qu'envoie le client ;
  - le pion et sa vie sont revérifiés côté serveur.
- **Aucune donnée audio traitée à la main** : capture, encodage, relais et lecture sont ceux du moteur. Seul le test du micro (et une partie sans réseau) vide à chaque image le tampon sortant du moteur avant l'envoi réseau. C'est ce qui garantit que rien ne sort.
- **Vie privée** (§6) :
  - appuyer-pour-parler par défaut ;
  - indicateur dès que le micro part ;
  - micro fermé hors mode et à la fin du monde (session détruite) ;
  - aucun pseudo dans les logs, seulement les noms d'objets moteur. `monke.Voice.Dump` ne sort que des compteurs.
- **Shipping** : `monke.Voice.Test`, `monke.Voice.FakeTalk` et `monke.Voice.Dump` sont compilés hors Shipping. La boucle locale du moteur (`OSS.VoiceLoopback`) aussi.
- **Limite connue du moteur** (risque accepté en coop privée, à revoir pour l'internet) : l'hôte ne vérifie pas l'expéditeur inscrit dans un paquet de voix. Un client modifié pourrait donc parler sous l'identité d'un autre, ou contourner une coupure. Sur une session internet (L7), cela relèvera de la VoIP de la plateforme (EOS Voice ou Steam).

## 6. Tester

### Après le build complet (éditeur fermé, par le lead)
1. Build complet : `Build.cs` et `.uproject` ont changé (§9).
2. Relancer l'éditeur, puis l'action `voicesetup` : elle crée `DA_VoiceRules` et le canal `VoiceChat` de `DA_SettingsRules`. Le résultat est aussi écrit dans `Saved/T186/voice-setup.json`.
3. Tests : `scripts/ue.ps1 test -Filter monke_island.Voice`, puis `monke_island.Settings` (non-régression des réglages).

### Seul (PIE solo ou « Standalone »)
- **Entendre sa voix** : F10 → Voix → « Tester le micro » (ou la console `monke.Voice.Test 1`). On parle et l'on s'entend. Le HUD dit « Test du micro ». `monke.Voice.Test 0` arrête.
  - La commande brute du moteur reste `OSS.VoiceLoopback 1`, mais elle diffuse aux autres en réseau.
- **Bruit sans micro** : `monke.Voice.FakeTalk 1`, puis tenir T près d'un mob la nuit. Le HUD dit « Vous parlez », et le mob vient voir.
  - Avec `monke.Stealth.Debug 1` (T-262), les bruits `Voice` sont tracés toutes les 0,5 s.
  - Pour forcer la nuit : `monke.Stealth.Night 1`.
- **Vrai micro** :
  - tenir T et parler : « Micro ouvert », puis « Vous parlez », et le mob vient ;
  - T tenu en silence : « Micro ouvert » seulement, pas de bruit ;
  - en activation vocale : le micro reste ouvert et le seuil coupe le silence.
- **État** : `monke.Voice.Dump` affiche la session, l'indicateur et les compteurs. `LogMonkeVoice` est en `Log`. `log LogMonkeVoice Verbose` affiche les changements d'état.

### À deux (PIE, `Play As Listen Server`, 2 joueurs)
1. Mode réseau « Écouter le serveur », 2 joueurs, « Run Under One Process » coché.
2. Fenêtre hôte : tenir T et parler.
   - Hôte : « Vous parlez ».
   - Client : « <Hôte> parle » en bas à gauche, et la voix vient de la tête de l'hôte : plus faible en s'éloignant, rien au-delà de 25 m.
3. Même essai dans l'autre sens.
4. Tuer l'un des deux (chute ou mob). L'autre ne l'entend plus. À la réapparition, on l'entend de nouveau.
5. Mode « Micro coupé » : rien ne part, aucun indicateur.
6. Réseau dégradé (`Net PktLag=200`, `Net PktLoss=3`) : la voix arrive avec du retard ou des trous, mais le bruit côté serveur suit toujours la parole, sans état bloqué.

Logs à relever : `LogMonkeVoice`, `LogOnlineVoice`, `LogVoiceEngine`, `LogNet`.

**Pièges du PIE** :
- Les deux fenêtres captent le même micro. Seule la fenêtre active reçoit T : rester en appuyer-pour-parler pour savoir qui parle.
- Le son sort par la fenêtre qui a le focus.
- La table des talkers du moteur est commune à tout le processus. À 3 joueurs ou plus dans un seul processus, une voix peut ne pas être spatialisée (un message `LogMonkeVoice` le signale). Tester 3–4 joueurs avec « Run Under One Process » décoché, ou sur plusieurs machines.
- En PIE à plusieurs fenêtres, le retour du test peut sortir dans l'autre fenêtre : faire le test seul.

### Deux machines (LAN)
- L'hôte : JOUER → HÉBERGER. L'autre : REJOINDRE avec l'IP de l'hôte (port 7777 ouvert dans le pare-feu de l'hôte). Refaire les essais ci-dessus.
- C'est la seule preuve « multi-machines ». La recette internet du L7 demande des réseaux distincts et une solution de session (EOS ou Steam), hors de ce chantier.

## 7. Fichiers
- `Source/monke_island/Voice/` :
  - `VoiceTypes.h` : `LogMonkeVoice`, `EVoiceIndicator` ;
  - `VoiceRules.h/.cpp` : `UVoiceRules` ;
  - `VoiceMath.h/.cpp` : les règles pures, le limiteur, l'étrangleur d'envoi et la cadence du bruit ;
  - `VoiceChatComponent.h/.cpp` ;
  - `VoiceChatSubsystem.h/.cpp` : la session vocale, les talkers, le HUD, la console ;
  - `Tests/VoiceSpec.cpp`.
- Accroches :
  - `monke_islandPlayerController.h/.cpp` : sous-objet `VoiceChat` et surcharge de `ToggleSpeaking` ;
  - `UI/Tapao/STapaoHUD.cpp` : deux lignes en bas à gauche.
- Réglages :
  - `Settings/SettingsRules.h/.cpp` : `EVoiceMicMode`, le canal `VoiceChat` et les défauts du micro ;
  - `Settings/MonkeUserSettings.h` ;
  - `Settings/SettingsMath.h` ;
  - `Settings/SettingsSubsystem.h/.cpp` : délégués `OnVoiceSettingsChanged` et `OnMicTestChanged`, `PreviewVoice`, `SetMicTest` ;
  - `Settings/SSettingsMenu.h/.cpp` : onglet Voix.
- Réglages ne dépend pas de `Voice/`. C'est `Voice/` qui écoute ses délégués.
- Configuration :
  - `monke_island.Build.cs` : `OnlineSubsystem`, `OnlineSubsystemUtils` ;
  - `monke_island.uproject` : greffons `OnlineSubsystem`, `OnlineSubsystemNull` et `OnlineSubsystemUtils`, déjà actifs par défaut et désormais listés ;
  - `DefaultEngine.ini` : `[Voice] bEnabled=true` ; `[OnlineSubsystem] DefaultPlatformService=Null`, `bHasVoiceEnabled=true` ; `[/Script/Engine.AudioSettings] VoiPSoundClass=SC_VoiceChat` ;
  - `DefaultGame.ini` : `[/Script/Engine.GameSession] bRequiresPushToTalk=true`, et `/Game/MonkeIsland/Voice` dans `DirectoriesToAlwaysCook`.
- Script éditeur : `scripts/editor/voice_setup_ue.py` (action `voicesetup`).
- Dépendances et licences : uniquement des modules et greffons du moteur Unreal (Epic, licence du moteur ; Opus est fourni avec le moteur). Aucun greffon tiers, rien de payant.

## 8. Limites connues et reste à faire
- **Touche V en conflit.** V servait déjà à la fanfare (`IA_TrumpetEmote` dans `IMC_TrumpetEmote`) et au changement de vue (`IA_ToggleView`, désactivé). `IMC_Voice` passe devant et consomme V : **la fanfare n'a plus de touche**. Deux choix :
  - le propriétaire d'`Emotes/` la déplace sur une autre touche ;
  - on change `PUSH_TO_TALK_KEY` dans `voice_setup_ue.py`.

  L'action liste les conflits qu'elle trouve.
- **Pas encore fait** (cahier §8, §18) :
  - mute individuel d'un joueur (le moteur l'offre : `MuteRemoteTalker`, `ServerMutePlayer`) : il manque une liste des joueurs à l'écran ;
  - choix du périphérique d'entrée et de sortie ;
  - canal équipage au hub ;
  - remappage de la touche.
- **Portée.** Toute la voix est relayée à tous, et seule l'atténuation la rend locale. Le serveur ne filtre pas par distance. C'est sans effet sur 4 joueurs, mais à revoir si la bande passante compte.
- **Volume du bruit.** Le bruit ne dépend pas du volume de la voix : chuchoter ou crier porte au même rayon. Le moteur transmet l'amplitude des clients, mais pas celle de l'hôte, d'où cette égalité volontaire.
- **Session internet** (L7) : rien ici. Ce sera la VoIP et la session de la plateforme retenue.

## 9. Preuves
- **Compilé** (2026-10-09, UBT `-SingleFile`, éditeur ouvert, sans erreur ni warning) :
  - `Voice/` : `VoiceMath`, `VoiceRules`, `VoiceChatComponent`, `VoiceChatSubsystem`, `Tests/VoiceSpec` ;
  - `monke_islandPlayerController` ;
  - `Settings/` : `SettingsRules`, `MonkeUserSettings`, `SettingsSubsystem`, `SSettingsMenu`, `Tests/SettingsSpec` ;
  - `UI/Tapao/STapaoHUD`.

  UHT a accepté les nouveaux en-têtes. Il n'y a **pas eu d'édition de liens** : le build complet reste à faire, avec la prise en compte de `Build.cs` et du `.uproject`.
- **Action `voicesetup`** (2026-10-09) : `IA_PushToTalk`, `IMC_Voice` (V) et `SC_VoiceChat` sont créés. Conflits relevés sur V : `IMC_TrumpetEmote` (`IA_TrumpetEmote`) et `IMC_Islander_Actions` (`IA_ToggleView`, inactif). L'action échoue proprement sur `DA_VoiceRules` et le canal `VoiceChat`, qui attendent le build.
- **Non testé** : les specs, le PIE solo, le PIE à 2 joueurs et les deux machines.

## 10. Journal
- 2026-10-09 : T-263. Voix de proximité sur la VoIP du moteur : modes, réglages, HUD, bruit pour la furtivité, coupure des morts, sécurité, spec et doc. Code compilé fichier par fichier.

## Arbitrage de la touche (lead, 2026-10-09)
Appuyer pour parler passe sur **T** : V reste la fanfare de trompette (`IA_TrumpetEmote`), comme avant la voix. T n'était lié à aucune action (relevé de toutes les touches des contextes : `scripts/editor/list_input_keys_ue.py`, action `inputkeys`). L'action `voicesetup` remappe `IMC_Voice` (elle retire d'abord les anciennes touches de `IA_PushToTalk`).
