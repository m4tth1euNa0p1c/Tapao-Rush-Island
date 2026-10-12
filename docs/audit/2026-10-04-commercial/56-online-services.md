# 56 — Services en ligne : sessions Steam, invitations, NAT, VoIP (L7)
Domaine : F · Auditeur : ingénieur services en ligne

## Verdict
L7 n'existe pas : le `.uproject` n'active ni Steam, ni EOS, ni voix, et hors LAN, jouer entre amis exige une IP et un port ouvert. Pile recommandée pour un jeu Windows vendu sur Steam : OnlineSubsystemSteam et SteamSockets (relais Valve), voix du moteur (`UVOIPTalker`), derrière un `USessionSubsystem` qui isole le fournisseur. Risque commercial : le cahier place L7 après une V1 « LAN/direct-IP » (§11), alors que jouer entre amis est le produit.

## Constats

- **[Bloquant commercial] Aucune brique en ligne, aucune tâche L7 au chantier**
  - Constat : `OnlineSubsystem` (Null) est actif par défaut mais jamais lié; aucune config en ligne; `GameInstance` du moteur; JOUER ouvre toujours `?listen`. Reste l'IP directe avec redirection de port (supposé hors de portée du grand public).
  - Preuve : monke_island.uproject:18-116; Build.cs:48-51; DefaultEngine.ini:14; TapaoUISkin.h:121; cahier §11 l.237-240; board.md (aucune ligne L7).
  - Recommandation : L7a (sessions, invitation, relais) avant le packaging L6, puis L7b (voix). Critère §11: réseaux distincts.
  - Effort : L (estimation: L7a 2–3 sem., L7b 1–2 sem.)

- **[Majeur] Pile: Steam seul en V1, EOS et Vivox écartés**
  - Constat : le moteur 5.8 livre OnlineSubsystemSteam et SteamSockets. SteamSockets relaie par défaut (SDR): ni redirection de port ni IP exposée; un hôte listen server ouvre une socket P2P; la connexion devrait survivre au seamless travel (supposé, voir 32). EOS (SDK 1.19.1): pas d'EOSPlus, donc deux identités à lier, et `JoinChannel` exige un jeton de salle. Vivox: absent du moteur. Piège: la page Epic « Online Subsystem Steam » décrit `SteamNetDriver`, sans traversée de NAT en 5.8.
  - Preuve : moteur BaseEngine.ini:2518; SteamSocketsNetDriver.cpp:83-95; EOSVoiceChatUser.cpp:575-586; SocketSubsystemSteamIP.uplugin; partner.steamgames.com/doc/features/multiplayer/networking; dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine.
  - Recommandation : `SteamSocketsNetDriver` avec repli `IpNetDriver`, derrière `ISessionBackend` (Null/LAN, Steam; OnlineServices v2, présent en 5.8, reste possible); EOS seulement pour Epic Games Store ou consoles (L8+). Le solo ne dépend jamais de Steam.
  - Effort : M

- **[Majeur] Code d'invitation: la recherche de lobby du moteur est régionale**
  - Constat : `FindSessions` Steam fige `k_ELobbyDistanceFilterDefault` (« même région ou voisines »): un code saisi par un ami lointain (Tahiti, France) risque de ne rien trouver. L'invitation passe par l'identifiant du lobby, sans ce filtre.
  - Preuve : moteur OnlineSessionAsyncLobbySteam.cpp:845; OnlineSessionAsyncLobbySteam.h:316-327; isteammatchmaking.h:44-47; IPAddressSteamSockets.h:63.
  - Recommandation : lobby `FriendsOnly` et invitation d'abord; « code » = SteamID hôte + PIN contrôlé en `PreLogin`, connexion directe `steam.<id>` sans recherche (supposé, à valider au spike).
  - Effort : M

