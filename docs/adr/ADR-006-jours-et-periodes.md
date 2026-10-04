# ADR-006 : Une partie = des jours sur des îles différentes, un quota tous les 5 jours, 10 périodes

- **Statut** : acceptée. Demande du propriétaire du 2026-10-04 (session monk-island-63), avec deux arbitrages tranchés par lui : « 10 quotas de 5 jours » et « vente seulement au jour 5 ».
- **Complète et modifie** l'ADR-005 (partie en manches autour du galion) : une **manche = un jour**, et le quota ne se contrôle plus à chaque manche mais **tous les 5 jours**.

## Demande (mots du propriétaire)
« Une fois le déluge, si je suis revenu sur le galion avant la fin du déluge (sinon mort), alors chargement vers le jour suivant, nouvelle map, jusqu'au 5e jour. Je dois avoir tous les objets sur le bateau et les sous, sinon mort. Ainsi de suite, 10 fois, jusqu'à ce que le taux de spawn des mobs augmente et que ça devienne plus dur. Créer un record dans le menu. »

## Décision
1. **Un jour = une expédition sur un nouveau fragment tiré au hasard** (catalogue de l'ADR-004, jamais deux fois de suite le même). Le galion est ancré à son mouillage (`EAnchorageRole::Galleon`), et l'équipe part en barque.
2. **Fin du jour = fin du déluge.**
   - À la fin de la submersion, chaque joueur qui n'est **pas à bord du galion** (`AboardVolume`) **meurt** (mort définitive, T-104).
   - Les joueurs à bord survivent. La cale du galion (`CargoVolume`), plus ce qu'ils tiennent, est figée dans le manifeste (ADR-005 §5).
   - Il n'y a plus de partie sans fin : la fin de submersion déclenche toujours la suite (revue REC, constat 4).
3. **Transition** : écran de chargement « Jour N+1 », puis nouvelle île. Les morts réapparaissent à bord au jour suivant ; leurs frais sont facturés (ADR-005 §9). **Si tout l'équipage est mort**, la partie est perdue.
4. **Jours 1 à 4** : rien n'est vendu. Le butin s'accumule dans la cale, d'une île à l'autre.
5. **Jour 5 : jour du quota.**
   - Après le déluge, le galion arrive en **zone sûre**. On vend tout à la Balance du Percepteur (ADR-005 §8).
   - **Quota atteint** : période suivante. L'argent restant passe à la boutique et à la sauvegarde.
   - **Quota manqué** : mort de l'équipage, la partie est perdue.
6. **10 périodes de 5 jours (50 jours)** : passer la 10e = **victoire**.
7. **Difficulté croissante par période**, avec des valeurs dans `DA_RunRules` (courbes indexées par la période, jamais en dur) :
   - multiplicateur de spawn des mobs (s'ajoute au jour/nuit de T-119) ;
   - quota ;
   - éventuellement la durée avant le déluge.
8. **Record**, dans la sauvegarde de profil, hors campagne :
   - meilleure partie : périodes passées, jours survécus, argent total vendu, date ;
   - affiché dans le menu principal TAPAO (« Record : jour 23, période 5, 4 820 $ ») ;
   - mis à jour à la fin de chaque partie, victoire ou défaite.

## Conséquences et répartition
| Sujet | Tâche | Propriétaire |
|---|---|---|
| `DA_RunRules` : jours par période (5), périodes (10), courbes quota / spawn / délai ; `FRunCalculator` : jour → période, contrôle du quota au jour 5 seulement | T-109 | monk-island-78 |
| `URunSubsystem` : numéro de jour et de période, manifeste conservé entre les jours ; voyage jour → jour ; `L_Galleon` / zone sûre seulement au jour 5 | T-113 | 78 et 19 |
| Fin de submersion : mort des joueurs hors du galion, puis départ automatique vers le jour suivant ; équipage entier mort = défaite | T-104 (évolution), T-112 | monk-island-aa (mort), 19 (galion, barre) |
| Échelle de spawn par période appliquée par le TikiDirector, en plus de `DaySpawnScale` / `NightSpawnScale` | T-119 (suite) | monk-island-78 |
| Record dans la sauvegarde de profil, affiché dans le menu TAPAO | T-115, T-106 | 19 (UI, sauvegarde) |
| Écran de chargement « Jour N / Période P », compteur « Jour N/5 » dans le HUD | T-106, T-097 | 19 (TAPAO), monk-island-4f (compteur) |
| Recette : RUN01-RUN14 complétées par DAY01-DAY06 (voir ci-dessous) | T-116 | QA |

## Recette ajoutée
- **DAY01** : à la fin du déluge, un joueur hors du galion meurt, un joueur à bord survit.
- **DAY02** : jour N → N+1 avec une autre île ; la cale est conservée et contient les mêmes objets, avec la même valeur.
- **DAY03** : jours 1 à 4 sans vente ; au jour 5, zone sûre et vente.
- **DAY04** : quota manqué au jour 5, puis défaite ; quota atteint, puis période 2 avec plus de mobs (mesuré : nombre de spawns hostiles par minute).
- **DAY05** : la 10e période passée donne la victoire.
- **DAY06** : le record est mis à jour et affiché dans le menu après une victoire comme après une défaite, et il survit au redémarrage du jeu.
