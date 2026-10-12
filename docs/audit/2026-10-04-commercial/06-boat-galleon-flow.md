# 06 — Barque physique, galion et palan : flux de navette
Domaine : A · Auditeur : designer navigation

## Verdict
La navette est la pièce physique la plus travaillée du prototype, mais elle n'a ni retenue, ni rappel, ni recours, et sa moitié retour (palan, barre) n'existe pas. Les durées ci-dessous sont calculées : rien n'a été chronométré ni testé sur deux machines. Risque commercial : une barque perdue ou restée au mauvais bord ruine la manche sans que l'équipe puisse agir, vers un galion que la montée des eaux noierait.

## Constats
- **[Majeur] Le galion-refuge est englouti**
  - Constat : il suit `GetSeaLevel()` global, or la montée est locale et sa grille de 2,56 km couvre le mouillage. Calc., non joué : pont à 1,62 m, montée 650 cm × α^1,6, donc plus de 1,2 m d'eau sur le pont vers +35 s, nage forcée. Contredit le cahier §24.3.
  - Preuve : `GalleonActor.cpp:128-132`, `FloodRules.h:78`, `FloodSimulationActor.cpp:468-473`, `storm-flood.md:67-69`.
  - Recommandation : galion posé sur `GetWaterSurfaceAt` sans vagues, avec un test.
  - Effort : S
- **[Majeur] Durée de traversée non pilotée**
  - Constat : arrivée à 200-350 m du rivage selon le fragment, galion 25 m plus loin (jusqu'à 300 m si le fond est sous 4 m). Calc. (intégration 1D de `BoatActor.cpp`, asset à 14 000 N), 275 m : 38 s seul, 53 s à quatre, 70 s à 500 kg ; 2,7 à 5 min avec les défauts C++. L'écart doc/asset est déjà relevé (01, 08) ; ici, la charge ralentit et rien ne fixe une cible.
  - Preuve : `fragments-tuamotu.md:72`, `GalleonRules.h:166-178`, `GalleonSpawner.cpp:70-76`, `BoatPhysics.h:131-135`.
  - Recommandation : cible ≤ 45 s chargée, verrouillée par une spec sur `BoatPhysics` ; mouillage « Galleon » par fragment ; tester `MinDepthCm` plus bas (galion cinématique).
  - Effort : M
- **[Majeur] Barque unique : falaise, aucune retenue, aucun rappel**
  - Constat : elle coule en 6 s dès qu'un plat-bord plonge (80 kg/s pour 450 kg, vidange coupée) ; franc-bord de 21 cm à pleine charge (calc.). Rien ne la retient à terre ni au galion, le vent la pousse (supposé), nul moyen de la rappeler : les équipiers à terre, qui nagent 30 m (09), attendent le passeur, surtout dans les 60 dernières secondes. Barque de secours : L8.
  - Preuve : `BoatActor.cpp:662-684`, `BoatRules.h:57,61`, `WindStormActor.cpp:339-369`, `galleon.md:44`, cahier §24.3.
  - Recommandation : ancre (F 2 s) ; rappel en autopilote par corne (ADR-003 B-01) ; alarme à 50 % d'eau ; écopage ; HUD « dernière barque dans X s ».
  - Effort : M
- **[Majeur] Atterrissage jamais joué**
  - Constat : platier de 150 m à −0,4 m, 35-120 m à traverser ; tirant d'eau 15 cm (rameur) à 39 cm (500 kg, calc.) : chargée, la barque racle probablement, puis se pousse à pied. Critères de recette non cochés ; ZQSD jamais piloté par un agent.
  - Preuve : `fragments-tuamotu.md:124-126,243`, `boat-rowing.md:100-110`.
  - Recommandation : passe de 1,2 m de fond et balise par mouillage ; playtest chronométré, solo puis deux joueurs.
  - Effort : M
- **[Majeur] Amarrage et palan : un test de distance, pas un geste**
  - Constat : « amarrée » = moins de 4,5 m du dock et moins de 40 cm/s, sans retenue. Avec houle et vent, le palan (maintien 3 s, refusé si elle bouge) risque des refus aléatoires (supposé). L'échelle téléporte sans animation et pose tous les descendants au même point de la barque.
  - Preuve : `GalleonActor.cpp:195-203,334-336`, `RunRules.h:64-66`.
  - Recommandation : amarre automatique (corde, son) sous 1 m/s dans le rayon, retenue souple ; `StandingSpots` ; animation d'échelle de 1,2 s.
  - Effort : M
- **[Mineur] Ramer, c'est maintenir Z**
  - Constat : aucune maîtrise (cadence, accostage), aucune endurance : le plaisir physique, signature possible du jeu, reste du transit. Le requin n'y menace rien (15, 16).
  - Preuve : `BoatRiderComponent.cpp:100-119`, `boat-rowing.md:36`.
  - Recommandation : poussée +20 % en cadence sur le rythme de 1,2 s ; accostage propre récompensé par un son et 3 s d'horloge.
  - Effort : M
- **[Mineur] Traversée muette**
  - Constat : `BoatRules.h` déclare 13 champs de gerbes et de sons sans classe `UOarSplashComponent`, et `BP_Barque` n'a aucun composant audio.
  - Preuve : `BoatRules.h:193-244`, grep `OarSplash`, `BP_Barque.uasset`.
  - Recommandation : composant de gerbes, sons d'aviron et de coque.
  - Effort : S

## Manques
Barque de secours, rappel, ancre, ponton et balise, écopage, danger de traversée (15, 16), horloge qui attend le premier pied à terre (01).

## Quick wins
1. Galion sur `GetWaterSurfaceAt`.
2. Spec « 250 m en 50 s au plus à quatre joueurs » (valeur actuelle, calc.) ; corriger `boat-rowing.md`.
3. Répartir les descendants de l'échelle sur `StandingSpots`.
4. Alarme sonore à 50 % d'eau dans la cale.

## Questions qui challengent l'équipe
1. Combien de secondes de rame par manche acceptez-vous, et pourquoi accélérer le bateau plutôt que rapprocher le galion ?
2. Quand la barque coule, trois joueurs à terre, quelle est la réponse du jeu avant L8 ?
3. La traversée est-elle un gameplay (où est le défi à quatre ?) ou un transit (pourquoi ne pas la raccourcir ?) ?
