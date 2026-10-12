# 88 — Comparatif de fonctionnalités avec 6 concurrents
Domaine : I · Auditeur : analyste concurrence

## Verdict
Sur 16 lignes, Crownfall est à parité sur 2 (objets physiques, solo) ; 4 codées en partie, 3 prévues, 7 absentes. Les leaders (LC, R.E.P.O., CW, Peak) vendent 7,49–9,75 € avec 93–97 % d'avis positifs, voix et déblocages ; écart principal face aux six : l'extraction à la barque sur une mer qui monte (81 : trois quasi-jumeaux hors panel). Risque : être jugé au prix du genre (≈ 10 €) sans ses quatre réflexes, l'atoll 1:1 n'étant pas un critère de comparaison.

## Constats
Vu (API Steam, 2026-10-04) : prix de base, notes, solo, accès anticipé, Workshop. Ventes : estimations d'analystes. Autres cellules concurrentes : supposé ; ? = non vérifié. Crownfall : CODÉ = compilé, jamais testé à deux ; partiel = codé en partie ; PRÉVU = au cahier, non jouable. Sixième : Raft.

|Ligne|LC|R.E.P.O.|CW|SoT|Peak|Raft|Crownfall|
|---|---|---|---|---|---|---|---|
|Boucle|ferraille|extraction|filmer|voyages|escalade|survie|partiel : île seule|
|Session (jour, niveau)|~20 min|~15 min|~30 min|libre|~1 h|libre|PRÉVU : 11-17 min (calcul)|
|Quota|oui|oui|oui|non|non|non|PRÉVU : calcul testé|
|Économie, boutique|terminal|boutique|boutique|or|non|non|partiel : bilan, 3 prix|
|Méta|faible|jetons de taxe|MetaCoins|énorme|badges|non|ABSENT|
|Monstres|~20|~20|~20|Kraken|décor|requins|partiel : 3 peaux, 1 IA|
|Objets physiques|poids|centrale|partielle|navire|partielle|radeau|CODÉ|
|Voix de proximité|oui|oui|oui|oui|oui|non|ABSENT|
|Cosmétiques|combinaisons|500+|chapeaux|payants|64 badges|non|ABSENT|
|Solo|oui|oui|non|non|oui|oui|CODÉ|
|Joueurs max|4|6|4|4/navire|4|4|PRÉVU : 4, sans plafond|
|Procédural|intérieurs|niveaux|?|non|carte du jour|?|partiel : 1 sur 20|
|Mods|communautaires|communautaires|communautaires|non|communautaires|communautaires|ABSENT|
|Prix de base (€)|9,75|9,49|7,49|39,99|7,49|20,49|non fixé|
|Note Steam (%)|97|96|93|88|94|93|aucune|
|Ventes estimées (M)|≥10|15,4|2,2|≈15|≥10|≥5|0|

Sources (6 appels web, dont 2 recherches groupées ; * = extrait de recherche) : https://store.steampowered.com/api/appdetails?appids=1966720,3241660,2881650,1172620,3527290,648800&filters=price_overview&cc=fr · https://api.steampowered.com/IStoreBrowseService/GetItems/v1 (mêmes appids) · https://alineaanalytics.com/blog/steam_june_2025/ · *https://www.aol.com/peak-sells-10-million-copies-155741375.html · *https://gamedeveloper.com/business/lethal-company-sold-an-estimated-10-million-copies · *https://www.vgchartz.com/article/461242/content-warning-sales-top-22-million-units-surpasses-88-million-players/ · *https://en.wikipedia.org/wiki/Raft_(video_game) · *https://raijin.gg/app/1172620/Sea_of_Thieves_2026_Edition · *https://www.destructoid.com/how-to-get-cosmetics-in-r-e-p-o/ · *https://games.gg/peak/guides/peak-all-final-ascent-badges-and-cosmetic-rewards/ · *https://www.pcinvasion.com/how-to-get-metacoins-mc-in-content-warning/ · *https://thunderstore.io/c/lethal-company/