- **[Majeur] Voix: proximité native, mais le §8 n'est pas couvert**
  - Constat : `UVOIPTalker` donne l'atténuation 3D; PTT par défaut; la voix suit la connexion de jeu, relayée par l'hôte. Le micro est le périphérique système: pas de choix sans code. `OnlineSubsystemNull` porte la même voix: prototype LAN à deux machines sans Steam. Plan B: `IVoiceChat` (EOS, Vivox), avec un second fournisseur.
  - Preuve : moteur VoiceConfig.h:46-128; BaseGame.ini:71; VoiceEngineImpl.cpp:363; OnlineSubsystemNull.cpp:319; VoiceChat.h:489.
  - Recommandation : `bHasVoiceEnabled`, `[Voice] bEnabled`; un `UVOIPTalker` par `APlayerState`, recréé après chaque voyage (supposé); atténuation par phase en DataAsset; micro de Windows en V1, Discord d'ici là (§8).
  - Effort : M (L avec sélecteur de micro)

- **[Majeur] Les six erreurs du §16 n'ont aucun signal câblé**
  - Constat : complet: `MaxPlayers` vaut 16 et le lobby suit `NumPublicConnections` (à fixer à 4); version: `ProjectVersion` absent (« 1.0.0 » partout); en expédition: `UpdateSession` avec `bAllowJoinInProgress=false` ferme le lobby, motif par `PHASE` et `PreLogin`; expirée: `ConnectionTimeout=80 s`, à régler; introuvable ou invitation périmée: `OnJoinSessionComplete`.
  - Preuve : moteur BaseGame.ini:68; NetworkVersion.cpp:90,223-262; OnlineSessionAsyncLobbySteam.cpp:447-450,657; BaseEngine.ini:2526.
  - Recommandation : table `EJoinError` ↔ signal moteur, un test par cas (NET01); voir 27 et 40.
  - Effort : M

- **[Mineur] Coûts: zéro serveur, chiffres non vérifiés**
  - Constat : ni backend ni hébergement (listen server, lobbies, relais Valve); SDK Steamworks sous contrat Valve. Non vérifiés (recherche web interrompue): frais Steam Direct (supposé 100 $), commission (supposé 30 %), gratuité du relais Valve et d'EOS, palier Vivox. L'AppID réel engage une publication: accord du propriétaire.
  - Preuve : Steamworks.tps; CLAUDE.md.
  - Recommandation : AppID réel avant les playtests externes; Crédits: Valve, Opus; RGPD: SteamID, voix non enregistrée.
  - Effort : S

## Manques
Dans l'ordre de mise en œuvre :
1. T-NNN L7a/L7b, ADR fournisseur, fiche `online-sessions.md`: rien n'existe.
2. `USessionSubsystem`: Null puis Steam, erreurs §16, `PreLogin`, hôte parti.
3. Invitation (overlay, démarrage à froid), code + PIN, écran « Rejoindre ».
4. Voix: PTT, mute, indicateur, atténuation.
5. Succès et cloud Steam: même plugin.

## Quick wins
1. Spike (≤ 1 j, éditeur fermé): plugins `OnlineSubsystemSteam` et `SteamSockets`, Build.cs, ini, AppID 480 (partagé, d'où une clé de filtre propre: supposé); log « Steam initialisé » en `-game`.
2. `open steam.<SteamID>` entre 2 PC, le 2e en partage 4G (CGNAT): valide relais, listen server et « code » (≤ 1 j).
3. Voix LAN (≤ 1 j): `UVOIPTalker` sur le pion, 2 PC (prototype du §8).
4. T-NNN L7a/L7b et ADR fournisseur (½ j).

## Questions qui challengent l'équipe
1. Qui fait le test à deux réseaux distincts (2e PC, compte Steam, 4G), et quand ?
2. La voix de proximité figure-t-elle sur la page Steam ? Sinon Discord suffit (§8): l'assumez-vous ?
3. Steam seul jusqu'en 1.0: renoncez-vous à Epic et aux consoles, et payez-vous l'AppID avant le premier playtest externe ?
