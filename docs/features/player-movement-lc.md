# Déplacements et mêlée façon Lethal Company

- **Tâche** : T-031, lot L1 (noyau joueur) ; cahier §4 (santé et endurance 100, commandes), §5 (mêlée validée par le serveur, cooldown).
- **Hors cahier, demandé par le propriétaire le 2026-10-03** : garde au clic droit, accroupi, dégâts de chute, poids porté façon Lethal Company. Les autres commandes du cahier (E interagir, G déposer, Tab) restent à brancher avec le portage.
- **Code** :
  - `Player/IslanderMovementComponent` : endurance, épuisement, garde, poids, accroupi, sprint, garde et vol prédits par les indicateurs compressés du mouvement ;
  - `Player/IslanderHealthComponent` : santé répliquée, dégâts, chute, mort ;
  - `Player/IslanderCombatComponent` : coup de poing et garde ;
  - `Player/IslanderHUD` : HUD provisoire ;
  - `Character/Bean/BeanPuppetComponent` : visuels du bean.
- **Tests** : `monke_island.Player.Movement` (endurance, chute, garde).

## Commandes
| Action | Touche | Effet |
|---|---|---|
| Courir | Maj (maintenir) | 6 m/s ; coûte 15 d'endurance par seconde ; impossible accroupi, en garde ou épuisé ; relève le personnage s'il est accroupi |
| Sauter | Espace | coûte 10 d'endurance ; impossible avec moins de 10 ; double appui = vol (outil T-021) |
| S'accroupir | C ou Ctrl (bascule) | 1,5 m/s, capsule de 120 cm ; le bean s'écrase (×1,12 en largeur, ×0,72 en hauteur) |
| Nager | automatique dans l'eau | coûte 4 d'endurance par seconde (cahier §4) ; Espace maintenu pour remonter, C ou Ctrl maintenu pour plonger ; épuisé = nage ×0,6 |
| Frapper | clic gauche | préparation visible 0,25 s, puis coup ; 20 dégâts, recul 3,5 m/s ; cooldown 0,7 s ; coûte 10 d'endurance ; impossible en garde, en nage ou en vol |
| Vue | R | bascule 3e / 1re personne : caméra aux yeux du bean (devant la visière), le corps suit le regard, tricorne caché pour soi, 3e personne forcée en ragdoll |
| Garde | clic droit (maintenir) | marche ×0,5, pas de course ; un coup reçu de face (cône d'environ 78° de chaque côté) ne fait que 25 % des dégâts et coûte 15 d'endurance ; sans endurance, la garde casse |

## Règles
- **Endurance** :
  - elle revient de 10 par seconde en marchant et de 20 par seconde à l'arrêt, après un délai de 0,8 s ;
  - à 0, le personnage est **épuisé** : pas de course, marche ×0,85, nage ×0,6 ;
  - il le reste jusqu'à remonter à 35 %.
- **Poids** (prévu pour le portage) : −0,8 % de vitesse par kg, jusqu'à un minimum de ×0,45 ; +2 % de coût d'endurance par kg.
- **Chute** : aucun dégât sous 10 m/s (environ 5 m de chute), mort à partir de 20 m/s (environ 20 m), linéaire entre les deux.
- **Mort** : ragdoll vu par tous les joueurs (multicast), commandes coupées. Réapparition au PlayerStart après 6 s : c'est **provisoire**, en attendant le mode spectateur et les règles de fin d'expédition (L5).

Toutes les valeurs sont réglables dans les Blueprints (`EditDefaultsOnly`) ; elles passeront dans `DA_ExpeditionRules` avec L1.

## Réseau
- **Prédit côté client** (indicateurs compressés du mouvement) : sprint, garde et vol. Accroupi passe par l'indicateur du moteur.
- **Décidé par le serveur** : coup de poing (cooldown et endurance revérifiés, balayage, dégâts, recul), santé, chute et mort.
- **Endurance** : calculée à la fois par le client propriétaire et par le serveur, sans réplication. Un léger écart est possible près de l'épuisement ; le serveur corrige alors la vitesse.
- **Vu par les autres joueurs** : chaque coup est rejoué via `PunchCount`, et la garde passe par `bReplicatedGuard`.

## Recette
- [x] Compilé (UBT, 0 erreur, 0 avertissement) ; 14/14 tests `monke_island.Player` verts (headless).
- [ ] Testé en éditeur : barres du HUD, sprint jusqu'à l'épuisement, accroupi (bean écrasé), saut refusé sans endurance, coup de poing (bras armé puis tendu), garde (avant-bras levés), nage (Espace et C ou Ctrl), chute de plus de 5 m, mort et réapparition.
- [ ] Testé hôte + client : coup de poing et garde vus par l'autre joueur, dégâts appliqués par le serveur uniquement, aucune correction de position permanente en sprint ou en garde.

## Limites connues
- Le coup de poing touche les pawns et les corps physiques, pas la végétation (`PunchObjectTypes`).
- Le mannequin source n'a pas d'animations d'accroupi ni de nage : la posture du bean vient du composant `BeanPuppet`.
- Pas encore d'échelles, de son de pas ni d'emotes : à planifier.
