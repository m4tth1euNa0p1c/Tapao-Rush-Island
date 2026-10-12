# 75 — Cohérence du monde et socle de lore
Domaine : H · Auditeur : worldbuilder

## Verdict
Le monde existe en assets (104 villages et hameaux japonais, 5 créatures, marae, camps), pas en règles : seule la charte de nommage tient. Rien n'explique la submersion, les tiki ni le japonais, et les pièces se contredisent (butin pris aux insulaires, perle « unique » ×40 au plus, Couronne « fictive » au drapeau réel). Risque : un collage sans clé de lecture, vulnérable aux critiques culturelles.

## Constats
- **[Majeur] Aucune bible, aucune clé de lecture**
  - Constat : « archipel imaginaire » justifie le Japon alors que le terrain est Raraka 1:1 ; le brief des minka est esthétique ; le cahier promet des factions insulaires aux motivations distinctes, livrées en une seule faction.
  - Preuve : islands.md:13, 26 ; minka-rurale.md:7 ; cahier l.11.
  - Recommandation : bible d'une page. Socle proposé (règle → mécanique servie) :
    1. **L'Anneau** : récifs sur une montagne noyée qui s'enfonce ; la Marée le reprend morceau par morceau, selon l'almanach du Bureau. → fragments, submersion, lave des Cendres.
    2. **Le Registre** : ne commandant pas la mer, la Couronne déclare « épave » ce qui va couler et en fait le récolement. Elle coule aussi (Crownfall) : les quotas montent. → contrat, taxe, défaite.
    3. **Le sermon** : la Couronne jure que la mer respecte les formes ; les insulaires, que l'île écoute qui se tait vingt secondes. Le jeu ne tranche pas. → +120 s, immobilité.
    4. **Les Veilleurs** (jamais « tiki » en jeu) : fruits de l'île (coco, crabe, cendre, ronce) que sa peur lève : nuit, villageois frappé, offrande emportée. Ni dieux ni ancêtres. → embuscades, vengeance.
    5. **Les villageois** : mêmes fruits, sans masque, dans les maisons des Plongeurs de perles, partis à une marée. → minka, -mura.
    6. **Marae, pêcheurs** : le marae rend ses perles à la mer ; la Perle du Gouverneur est un bijou du Bureau reposé là. Pêcheurs jamais vus, feu allumé. → zones calmes, camps.
    7. **L'équipage** : haricots recrutés, remplaçables ; la satire vise le Bureau, jamais l'île ; aucun humain à l'écran.
  - Effort : S

- **[Majeur] Submersion sans cause ni cohérence visuelle**
  - Constat : « île condamnée » n'a pas de cause ; l'eau monte dans une grille locale de 2,56 km, le reste de l'atoll restant sec à l'horizon.
  - Preuve : cahier l.9 ; storm-flood.md:67-71 ; ADR-004:19.
  - Recommandation : règle 1.
  - Effort : S

- **[Majeur] Veilleurs : lois d'hostilité et nom**
  - Constat : villageois et guerriers partagent la bourre de coco, l'un fuit, l'autre tue ; trois déclencheurs sans loi commune (nuit, villageois frappé, buisson approché) ; lave sur un atoll corallien ; « tiki », images sacrées, désigne des monstres.
  - Preuve : characters-tiki.md:25, 32, 109-120, 137-148, 222-225.
  - Recommandation : règle 4 ; « Veilleurs » en jeu.
  - Effort : S

- **[Majeur] Butin : la charte promet, la boucle pille**
  - Constat : la charte écarte le pillage des habitants et des lieux sacrés, or offrandes (120 $), conques (80), totems (150) et 12 objets domestiques valent de l'argent. La Perle du Gouverneur excuse un butin sur trois et se retrouve dans chacun des 2 marae par fragment (jusqu'à 40). Seul un villageois frappé déclenche une alarme.
  - Preuve : islands.md:67 ; marae-des-perles.md:16-22 ; characters-tiki.md:155 ; loot-common.md ; set_marae_rules.py:1-2 ; TikiVillager.cpp:406, 425.
  - Recommandation : assumer la satire (« récolement »), alarme sur offrande emportée.
  - Effort : M

- **[Mineur] Pêcheurs : un décor sans personne**
  - Constat : les camps (jusqu'à 8 par fragment) ont feu, outils, poissons, mais ni pêcheur ni identité : butin et soin seulement.
  - Preuve : CampRules.h:68-71 ; board T-096.
  - Recommandation : règle 6 ; une note par camp (« le feu est tiède »).
  - Effort : S

- **[Majeur] Couronne et réel : quatre écarts**
  - Constat : Couronne « fictive », mais drapeau « Union Jack, Royaume-Uni dans la fiction » et croix de Saint-Georges prévue au galion ; cahier encore « archipel fictif » après ADR-004 ; skin « indigène fictif » prévu ; « Crownfall » sans chute.
  - Preuve : cahier l.9, 24, 108 ; ADR-004:13 ; galleon.md:50 ; character-bean-soldier.md:65.
  - Recommandation : un pavillon fictif, amender §1 « Monde », retirer le skin, règle 2.
  - Effort : S

## Manques
1. Textes diégétiques : fiches d'objets, astuces de chargement, voix du Percepteur.
2. Second style de village, sinon 104 colonies identiques.
3. Signal d'arc : Percepteur aux pieds mouillés, manche finale « Crownfall ».
4. Indices de mystère par île (Plongeurs partis, feux allumés).

## Quick wins
1. Écrire la bible et son glossaire FR/EN, amender cahier §1.
2. « Veilleurs » pour « tiki » en jeu ; retirer « indigène fictif ».
3. Nom affiché « Perle du Gouverneur n° N ».
4. Dix astuces de chargement (§16) énonçant les règles 1 à 4.

## Questions qui challengent l'équipe
1. Qui est le méchant : la Couronne, le joueur qui exécute, ou l'île ?
2. Le joueur découpe des Veilleurs, mais l'île punit le meurtre d'un villageois : où est la ligne, devant un streamer ?
3. Raraka réel, jamais nommé (islands.md:33) : que gagne le joueur au coût du réel ?
