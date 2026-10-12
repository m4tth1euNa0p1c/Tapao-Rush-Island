# 83 — Potentiel streamers et créateurs
Domaine : I · Auditeur : expert en marketing d'influence

## Verdict
Crownfall a les moteurs physiques du clip (culbute, noix, naufrage), mais de vos six moments deux existent dans `main`, aucun n'a de coupable ni de légende, et quatre amis ne peuvent ni s'inviter ni masquer leur IP. Les créateurs lancent le genre et amplifient un coop cassé aussi vite qu'un bon moment. Risque : une première vague de streamers sur un multijoueur jamais testé à deux machines.

## Constats
- **[Bloquant commercial] Une équipe de streamers ne peut pas jouer**
  - Constat : hors LAN, rejoindre exige une IP et le Shipping coupe `open` (27, 56) ; aucun mode streamer. Le genre naît chez eux : Lethal Company domine Steam et Twitch ; Content Warning explose avec les streamers ; Peak (1 M en 6 jours) et R.E.P.O. (1,5 à 3,1 M en 3 semaines, estimations) suivent, rôle des streamers supposé.
  - Preuve : https://afkgaming.com/gaming/general/lethal-company-dominates-steam-twitch-to-become-the-best-steam-release-of-2023, https://www.pcgamesn.com/content-warning/player-count, https://www.cgmagonline.com/news/steam-peak-sells-1-million-copies/, https://wnhub.io/news/indie/item-47358, `Build.cs:48-51`.
  - Recommandation : aucune clé avant « 4 amis, invitation Steam, 2 réseaux, 30 min stables » ; SteamSockets, IP masquées (56) ; mode streamer : masque code d'invitation et noms Steam, coupe la voix des inconnus.
  - Effort : L

- **[Majeur] Vos six moments : 2 présents, 1 partiel, 3 absents**
  - Constat : naufrage codé, annoncé par un message de debug ; tiki qui bondit en rugissant, sans préavis (contraire à §8) ; sermon : 0 occurrence dans `Source/` (T-054 reste sur `dev/T-054`) ; abandon et défaite `a-faire` (défaite actuelle : ruban et 3 compteurs).
  - Preuve : `BoatActor.cpp:959`, `TikiWarrior.cpp:82-83`, T-054, T-112, T-114, `TapaoUISubsystem.cpp:511-513`.
  - Recommandation : par coût par clip : naufrage audible (craquements, cloche) ; défaite en scène de 8 s (noms, tampon) ; sermon et abandon : nommer le fautif ; une légende plein cadre par moment (TikTok, muet : supposé).
  - Effort : M

- **[Majeur] Le bean catapulté n'a aucun déclencheur**
  - Constat : seuls la culbute X, la noix (15 % de sa vitesse) et le crash en vol le mettent en ragdoll (hors mort) ; coup de tiki, morsure, vent, naufrage (+3 m/s) ne font que pousser. La caméra passe pourtant en 3e personne pendant le ragdoll : le plan est gratuit, la cause manque. Baril de poudre : modélisé, non intégré (17).
  - Preuve : `IslanderCharacter.cpp:350,556`, `CoconutFallRules.h:116`, `TikiWarrior.cpp:208`, `SharkActor.cpp:312`, `WindStormActor.cpp:356`, `BoatActor.cpp:934`.
  - Recommandation : `Knockout` sur naufrage et fortes rafales en terrain exposé (abri déjà calculé) ; baril à mèche (saisie et lancer existent), souffle non létal : le slapstick se clippe, la mort fait fuir (28).
  - Effort : M

- **[Majeur] Personne n'est coupable : rien ne se raconte**
  - Constat : `UValuableComponent` ignore qui tenait l'objet ; `OnValueChanged` et `OnSunk` n'ont aucun abonné (24) ; la valeur n'est lue que dans le prompt de qui tient ou vise l'objet : à quatre caméras, la plupart des points de vue ne voient pas la Perle s'abîmer.
  - Preuve : `ValuableComponent.cpp:48-69`, `IslanderHUD.cpp:139,157`.
  - Recommandation : `LastHolder` et compteurs par joueur (valeur perdue, noix, coups, abandons) ; étiquette de valeur visible de tous ; « Rapport d'incidents du Percepteur » à la vente, image partageable (18).
  - Effort : M

- **[Majeur] Voix de proximité : pas indispensable aux premiers créateurs, requise avant l'accès anticipé**
  - Constat : les streamers ont déjà Discord (supposé) : la voix ne rend pas leurs vidéos audibles. Elle apporte le jeu sur le silence et le bruit (supposé) et les parties avec inconnus ; la presse la cite pour R.E.P.O. Coût modéré : `UVOIPTalker` natif (56). Le bean n'a ni bouche ni cri.
  - Preuve : https://www.pcgamesn.com/repo/viral-horror-game, cahier §8 l.185, `character-bean-soldier.md:18`, `TrumpetEmoteRules.h:71`.
  - Recommandation : sessions Steam, puis pings, roue d'emotes et trompette qui attire les tiki (74), puis voix PTT ; yeux du bean qui s'écarquillent avec le volume.
  - Effort : M

- **[Mineur] Outils créateurs : le « replay » est déjà chez Valve**
  - Constat : `ISteamTimeline` pose marqueurs, priorité « Featured » et « phases de partie » (une manche) ; le SDK v1.64 livré avec UE 5.8 le contient (vérifié). Aucun module Steam dans le projet.
  - Preuve : https://partner.steamgames.com/doc/api/ISteamTimeline, `ThirdParty/Steamworks/Steamv164/sdk/public/steam/isteamtimeline.h`.
  - Recommandation : ni replay, ni mode photo, ni API Twitch avant 1.0 ; après le spike Steam (56) : un marqueur par moment signature, une phase par manche, des succès nommés d'après eux ; pseudos du chat en noms de villageois (fichier) ; garder la bande-son sans Content ID.
  - Effort : S

## Manques
Invitation Steam, noms, couleurs (56, 18) ; attribution et rapport d'incidents ; mode streamer ; déclencheurs de ragdoll ; bean qui crie, yeux animés ; spectateur utile (28) ; voix PTT.

## Quick wins
1. Bruissement 0,8 s avant le bond du tiki : retarder `LaunchCharacter` (`TikiWarrior.cpp:82`).
2. `LastHolder` dans `UValuableComponent`, journal « Perle abîmée par X ».
3. Six clips de 30 s (naufrage, noix, tiki, culbute, découpe) montrés à 5 inconnus : chronométrer le premier sourire.

## Questions qui challengent l'équipe
1. Combien d'inconnus ont ri devant ce build, et en combien de secondes ?
2. Quel créateur reçoit la première clé, avec quels trois amis, sur quels réseaux, et qui a testé cette session avant ?
3. Un spectateur étranger comprend-il le naufrage sans lire le français ni entendre un cri ?
