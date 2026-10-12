# 20 — Le galion comme lieu de vie (hub)
Domaine : B · Auditeur : designer de hub

## Verdict
Le galion est une belle coque marchable (cabine meublée, 14 sabords), pas un lieu : ni carte, ni barre, ni cale, ni Balance, ni boutique, et le plan prévu est un sas de vente. Rien n'y est à soi, vivant ni durable. Risque commercial : une part majeure de chaque manche (supposé) se jouerait dans un hub sans âme ni mémoire, donc sans clip à partager ni raison d'y revenir.

## Constats
- **[Bloquant commercial] Le hub n'est qu'une coque**
  - Constat : n'existent que l'acteur (ponts, échelle, volumes) et le spawner. JOUER mène à `L_Boot`, carte vide ; barre, palan, Balance, registre, boutique et intérieur de cale manquent, or lobby, vente et signature font chaque manche.
  - Preuve : Content/MonkeIsland/Maps (3 cartes) ; ExpeditionBootGameMode.h:16 ; board.md:119-123.
  - Recommandation : tranche verticale jouable à deux (greybox accepté) : lobby, Balance, signature, boutique, chronométrée avant tout nouveau fragment.
  - Effort : L

- **[Majeur] Le Percepteur, « visage de la satire », n'est qu'un mot**
  - Constat : ni modèle, ni voix, ni répliques ; cotre sans maquette ; objets de valeur sans nom ni fiche. En solo, il serait la seule compagnie à bord.
  - Preuve : cahier §24.1 ; maquettes du 2e PC (rien) ; `ARoyalAppraiserActor` absent ; ValuableComponent.h.
  - Recommandation : un PNJ-bean (perruque, registre à modéliser ; clips existants Idle/Wave/Interact), ~30 répliques en String Table, voix-borborygmes, réactions : vente, objet abîmé, mort facturée, quota atteint ou manqué, plus belle pièce.
  - Effort : M

- **[Majeur] Aucune personnalisation, boutique sans enjeu**
  - Constat : skins non codés ; en 1re personne (défaut), la tenue ne se voit qu'avec R. Boutique : 3 articles (550 couronnes), hache et lanterne sans objet ni mécanique ; la défaite ne dépend que des ventes brutes, donc l'argent ne sert plus dès la manche 3 (supposé).
  - Preuve : board.md T-028 ; IslanderCharacter.h:185 ; EconomyRules.cpp:7-9 ; RunCalculator.cpp:82-83 ; GalleonRules.h:30-48.
  - Recommandation : boutique du Percepteur sur son cotre, 12 à 15 articles (places de cale, pavillon, voiles, chapeaux) ; vestiaire avec miroir en cabine ; galion baptisé, nom peint à la poupe.
  - Effort : M

- **[Majeur] Rien ne survit à la défaite : pas de « chez soi »**
  - Constat : partie perdue = close, injouable ; trésorerie, cale, améliorations disparaissent ; aucune progression hors partie prévue. Reste un « meilleur objet » au bilan.
  - Preuve : cahier §24.2 (étape 10), §24.7, §17 ; board.md T-115.
  - Recommandation : salle des trophées en cabine (meilleur objet de chaque partie, plaque équipage), journal de bord auto-rédigé ; profil persistant (pavillons, chapeaux) débloqué par paliers de manche.
  - Effort : M

- **[Majeur] La cabine, seul espace meublé, est noire et sans doute inaccessible**
  - Constat : table-carte, lit, coffres ; le 2e PC y prévoit coffre, carte, sauvegarde ; cahier et ADR-005 l'ignorent. Plafond 1,30 m pour une capsule de 1,80 m (supposé encore valide), aucune lumière.
  - Preuve : GALLEON_UE.md:3,27 (maquette) ; IslanderCharacter.cpp:33 ; board.md T-110 ; CA-G2 non testé.
  - Recommandation : cabine = salle des contrats : table-carte affichant `Name/CrownName/Pitch/Rumor` de `ST_Islands` (8 fragments sur 20, aucun code ne les lit), contrats au choix de l'équipage (§16 le prévoit, §24.2 tire l'île au sort) ; allumer, relever le plafond ou réduire la capsule.
  - Effort : M

- **[Majeur] Zone sûre sans rythme ni jeu**
  - Constat : « ni horloge ni ennemi » et vente objet par objet (jusqu'à 40) : un joueur travaille, trois attendent (supposé). Clips Wave et Celebrate, trompette, canons, mousquet : disponibles, inutilisés au hub.
  - Preuve : cahier §24.3-24.4 ; BeanAnimLibrary.h:87-93 (jamais lus).
  - Recommandation : plateau de Balance (vente groupée) ; jeux sans enjeu : cible mousquet/sabre faisant tutoriel, canon de pont tirant vers l'horizon ; émotes sur deux touches.
  - Effort : M

- **[Mineur] Départ solitaire, bord muet**
  - Constat : un joueur maintient 2 s à la barre ; la maquette offre barre, cabestan, ancres, cloche. Aucune musique ni son de bord.
  - Preuve : RunRules.h:58 ; galleon-report.json ; aucun `MUS_*`.
  - Recommandation : quatre postes optionnels raccourcissant le préavis ; sons de bord synthétisés comme `trumpet_sounds.py` ; heure dorée fixe en zone sûre.
  - Effort : M

## Manques
1. Cale visible (intérieur non modélisé) : la salle du trésor.
2. Écrans TAPAO équipage, contrats, registre, boutique (absents du kit) ; Percepteur, cotre, Balance (modèles, voix, textes).
3. Vestiaire, skins, trophées, journal de bord.
4. Musique, sons de bord, éclairage de nuit.

## Quick wins
1. Allumer la cabine (2 lanternes) et tester l'entrée en PIE.
2. Baptiser le galion : nom satirique en TextRender à la poupe.
3. Brancher Wave et Celebrate sur deux touches (clips importés).
4. Afficher `Name` et `Pitch` de `ST_Islands` à l'écran de chargement (`Tip` existant).
5. Cloche synthétisée (socket `bell`) à l'ancrage du galion.

## Questions qui challengent l'équipe
1. Après une défaite, que reste-t-il au joueur, et pourquoi reviendrait-il sur ce galion ?
2. Qui écrit, anime et double le Percepteur avant L4 ?
3. Combien de minutes un équipage passe-t-il au hub par manche, et que fait-il pendant qu'un seul vend ?
