# 12 — Fonctionnalités du genre absentes
Domaine : B · Auditeur : analyste du genre (extraction coop)

## Verdict
Crownfall reprend la surface mécanique de Lethal Company, R.E.P.O. et Content Warning (T-031, T-033, T-034), mais aucune de leurs couches sociales : invitation, voix de proximité, monstres à règle, outils d'information, spectateur, cosmétiques. Le cœur du genre, vendre au quota (§24.4), n'existe pas non plus dans `main` (calcul pur seulement ; voir 11). Risque principal : un jeu coop que quatre amis ne peuvent ni rejoindre ni parler dedans.

## Constats
- **[Bloquant commercial] Ni invitation Steam ni voix de proximité**
  - Constat : JOUER lance toujours `listen` : ni bouton Rejoindre, ni OnlineSubsystem, ni voix. Le cahier les classe « secondaires comme demandé », après une V1 LAN/direct-IP. R.E.P.O., Content Warning et Peak affichent la voix de proximité sur Steam ; Lethal Company et Peak ont gagné le Steam Award « Better With Friends ».
  - Preuve : `STapaoScreens.cpp:143-148`, `TapaoUISkin.h:121`, `monke_island.Build.cs:49`, cahier §11, §23.
  - Recommandation : avancer L7 avant L6 : spike Steam (AppID de test 480), session privée, bouton Rejoindre, puis voix positionnelle (EOS Voice, supposé).
  - Effort : L
- **[Majeur] Aucun monstre « à règle »**
  - Constat : Coco, Crabe et Cendres sont un seul `ATikiWarrior` (même IA de mêlée) ; Rôdeur à sarbacane modélisé, non importé ; le requin est un acteur posé à la main, sans spawner. Lethal Company : 9+ créatures à scanner ; DRG : 50+ ennemis. Le joueur démarre armé : rien à fuir.
  - Preuve : `TikiWarrior.h:11-31`, characters-tiki.md:11, shark.md:9, board T-069/T-070/T-099.
  - Recommandation : 3 monstres à règle lisible en DataAsset (ex. : inerte tant qu'on le regarde ; attiré par la torche) ; un requin par fragment.
  - Effort : M
- **[Majeur] Aucun outil d'information ni de coordination**
  - Constat : HUD sans carte, boussole, marqueur de zone ni ping. Le cahier prévoit carte et pings (§19), pas de scanner, radio ni longue-vue ; le galion n'a ni terminal ni vigie. Lethal Company a scanner, terminal, talkies-walkies ; DRG des pings.
  - Preuve : `STapaoHUD.cpp:106-160` ; sprites `compass_bar` et `minimap_frame` dans `DA_TapaoUISkin`, aucun widget.
  - Recommandation : boussole et ping répliqué (palliatif de la voix) ; puis vigie à longue-vue et « registre des créatures » du Percepteur (scanner satirique).
  - Effort : M
- **[Majeur] Le mort n'a rien à regarder**
  - Constat : mort définitive, entrée coupée, ragdoll jusqu'à la fin de l'expédition ; `Respawn()` « provisional » ; aucun spectateur malgré le cahier. Standard du genre (supposé).
  - Preuve : `IslanderCharacter.cpp:442`, `ExpeditionRules.h:55-57`, cahier §4, §21.
  - Recommandation : `ASpectatorPawn` qui suit un vivant ; le mort garde le ping.
  - Effort : S
- **[Majeur] Véhicule partagé sans rôles actifs**
  - Constat : un seul rameur, les autres restent debout ; galion immobile, traversée = écran de chargement ; barre, palan, balance non codés. Dans Sea of Thieves, tout l'équipage manœuvre le navire.
  - Preuve : `BoatRules.h:168-171`, cahier §24.3, galleon.md « Hors V1 », board T-112 à T-116.
  - Recommandation : deux rameurs ; barre, vigie et palan en parallèle.
  - Effort : M
- **[Majeur] Contenu trop étroit pour le prix du genre**
  - Constat : un seul décor (20 fragments de 15 à 80 ha), 3 mêlée + 1 requin, 3 armes ; hache et lanterne n'existent qu'en prix. Lethal Company : 7+ lieux, 9,75 € ; Peak : 4 biomes, carte quotidienne, 7,49 € ; DRG : 11 biomes. L'effort est allé à l'échelle (atoll 1:1), pas aux règles.
  - Preuve : `fragments-tuamotu.md:39-40`, `EconomyRules.cpp:7-8`, pages Steam.
  - Recommandation : plancher d'accès anticipé : 5 monstres, 12 outils achetables, 4 modificateurs de manche ; geler l'extension géographique.
  - Effort : L
- **[Mineur] Ni outil de clip ni rétention sociale**
  - Constat : une emote (trompette), ni mode photo ni replay (absents du cahier), skins à faire, aucune intégration Steam. Content Warning : caméra de tournage, boutique de cosmétiques ; Peak : badges, cosmétiques ; DRG : succès, cartes, cloud.
  - Preuve : `Emotes/`, board T-028, pages Steam.
  - Recommandation : 6 cris vocaux, mode photo, 10 succès Steam, tricorne débloquable.
  - Effort : M

## Manques
Par priorité (socle du genre : 1 à 5) :
1. Invitation Steam, session privée, voix de proximité.
2. Vente au quota jouable (voir 11).
3. Monstres à règle ; requin par fragment.
4. Ping, boussole, longue-vue, registre des créatures, spectateur.
5. Outils achetables, modificateurs de manche.
6. Graine du jour, mode photo.
7. Cosmétiques, succès et cloud Steam.

Sources : https://store.steampowered.com/app/1966720 · https://store.steampowered.com/app/3241660 · https://store.steampowered.com/app/2881650 · https://store.steampowered.com/app/3527290 · https://store.steampowered.com/app/548430 · https://store.steampowered.com/app/1172620

## Quick wins
1. Caméra spectateur : suivre le vivant suivant (flèches).
2. Ping minimal : touche, RPC serveur, marqueur répliqué 6 s.
3. Boussole : câbler `compass_bar` (barque, galion).
4. Graine du jour : dériver de la date le `Seed` de `PickRandomIsland` (`ExpeditionIslandSubsystem.cpp:472`).

## Questions qui challengent l'équipe
1. Quel groupe d'amis achète un jeu coop où l'on ne peut ni s'inviter ni se parler, et pourquoi L7 vient-il après L6 ?
2. Quel monstre ou objet les joueurs nommeront-ils sur un forum ? Trois tiki de mêlée à l'IA commune n'y suffiront pas.
3. À 7,49 € (Content Warning, Peak), qu'apporte à l'acheteur un atoll de 29 × 26 km plutôt que des monstres à règle ?
