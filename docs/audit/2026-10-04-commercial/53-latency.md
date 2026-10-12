# 53 — Latence : jouer à 100–200 ms
Domaine : F · Auditeur : ingénieur netcode (latence)

## Verdict
Le serveur décide de tout, mais hors animations rien n'est prédit, rembobiné ni adapté au ping : l'hôte joue à 0 ms, les trois clients subissent RTT + lissage. Lecture de code seule, aucun test dégradé (recette n°10 jamais jouée). Risque commercial : des avis « lag » dès 100 ms, sur les valeurs portées, cœur du jeu.

## Constats

- **[Majeur] Portage : l'objet tenu n'obéit qu'au serveur**
  - Constat : seul le serveur possède le `UPhysicsHandleComponent` ; le porteur voit l'objet suivre sa caméra avec RTT + ~100 ms de retard (lissage supposé) et le cogne là où il le croit libre ; chocs > 250 cm/s = perte de valeur. Bloquant dès ~120 ms si confirmé.
  - Preuve : IslanderGrabComponent.cpp:39-45, 516 ; ValuableComponent.h
  - Recommandation : proxy visuel local (cible et balayage locaux) ; cible du porteur envoyée en non fiable, bornée par `MaxTargetLead` ; perte de valeur après grâce ≈ RTT.
  - Effort : M

- **[Majeur] Mousquet : tir serveur sans rewind ni retour de touche**
  - Constat : tir tracé 0,12 s après réception, visée et cibles du serveur ; le client voit les tiki RTT + ~100 ms en retard (≈ 1 m à 200 ms) : cible latérale = raté. Aucun marqueur de touche. `bLoaded` non prédit : un 2e clic rejoue un faux tir.
  - Preuve : MusketWeapon.cpp:259-263, 298-345 ; TikiRules.h:111
  - Recommandation : historique des capsules tiki (30 Hz, 250 ms), rembobinage min(RTT/2 + lissage, plafond DA) ; marqueur de touche serveur → client ; `bLoaded` prédit.
  - Effort : M

- **[Majeur] Sabre : impact tardif, refus en cascade**
  - Constat : balayage 0,245 s après réception : correct au corps à corps (supposé). Son et chiffres (RPC non fiables) arrivent RTT après la lame : ~200 ms de décalage. Un refus laisse `LocalLastCombo` en avance : les 1–2 clics suivants sont refusés aussi.
  - Preuve : CutlassWeapon.cpp:348, 351-356, 451-466
  - Recommandation : impact prédit chez le porteur (overlap à `HitTime`, dédoublonné) ; combo remis à zéro au refus ; rewind via l'historique du mousquet.
  - Effort : S

- **[Majeur] Rames : entrée non fiable, sans prédiction**
  - Constat : l'entrée ne part qu'au changement, en RPC non fiable, sans répétition : un paquet perdu (1–3 %) immobilise la barque tant que la touche reste enfoncée. Rames et poussée ne réagissent qu'après RTT. `StrokePhase` s'accumule par machine : poussées décalées (supposé).
  - Preuve : BoatRiderComponent.cpp:195-208 ; BoatActor.cpp:611-657
  - Recommandation : renvoyer l'entrée tenue à 5–10 Hz ; prédire chez le rameur ; phase dérivée de l'heure serveur.
  - Effort : M

- **[Majeur] Subir un tiki : parade réactive impossible à 200 ms**
  - Constat : préparation de 0,45 s ; il reste ≈ 0,25 s au client à 200 ms, son temps de réaction. Télégraphe en multicast non fiable (1–3 % de coups sans préparation). `LaunchCharacter` serveur sur la victime : rattrapage ≈ 80 cm (à confirmer).
  - Preuve : TikiRules.h:127 ; TikiWarrior.cpp:165-173, 184-212 ; TikiCreature.h:381
  - Recommandation : parade horodatée acceptée jusqu'à 150 ms rétroactivement (DA) ; télégraphe en état répliqué ; recul rejoué chez la victime.
  - Effort : M

- **[Majeur] Sermon, votes, départ : pas d'entrée, pièges de latence**
  - Constat : les 10 cm portent sur la position serveur : le jitter ne déplace pas un joueur immobile (nuance 03). Reste : aucun appelant d'entrée, refus `NotStandingStill` (freinage ≈ 0,3 s), cause d'interruption non répliquée, barre à 100 % avant validation. Départ (T-112, non codé) : qui monte à bord à T−0,3 s serait compté absent.
  - Preuve : PacificationRules.h:26, 34 ; PacificationZone.cpp:278-309 (dev/T-054) ; ADR-005 §6
  - Recommandation : RPC fiables début/fin, amorçage 0,5 s à l'arrêt, cause répliquée, barre ≤ 99 % ; gel du départ avec grâce = max(RTT), votes idempotents.
  - Effort : S

- **[Mineur] Interactions : marge fixe, refus muets, double appui**
  - Constat : `GrabRange + 50` cm mesuré sur la position serveur, en retard de L_up : en sprint à 200 ms (60 cm) F est ignoré en bord de portée. Pas d'anti-rebond : F deux fois sur la barque = s'asseoir puis se lever. Endurance et poids hors mouvement prédit (34). Tolérances 0,12 / 0,15 / 0,25 s et +50 cm en dur, contre la règle DataAsset.
  - Preuve : IslanderGrabComponent.cpp:237, 288 ; BoatActor.cpp:331-345 ; CutlassWeapon.cpp:41-44, 321
  - Recommandation : `DA_NetTolerances` pilotée par `ExactPing` ; verrou d'interaction jusqu'à réponse ; RPC client de refus.
  - Effort : S

## Manques
1. Ping et perte ni lus ni affichés (§18) ; hôte lent en sus (51).
2. Historique de positions ; horodatage des entrées tenues.
3. Retours serveur → client : refus, cause d'interruption, touche.

## Quick wins
1. Rames : renvoi périodique de l'entrée tenue.
2. `ClientRejectAttack` remet le combo à zéro ; `bLoaded=false` prédit.
3. Anti-rebond local de 0,5 s sur l'interaction.
4. Tolérances vers un DataAsset.
5. PIE à 2 : `Net PktLag=100 PktLagVariance=20 PktLoss=2`, puis 200.

## Questions qui challengent l'équipe
1. À quel RTT un client doit-il finir la manche 3 sans frustration ?
2. L'équipe teste côté hôte à 0 ms : qui joue client trois heures par semaine ?
3. « Ni duplication ni désaccord final » (§12 n°10) suffit-il pour vendre ?
