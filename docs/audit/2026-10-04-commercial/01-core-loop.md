# 01 — Boucle complète : menu → galion → île → zone sûre
Domaine : A · Auditeur : game designer senior (extraction coop)

## Verdict
La boucle n'existe pas encore et aucune durée n'a été mesurée : tout ce qui suit est calculé. Le rythme est mal réglé (paramètres contradictoires, zones sans retour) et la perle du marae rend zones et combat facultatifs. Risque commercial : une manche de 12 minutes sans arc écrit ni pic final, réduite à une course au marae.

## Chronométrage d'une manche (fragment médian, calculé ; actuel → cible)
- **Menu → pont** : JOUER puis `L_Boot` puis carte de 110 Mo, sans lobby → ≤ 30 s, contrat lu à bord.
- **Zone sûre** : vente objet par objet, ~10 s l'objet (supposé) → 2:00, total révélé d'un coup.
- **Voyages** : 2 chargements par manche, 20 s visés, jamais mesurés → ≤ 25 s, masqués par la mise à la voile.
- **Pont → plage (275 m)** : 1:15 (3:30 à 1,6 m/s) → 1:00.
- **Zone 1** : sermon validé à 3:30 → avant 3:00 (pic 1).
- **Milieu** : 3:30 de marche et d'embuscades (≤ 1 par joueur / 18 s) → zone 2, marae ou camp, crépuscule lié à l'échéance.
- **Alerte → départ** : barre et palan non codés → 1:30 de retour en tempête + 0:30 de palan (pic 2).
- **Mort** : spectateur ~6 min → retour sur le pont à 90 s (palan, vigie).
- **Manche** : 11 à 17 min → 11 à 13 min, vente = pic 3.

Actuel (1 zone) : 0:00 horloge, 1:15 plage, 3:30 zone validée, 7:00 montée, 8:00 départ du galion ; pics : combat, sermon, alarme d'eau, aucun pic final. Cible : échéance 4:30, +3:00 par zone, horloge à la mise à l'eau.

## Constats
- **[Bloquant commercial] Boucle absente, jamais chronométrée**
  - Constat : seule l'île tourne ; barre, palan, départ, zone sûre, vente et manches manquent. L'horloge démarre au BeginPlay, pas après chargement plus 10 s.
  - Preuve : board T-112 à T-116 `a-faire` ; `ExpeditionGameState.h:46` ; journal T-106 « victoire non branchée ».
  - Recommandation : tranche verticale solo menu → île → vente → quota.
  - Effort : L
- **[Majeur] Valeurs de rythme contradictoires**
  - Constat : échéance 120 s (cahier) ou 300 s (DA) ; barque 1,6 m/s (fiche) ou ≈ 8-11 m/s (DA) ; équipage mort 8 s ou 30 s. La traversée dure 35 s ou 2:50. À 120 s, la zone 1 (≈ 3:30) est inatteignable.
  - Preuve : `ExpeditionRules.h:25` ; .uasset lus en binaire, à confirmer (300 ; 30 ; MaxRowSpeed 1 100) ; `boat-rowing.md:35` ; `fragments.json` (arrivée à 275 m).
  - Recommandation : table « budget de manche » et spec qui échoue si traversée + marche vers la zone 1 dépassent 60 % de l'échéance.
  - Effort : S
- **[Majeur] Une zone coûte plus qu'elle ne rapporte**
  - Constat : marche entre villages (médianes 376 à 914 m, 2:20 à 5:40), sermon 20 s, combat ~50 s (supposé) : 3:00 en barque, 3:30 à 7:00 à pied, pour +2:00. 6 fragments sur 20 ont un tour des zones de plus de 2 km. Le « temps ajusté en solo » (§6.3) n'existe pas.
  - Preuve : `villages/fragments/index.json` (calcul, détour ×1,3, 3,5 m/s) ; `RunRules.h:42`.
  - Recommandation : +180 s par zone ou coffre garanti ; tirage filtré (≤ 2 km).
  - Effort : M
- **[Majeur] Butin bimodal : décor inutile, perle qui annule le quota**
  - Constat : 12 objets communs à 1-6 (moyenne 2,9) : cale de 40 = 114 pour un quota de 250. Chaque marae (2 par fragment, zone calme, ≈ 500 m de l'arrivée) porte une perle de 1 500 : une seule suffit aux manches 1 à 5 en solo, 1 à 3 à quatre.
  - Preuve : `loot-common.md` ; `marae-des-perles.md` ; `MaraeRules.h:79-87` ; `RunRules.h:30,70` ; `fragments.json` (sites de marae).
  - Recommandation : perle ≈ 40 % du quota, gardée par un élite ; objets communs ×10 ou conteneurs.
  - Effort : M
- **[Majeur] Arc émotionnel non écrit**
  - Constat : heure de départ tirée entre 6:30 et 18:00, coucher en 15 s : une expédition sur deux (calcul) voit la nuit, sans lien avec l'échéance. Ni boussole, ni carte, ni ping. Fin de manche « sans pression » (§24.4).
  - Preuve : `DayNightRules.h:40-55` ; recherche `TacticalMap|Compass` : 0 résultat.
  - Recommandation : crépuscule à échéance − 60 s ; fumée sur chaque village ; Percepteur qui commente chaque objet.
  - Effort : M

## Manques
Barre, palan, vente, défaite (T-112 à T-116) ; boussole, carte (§19) ; événement de milieu de manche ; premier contrat guidé ; puits d'argent (le verdict ne lit jamais la trésorerie, `RunCalculator.cpp:75-98`).

## Quick wins
1. Journal « phase, durée » dans `AExpeditionGameState`.
2. Horloge au passage Preparing → Exploring : `bStartOnBeginPlay = false`.
3. Filtrer le tirage de fragment : tour des zones ≤ 2 km.
4. Corriger `boat-rowing.md` et `day-night-cycle.md`.

## Questions qui challengent l'équipe
1. Si une perle paie six quotas, pourquoi pacifier, et que montrera un streamer ?
2. Chaloupe à rames (1,6 m/s) ou hors-bord (≈ 10 m/s) : un DataAsset décide seul.
3. Quelle durée de manche visez-vous, mesurée à quatre dans un build ?
