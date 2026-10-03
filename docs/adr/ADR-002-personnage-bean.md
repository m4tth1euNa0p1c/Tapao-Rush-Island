# ADR-002 — Personnage « bean » : échelle, physique, vêtements, caméra

- Statut : **proposé, à arbitrer par le propriétaire**
- Date : 2026-10-03
- Tâches : T-022 à T-029 (lot L6)

## Contexte
Le 2026-10-03, le propriétaire a fourni la planche `sprite/player-1.png` (un bean en soldat britannique). Il demande trois choses :
- une échelle « Minion à côté d'un humain » ;
- une démarche physique et un ragdoll façon *Fall Guys* ;
- un chapeau avec physique et des skins modulaires façon *R.E.P.O.*

Le §10 du cahier fixe le contrat personnage. Le propriétaire produit les personnages ; les agents les intègrent.

## Écarts avec le cahier

| # | Cahier | Demande | Proposition |
|---|---|---|---|
| P-01 | §10 : « taille humaine cohérente avec la capsule » | 1,05 m (rapport Minion/humain) | Adopter. La capsule, les vitesses, les hauteurs d'interaction et la portée de saut deviennent des données (`DA_BeanPhysics`, `DA_ExpeditionRules`). À vérifier : sièges de la barque, portage, hauteur d'eau pour la nage (le bean nage plus tôt). |
| P-02 | §10 : « vêtements simulés hors V1 » | chapeau physique, corps mou | Adopter le chapeau (corps rigide contraint) et les basques (2 os simulés) : coût faible et contrôlé. Pas de simulation de tissu (Chaos Cloth) en V1. |
| P-03 | §2 / E-01 : 1re personne | démarche et ragdoll « comme Fall Guys », jeu en 3e personne | Garder la 1re personne (E-01). Le wobble et le ragdoll se voient par les autres et dans les reflets/ombres. Pendant un ragdoll, la caméra passe en 3e personne et suit le pelvis jusqu'au relevé. À trancher par le propriétaire. |
| P-04 | §10 : planches en pose A | planche bras le long du corps | Accepter pour le pilote ; exiger la pose A pour les variantes (mob, pnj, sniper déjà dans `sprite/`). |
| P-06 | §5 : portage V0 = un porteur, objet figé devant le joueur, physique suspendue ; transport à deux après la V1 | saisie physique façon R.E.P.O. : objet tenu par un ressort physique, distance à la molette, lancer, portage à plusieurs pour les objets lourds | Fait (T-033) : `UIslanderGrabComponent` tient l'objet côté serveur ; portée 200 cm, ligne de vue et masse revérifiées ; objet relâché une seule fois à la mort ou à la déconnexion. `bAllowSharedGrab=true` (mettre false pour revenir à un seul porteur). L'arrimage à la barque (E sur un emplacement) reste à faire en L2. |
| P-07 | §5 : chaque ressource a une valeur | objets de valeur qui perdent de la valeur à chaque choc et se cassent à zéro (R.E.P.O.) | Fait (T-033) : `UValuableComponent`, côté serveur uniquement (le client ne choisit jamais la valeur), seuils réglables. À valider avec les quotas (§7). |
| P-08 | §2 : 1re personne, bras visibles localement | coups « comme Content Warning » | Fait (T-034) : bras 1re personne visibles par leur seul propriétaire, animés en procédural (coup, garde, saisie, course) ; corps masqué pour soi, ombre conservée. |
| P-05 | Ordre des lots | personnage avant L1–L4 | Préparer le pilote en parallèle (« l'absence de personnages finaux ne bloque aucun jalon ») sans détourner les agents du noyau. Le code bean va dans un module de fichiers séparé (`Source/monke_island/Character/Bean/`) pour ne pas toucher `AIslanderCharacter` (T-021 en cours). |

## Conséquences
- Nouvelle équipe : `ue-character-artist`, `ue-character-rigger`, `ue-physics-animator`, `ue-cosmetics-programmer`.
- Nouveaux skills : `/character-pipeline`, `/character-model`, `/character-rig`, `/character-physics`, `/character-skin`.
- Toutes les valeurs de la physique du bean sont en DataAsset ; le serveur fait autorité sur l'état debout / ragdoll / relevé.
