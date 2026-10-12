# 46 — Bande passante d'un listen server à 4
Domaine : E · Auditeur : ingénieur réseau (bande passante)

## Verdict
Aucune capture réseau n'existe : le budget de `performance.md:31` (16 Ko/s moyen, 64 en pic par client) reste « à mesurer ». La conception est sobre (villages, camps, marae : une graine ; culling du butin à 80 m ; animations tiki < 0,5 Ko/s), mais tout reste au défaut moteur et la cargaison arrimée est répliquée à chaque image. Modèle non mesuré : ≈ 28 Ko/s par client en traversée avec 10 objets à bord, ≈ 47 en combat (38 pour 20 tiki), ≈ 100 et plus en tempête.

## Constats

- **[Majeur] La cargaison arrimée est un flux permanent**
  - Constat : chaque objet arrimé est replacé sur la coque à chaque image : il ne dort jamais, son mouvement part à chaque mise à jour. Modèle : ≈ 32 o × 60 Hz ≈ 1,9 Ko/s par objet et par client (10 objets : 19 Ko/s ; 40 : 76). Nombre non limité (charge max 500 kg) ; le cahier §5 prévoit de « couper sa simulation ».
  - Preuve : BoatActor.cpp:473-480 ; BoatRules.h:41 ; CAHIER l.72 ; moteur ReplicatedState.cpp:67-140 (taille estimée).
  - Recommandation : arrimage = physique coupée, attaché à la coque, `SetReplicateMovement(false)`, dormance.
  - Effort : S

- **[Majeur] Défauts moteur partout : 100 Hz, adaptatif coupé, dormance absente**
  - Constat : aucun `SetNetUpdateFrequency` sur props, tiki (APawn : 100 Hz, priorité 3), cadavres, barque ; `net.UseAdaptiveNetUpdateFrequency` vaut 0 ; seule la noix est dormante. Un tiki en marche ≈ 1,9 Ko/s par client : 8 = 15 Ko/s, 20 en combat = 38 ; ×1,6 à 100 i/s (hôte non plafonné). Animations : multicast non fiable de 2 o, locomotion déduite de la vitesse : négligeables.
  - Preuve : moteur Actor.cpp:294-314, Pawn.cpp:86-89, NetDriver.cpp:523 ; TikiCreature.cpp:225-231 ; grep et BP_* décodés : fréquence réglée sur 4 acteurs globaux seulement.
  - Recommandation : tiki, props, butin 20 Hz (min 1), barque 30, joueurs 50, cadavres 10 ; adaptatif à 1 avec `ForceNetUpdate()` aux interactions (sinon premier envoi jusqu'à 0,5 s en retard).
  - Effort : S

- **[Majeur] 4 110 butins : peu d'octets, mais le CPU de l'hôte**
  - Constat : le culling tient (≤ 90 butins pertinents par client, 1 par ≈ 220 m² : calcul) ; trafic au repos nul. Mais le pilote classique parcourt tous les acteurs actifs à chaque image (`PreReplication`, pertinence × 3 connexions), et la dormance exige un canal ouvert : un butin hors de portée ne dort jamais. 3 à 9 ms par image (supposé).
  - Preuve : moteur NetDriver.cpp:5304-5440, 5506-5510 ; PhysicsPropActor.cpp:12-23 ; BP_Loot_* décodés (`NetCullDistanceSquared` 6,4e7 cm² = 80 m) ; board.md:115.
  - Recommandation : adaptatif (butin oisif à 2 Hz), puis butin passif en instances locales déterministes (`Seed` existe), acteur répliqué à la saisie : 4 110 → ≈ 50.
  - Effort : S puis L

- **[Majeur] Le vent réveille le butin : tempête finale à ≈ 100 Ko/s et plus (supposé)**
  - Constat : le serveur pousse à chaque image tout corps libre à 50 m d'un joueur (375 Pa ×4 en tempête finale, jusqu'à 25 m/s²) : 35 à 50 butins éveillés (calcul) ≈ 66-95 Ko/s par client, en plus du reste. Le réveil par `AddForce` n'est pas vérifié.
  - Preuve : WindStormActor.cpp:376-406, 429-441 ; WindRules.h:84, 140-148.
  - Recommandation : butin hors vent (tag `WindSheltered`) ; PIE à 2 en tempête avec `stat net`.
  - Effort : S

- **[Majeur] L'hôte n'est pas protégé, le budget n'est pas réparti**
  - Constat : plafond moteur 100 Ko/s par client (3 clients = 2,4 Mbit/s montants, ADSL dépassé) ; aucune section réseau dans `DefaultEngine.ini`. Budget proposé par client (moy./pic Ko/s) : joueurs 3/6, tiki 4/10, barque et cargaison 1,5/4, props éveillés 2/10, cadavres 0,5/8, globaux 1/2 = 12/40.
  - Preuve : moteur BaseEngine.ini:1839-1861 ; DefaultEngine.ini.
  - Recommandation : `MaxClientRate = MaxInternetClientRate = ConfiguredInternetSpeed = 64000` ; mesurer le scénario §12 avec `-NetTrace=1`.
  - Effort : S

- **[Mineur] Cadavres : ≈ 1,4 Ko par corps à la naissance**
  - Constat : `Pose` = 19 `FTransform` non quantifiés (≈ 72 o) par client, plus 2 à 4 s de mouvement du corps racine (≈ 6 Ko) : 20 morts en 10 s ≈ 150 Ko par client.
  - Preuve : TikiRagdollCorpse.h:75 ; characters-tiki.md:48 ; moteur Vector.h:1193 ; TikiRules.h:178.
  - Recommandation : quaternions 16 bits (≈ 130 o) ou pose recapturée par chaque client.
  - Effort : S

## Manques
1. Aucune mesure réseau (Networking Insights, `stat net`, `net.PktLag`).
2. LOD réseau : 80 m pour une cuillère de 5 cm.
3. VoIP (L7) : Opus ≈ 3-4 Ko/s par locuteur via l'hôte (supposé).

## Quick wins
1. `net.UseAdaptiveNetUpdateFrequency=1` plus `ForceNetUpdate()` dans `Grab`, `Release`, `HandleHit`.
2. `SetNetUpdateFrequency` aux constructeurs : tiki, props, butin 20 ; barque 30 ; cadavres 10.
3. `MaxInternetClientRate=64000`.
4. Torche : `Fuel` (TorchItem.cpp:353) répliqué à chaque image (≈ 0,5 Ko/s) : répliquer l'heure de fin.

## Questions qui challengent l'équipe
1. Quelle ligne montante minimale exigez-vous d'un hôte, et qui mesure à 4 joueurs ?
2. Les 4 110 butins doivent-ils être des acteurs physiques répliqués avant qu'un joueur y touche ?
3. Le vent doit-il déplacer le butin chez tous, quand le réseau est le plus chargé ?
