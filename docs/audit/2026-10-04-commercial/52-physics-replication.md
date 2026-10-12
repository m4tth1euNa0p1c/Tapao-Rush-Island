# 52 — Physique répliquée : risques de désynchronisation
Domaine : F · Auditeur : ingénieur physique réseau

## Verdict
Barque, butin, armes lâchées, cadavres et noix tournent en réplication `Default` : serveur et clients simulent, le serveur corrige. Aucun mode n'a été choisi ni testé (cahier §5), la physique asynchrone est absente, rien n'a tourné à deux machines. Le galion cinématique est le bon modèle ; la barque chargée, au centre de la boucle, est le point fragile.

## Constats

- **[Bloquant commercial] Mode de réplication jamais choisi**
  - Constat : aucun `SetPhysicsReplicationMode` en C++ ni dans les assets ; `bTickPhysicsAsync` est faux, donc ni prédiction ni Resimulation (WIP). En `Default` : correction par la vitesse, extrapolation ≤ 10 ms, saut dur au-delà de 400 cm ou après 0,5 s sans convergence, corps liés non corrigés.
  - Preuve : DefaultEngine.ini:118-122 ; PhysicsSettings.cpp:24 ; EngineTypes.h:2019-2033, 3619-3629 ; PhysicsReplication.cpp:123 ; CAHIER §5 l.71.
  - Recommandation : ADR à deux voies. A : un seul simulateur par corps partagé, clients suiveurs. B : `Tick Physics Async`, prédiction et `PredictiveInterpolation` sur tous les corps qui interagissent.
  - Effort : M

- **[Majeur] Coque : chaque machine intègre sa barque**
  - Constat : forces recalculées à chaque image, donc à une cadence propre à chaque machine ; poussée de rivage, désenlisement et vent n'existent que sur le serveur ; phase de houle locale ; cargaison arrimée, téléportée par le serveur, simulée librement au contact de la coque cliente (supposé). Sous 20 i/s l'hôte ralentit sa physique.
  - Preuve : BoatActor.cpp:39, 389, 404-414, 480 ; WindStormActor.cpp:331 ; ChaosScene.cpp:340.
  - Recommandation : le serveur seul simule la coque ; clients cinématiques interpolés (~100 ms) ; cargaison attachée, simulation coupée (cahier §5 l.72).
  - Effort : M

- **[Majeur] Objets portés : poignée serveur seule**
  - Constat : un porteur client voit l'objet avec au moins un aller-retour de retard. Le poids porté ralentit : le serveur l'applique aussitôt, le propriétaire après réplication, d'où une correction à chaque saisie ou co-porteur. `SetHeldGhost` restaure `Block` partout (profil devenu « Custom ») : un cadavre `Ragdoll`, qui ignorait Pawn, bloque ensuite les pions.
  - Preuve : IslanderGrabComponent.cpp:429-451, 568-574 ; IslanderMovementComponent.cpp:389 ; BodyInstance.cpp:564-570.
  - Recommandation : état « tenu » répliqué, pose calculée localement depuis la visée (physique suspendue, cahier §5 l.67) ; profil sauvegardé puis restauré.
  - Effort : M

- **[Majeur] Cadavres : seul le bassin est répliqué**
  - Constat : os, vitesses initiales et direction du coup restent locaux : à +2,5 s le cadavre saute à la pose serveur (1-2 m, supposé) ; un client qui le reçoit après un déplacement reconstruit la pose de création sur la position actuelle (supposé). Les moitiés rigides se répliquent bien.
  - Preuve : TikiRagdollCorpse.h:91 ; TikiCreature.cpp:423, 650-654 ; TikiRules.h:155 ; ActorReplication.cpp:304-313.
  - Recommandation : ragdoll cosmétique 2,5 s, puis cadavre rigide figé (`ATikiCorpse`, déjà codé) ; répliquer la direction du coup.
  - Effort : M

- **[Majeur] La marche pousse les copies locales**
  - Constat : `bEnablePhysicsInteraction` est actif : le CMC pousse sur chaque machine, prédiction du propriétaire comprise, sa copie locale du butin, des cadavres, de la cargaison ; l'état serveur revient et corrige (va-et-vient, supposé).
  - Preuve : IslanderMovementComponent.cpp:143-150 ; CharacterMovementComponent.cpp:7806, 7885 (moteur).
  - Recommandation : voie A : poussée désactivée hors autorité ; voie B : prédiction.
  - Effort : S

- **[Majeur] Galion sain, barque amarrée libre**
  - Constat : balancement lu sur l'horloge serveur : 100 ms d'écart donnent ≈ 2 cm de pont. `MoorBoat` téléporte la barque une fois, sans lien ensuite (spec) : elle dérive dès qu'on la touche (supposé). Le palan (T-112) en dépendra.
  - Preuve : GalleonActor.cpp:157-165, 240-254 ; GalleonRules.h:65-79 ; galleon.md:44.
  - Recommandation : amarrée = cinématique, attachée à `barque_dock`, physique rendue au largage.
  - Effort : S

- **[Majeur] Griefing physique à portée de tout joueur**
  - Constat : un lancer (900 cm/s) coûte 24 % de la valeur par choc, destruction en cinq chocs ; la saisie partagée laisse un tiers tirer l'objet ; tout corps dans le volume compte pour la charge (sept cadavres et quatre joueurs font couler la barque) ; le poing, sans filtre ami, repousse un coéquipier hors de la barque.
  - Preuve : ValuableComponent.h:33-45 ; IslanderGrabComponent.h:93-97 ; BoatActor.cpp:533 ; IslanderCombatComponent.cpp:231.
  - Recommandation : pas de perte de valeur par choc dû à un joueur, filtre ami sur le poing, charge comptée sur l'arrimé.
  - Effort : M

## Manques
1. Test PIE à 2 avec `Net PktLag=120` : barque chargée, saisie à deux, kill puis cadavre, tempête (boat-rowing.md:109).
2. Règle écrite « un corps, un simulateur ».
3. `UNetworkPhysicsSettingsComponent` par corps (seuil de saut dur).

## Quick wins
1. PIE à 2 : `Net.RepMovement.DrawDebug 1` et `p.LogPhysicsReplicationHardSnaps 1`, compter les sauts durs de la barque.
2. `bEnablePhysicsInteraction = HasAuthority()`.
3. Répliquer `LastBlowDirection` avec la mort.
4. `SwellTime` sur l'horloge serveur.
5. Entrée de rame renvoyée toutes les 0,25 s : un paquet perdu la bloque (BoatRiderComponent.cpp:196-208).

## Questions qui challengent l'équipe
1. Qui est l'unique simulateur de la barque, et avez-vous déjà compté ses sauts durs à 150 ms ?
2. Un cadavre de 40 kg doit-il être répliqué sur quatre machines, ou attaché dès qu'il touche le plancher ?
3. Objet porté : réactivité (le porteur simule, triche possible) ou autorité serveur (un aller-retour de retard) ?