- **[Bloquant commercial] Quatre réflexes du genre, aucun jouable à deux chez nous**
  - Constat : LC, R.E.P.O. et CW ont invitation et voix, boucle vendre → quota → boutique, déblocages gagnés, monstres à règle. Ici : ni invitation, ni voix, ni déblocages ; boucle non branchée ; trois peaux, une IA.
  - Preuve : tableau ; `monke_island.Build.cs:49` ; board T-028, T-112 à T-116 `a-faire` ; `TikiWarrior.h:31` ; 12, 14.
  - Recommandation : critère de sortie de L4 : les quatre jouables à deux machines avant tout fragment de plus.
  - Effort : L
- **[Majeur] Bande de prix 7,49–9,75 € ; accès anticipé accepté**
  - Constat : le 2026-10-04, cinq des six sont soldés (−33 à −65 %) : 4,64 € payés (CW, Peak) à 13,99 € (SoT, 39,99 € de base). LC et R.E.P.O. restent en accès anticipé.
  - Preuve : API Steam appdetails, IStoreBrowseService (vu).
  - Recommandation : modéliser les revenus au prix soldé (≈ 65 % du prix de base chez quatre des six).
  - Effort : S
- **[Majeur] Avantage réel face aux six : l'extraction maritime physique**
  - Constat : aucun des six ne combine navette à rames, horloge commune et mer qui monte (SoT sans extraction, Raft sans quota). Codé, jamais testé à deux. Autre écart codé : cadavres portables, découpables (T-072 ; risque : 80). Le sermon, seul mécanisme sans équivalent (supposé), est hors `main`.
  - Preuve : `Boats/`, `Flood/`, `Galleon/` ; 03 ; 81.
  - Recommandation : fusionner le sermon (03), puis centrer la tranche verticale sur « barque chargée, mer qui monte » (01).
  - Effort : M
- **[Mineur] Mods : aucun Workshop dans le panel**
  - Constat : la catégorie Workshop manque aux six ; les mods de LC, R.E.P.O. et CW sont communautaires. L'éditeur d'îles E1 n'a aucun précédent d'achat ici.
  - Preuve : IStoreBrowseService (vu) ; cahier §20 ; thunderstore.io*.
  - Recommandation : reporter E1 après 1.0 ; garder les règles en DataAsset.
  - Effort : S
- **[Mineur] Ventes : trois hits aberrants, un public commun**
  - Constat : R.E.P.O. 15,4 M (06/2025), Peak et LC ≥ 10 M (≈ 46 M à eux trois en 08/2026, 82) ; CW 2,2 M payants (+ 6,6 M gratuits), Raft ≥ 5 M (2024). 61 % des joueurs de Peak ont joué R.E.P.O., 30 % LC.
  - Preuve : Alinea (vu) ; 82.
  - Recommandation : plan financier sur le plancher de 90, pas sur ces hits ; parité sociale d'abord.
  - Effort : S

## Manques
Tables stakes absentes, par priorité :
1. Inviter un ami et lui parler : 6/6 en ligne, 5/6 avec voix (supposé hors LC).
2. Boucle fermée vendre → quota → boutique : LC, R.E.P.O., CW.
3. Déblocages gagnés : 4 leaders sur 4.
4. Monstres à règle : ≈ 20 chez chaque cousin contre 3 peaux.
5. Page Steam (prix, capsule, avis) : zéro.

## Quick wins
1. Adopter ce tableau comme grille de recette de L4 (½ j).
2. Script de veille Steam (les deux API ci-dessus) : prix, avis, accès anticipé de 6 jeux + 3 quasi-jumeaux de 81 (2 h).
3. Plafond de 4 joueurs dans la session d'hébergement (`MaxPlayers` : 0 occurrence) (½ j).

## Questions qui challengent l'équipe
1. 61 % des joueurs de Peak ont joué R.E.P.O. : à 9,99 €, que leur offrez-vous de plus ?
2. Quel écart quatre amis voient-ils en 5 minutes face à R.E.P.O., et est-il dans `main` ?
3. Accepteriez-vous un accès anticipé à 9,99 € (1 île, voix, boucle fermée, 5 monstres), comme LC, plutôt que 20 fragments sans voix ?
