# 08 — Interdépendance, rôles émergents et communication coop
Domaine : A · Auditeur : designer coop

## Verdict
Les briques physiques de coopération existent (poids des joueurs qui fait gîter la barque, saisie à plusieurs, vote de départ), mais aucun rôle n'est différencié : même kit pour tous, butin trop léger pour exiger un porteur, sermon protégé injouable. Rien n'a été joué à deux humains. Risque commercial : un coop « additif », sans signal ni voix, donc sans moment de groupe racontable.

## Constats

- **[Bloquant commercial] Le sermon n'est pas jouable**
  - Constat : T-054 vit dans la branche `dev/T-054`, non fusionnée ; ni RPC joueur ni Blueprint ; `AddZoneBonus` n'a aucun appelant, donc aucun +120 s. L'officiant n'existe pas en jeu.
  - Preuve : board.md:61 ; journal.md:132 ; `git merge-base` négatif ; grep `AddZoneBonus`.
  - Recommandation : fusionner et brancher, puis playtest à deux humains avant tout contenu neuf.
  - Effort : M

- **[Majeur] Le sermon est injuste pour l'équipe**
  - Constat : tout dégât remet à zéro, noix comprise. Une noix tombe toutes les 7 s au calme, 1,2 s sous tempête, parfois visée sur la tête : l'officiant immobile est la cible idéale et les protecteurs n'y peuvent rien (cahier §8).
  - Preuve : T-054 PacificationRules.h:50, PacificationZone.h:271 ; CoconutFallRules.h:28-48 ; storm-flood.md:55-58.
  - Recommandation : aucune chute à moins de 10 m du drapeau ; arbitrer une reprise partielle (un coup retire 5 s).
  - Effort : S

- **[Majeur] Aucun rôle différencié**
  - Constat : chaque bean démarre avec mousquet, sabre et torche ; tir ami coupé. Les « rôles » du cahier (§6.3) sont des tâches de 2 à 20 s. Le quota monte de 25 % par joueur seulement : 250 en solo, 109 par tête à quatre, donc on peut s'y cacher.
  - Preuve : musket.md:57 ; cutlass.md:68 ; journal.md:238 ; cahier §24.6.
  - Recommandation : pas de classes : une lanterne d'équipage en boutique plutôt qu'une torche par joueur.
  - Effort : S, à arbitrer

- **[Majeur] Pas de porteur : le butin pèse des grammes**
  - Constat : butin commun de 0,03 à 1,2 kg pour 1-6 couronnes ; perle 9 kg. La saisie à plusieurs (25 kg par joueur) est codée, mais aucun objet de valeur ne l'exige ; le cadavre de 40 kg ne vaut rien. Barque 500 kg : jamais de choix « quoi laisser ».
  - Preuve : loot-common.md:8-21 ; IslanderGrabComponent.h:46-52 ; characters-tiki.md:122.
  - Recommandation : 4 à 6 objets lourds et fragiles (idole, cloche), 150-400 couronnes, à 2-4 porteurs.
  - Effort : M

- **[Majeur] Le rameur est un taxi solitaire**
  - Constat : un seul siège ; tant que quelqu'un rame, nul ne peut le relever. Rameur sans mouvement ni arme, passagers sans tâche (requin et embuscades ignorent la barque). Doc : 1,6 m/s ; l'asset a été réglé à 11 m/s.
  - Preuve : BoatRules.h:168-171 ; BoatActor.cpp:249,323 ; BoatRiderComponent.cpp:261-271 ; shark.md:14 ; characters-tiki.md:147 ; commit c5fe989.
  - Recommandation : une rame par joueur (OarLeft/OarRight existent), écopage manuel en surcharge.
  - Effort : M

- **[Majeur] Palan et barre : des boutons solitaires**
  - Constat : palan (3 s) et barre (2 s) sont des maintiens d'un seul joueur ; le départ (préavis 10 s, annulable à la barre) la spec n'affiche pas les votants. T-112 reste à faire : conception libre.
  - Preuve : cahier §24.2 ; board.md:119.
  - Recommandation : cabestan à deux (3 s à deux, 6 s seul), vote nominatif.
  - Effort : S conception, M code

- **[Majeur] Communication : une trompette**
  - Constat : seule émote, la fanfare (V). Ni ping ni nom d'équipier ; le HUD ne lit que le pion local. Voix de proximité en L7, après la V1. Un officiant immobile ne peut pas être prévenu ; des inconnus ne se coordonnent pas (supposé : le genre vit de ces échanges).
  - Preuve : TrumpetEmoteRules.h:49-65 ; STapaoHUD.h:14-17 ; cahier l.185, l.375 ; aucun symbole `Ping`.
  - Recommandation : noms et distance, un ping limité, fanfare fléchée ; prototype de voix avant L6.
  - Effort : M (pings), L (voix)

- **[Majeur] Aucune entraide**
  - Constat : mort définitive sur l'île, ni relève, ni soin d'autrui, ni transport d'équipier (« après la V1 »). L'abandon existe, le sauvetage non.
  - Preuve : ExpeditionRules.h:57 ; consumable-coconut.md:12-18 ; cahier §4.
  - Recommandation : un corps rapporté à bord (deux porteurs) annule les 50 couronnes de recrutement.
  - Effort : M

## Manques
Voix de proximité ; pings ; objets lourds ; noms des équipiers ; outils rares ; seconde rame ; rapatriement des corps ; titres de fin de manche ; table de navigation.

## Quick wins
1. Noms et distance au-dessus des équipiers.
2. Aucune noix de coco près du drapeau.
3. Fanfare = flèche de ralliement.
4. Une cloche de 80 kg à 300 couronnes, pour tester le portage à deux.
5. Texte satirique du sermon lu à voix haute par l'officiant, sans effet de règle.

## Questions qui challengent l'équipe
1. Quel moment oblige quatre joueurs à agir ensemble ? Si c'est le sermon, il n'est pas jouable.
2. Si trois joueurs restent passifs, le quatrième gagne-t-il ? Pourquoi les passifs s'amuseraient-ils ?
3. Un coop sans moyen de se parler, avec la voix après la V1 : quel clip un joueur enverra-t-il à un ami ?
