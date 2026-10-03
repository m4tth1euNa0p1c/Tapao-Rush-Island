# Crownfall Expeditions — Cahier des charges et mandat pour agents Claude

Version 1.2 — 3 octobre 2026. Nom provisoire. Document de référence à déposer dans `Docs/CAHIER_DES_CHARGES.md` du futur dépôt.

## 1. Objectif et décisions initiales

Créer un jeu coopératif d’extraction maritime : un équipage mandaté par une Couronne fictive débarque sur une île condamnée, transporte des ressources physiques vers sa barque et repart avant la submersion. Le galion sert de hub entre les expéditions. La tension vient de la coordination entre pacification PvE, collecte, transport et fuite : conquérir du temps pour atteindre les quotas, puis choisir quand abandonner l’île.

Les personnages incarnent des colons/explorateurs dans un univers maritime fictionnel inspiré de l’époque des galions. Les factions insulaires ont des motivations et comportements distincts ; elles ne sont pas toutes hostiles. Le ton combine aventure, peur nocturne, chaos coopératif et satire de la bureaucratie royale.

Décisions proposées, exécutables par défaut et modifiables par le propriétaire :

| Sujet | Base de travail |
|---|---|
| Plateforme | Windows PC, clavier/souris, interface française |
| Moteur | Unreal Engine 5, C++ pour règles et composants, Blueprints pour assemblage et réglage |
| Version | Utiliser la version stable installée et compatible ; enregistrer la version exacte avant tout développement, aucune mise à niveau automatique |
| Caméra | Première personne ; corps visible par les autres joueurs, bras visibles localement |
| Joueurs | Solo et coop 2–4 ; listen server, un joueur héberge |
| Expédition | 120 s avant début de submersion ; +120 s par zone pacifiée, horloge commune à l’équipe |
| Direction artistique | Stylisée, personnages aux silhouettes lisibles, ambiance maritime inquiétante |
| Monde | Archipel fictif ; îles faites à la main et variantes de placement avant génération procédurale |
| Contrôle réseau | Serveur autoritaire pour objets, dégâts, temps, récompenses et extraction |
| Livraison | Prototype jouable puis V1 ; aucune publication commerciale incluse dans ce mandat |

Les valeurs ci-dessous sont des paramètres de départ à tester, pas un équilibrage définitif. Les mettre dans des Data Assets ou configurations, pas dispersées dans le code.

## 2. Responsabilités

**Propriétaire :** produire les références et modèles des personnages, choisir leur apparence, tester le plaisir de jeu, arbitrer les modifications de vision.

**Agents Claude :** créer le dépôt local et sa documentation, implémenter le jeu, fabriquer les environnements et objets provisoires, intégrer les assets reçus, tester et fournir des builds et procédures reproductibles. L’absence de personnages finaux ne bloque aucun jalon.

Un agent ne peut pas déclarer un build validé sans l’avoir exécuté. Si Unreal, le compilateur ou l’éditeur est absent, il fournit le code, les étapes restantes et un statut explicite « non compilé / non testé ». Les tâches d’éditeur nécessaires restent identifiées et assignées ; aucun asset `.uasset` ne doit être prétendu créé par simple rédaction de code.

## 3. Boucle et états de partie

`Hub → Préparation → Chargement île → Expédition → Extraction → Bilan → Hub`.

- Au hub, équiper l’équipage, consulter les quotas et lancer le départ quand tous sont prêts.
- La barque apparaît près de la plage ; arrivée en galion et descente animée sont une mise en scène ultérieure.
- Sur l’île, se répartir entre pacification des zones, récupération de ressources dans l’environnement et sur les ennemis, transport et protection des itinéraires ; ramener physiquement le butin.
- Au départ, les ressources sécurisées sont figées ; le bilan applique quotas, vente et taxe.
- Les survivants et les morts reviennent au hub à la prochaine expédition. Pas de respawn sur l’île en V1.
- Si tous meurent, bilan de défaite : aucun butin de l’expédition, équipement emporté perdu, progression antérieure conservée.
- Une partie ne change d’état qu’une fois. Récompenses et sauvegarde sont idempotentes par identifiant d’expédition.

## 4. Joueur et interactions

Santé 100 ; endurance 100. Course et nage consomment l’endurance, marche et repos la régénèrent. À zéro, course impossible, nage ralentie mais pas mort instantanée. La faim reste une fonctionnalité ultérieure.

Commandes par défaut : ZQSD, souris, espace sauter, Maj courir, E interagir, G déposer, clic gauche utiliser outil, Tab objectifs/cargaison, Échap menu. Toutes les actions passent par Enhanced Input et peuvent être remappées.

L’interaction affiche une cible et une action seulement à portée et avec une ligne de vue. Portée initiale 200 cm. Le serveur revérifie distance, visibilité, état du joueur, outil requis et disponibilité de l’objet. Un client ne choisit ni la valeur du loot ni ses propres dégâts.

Les morts deviennent spectateurs, sans influencer l’expédition. Le corps et le transport à deux joueurs sont prévus après la V1.

## 5. Objets physiques et cargaison

Chaque ressource possède un identifiant stable, une catégorie, une masse, une valeur, une quantité et un état : `Libre / Portée / Arrimée / Perdue`.

- V0 : un objet porté à la fois, en position fixe devant le joueur, avec physique suspendue pendant le portage ; retour à la physique au dépôt.
- Le ralentissement dépend de la masse. Pas de contrainte physique complexe bras/objet pour le premier prototype.
- Deux joueurs demandant le même objet : le serveur accorde une seule prise ; l’autre reçoit un refus lisible.
- À la mort ou déconnexion du porteur, l’objet est libéré une seule fois à la dernière position valide.
- Les objets libres simulent la physique ; seuls les changements utiles sont répliqués. Mode de réplication choisi et testé sur la version moteur retenue.
- Pour sécuriser un objet dans la barque, le joueur vise un emplacement libre et utilise E. Le serveur arrime l’objet, coupe sa simulation et le compte dans la cargaison.
- Un objet simplement jeté sur la barque reste libre et n’est pas comptabilisé. L’interface le signale.
- Capacité initiale : 12 emplacements, 150 kg ; les joueurs ont leurs sièges séparés. Limites réglables.
- Désarrimer est possible avant l’extraction ; aucun double comptage entrée/sortie.

| Ressource initiale | Collecte | Masse indicative | Quantité/valeur |
|---|---|---|---|
| Bois | Bûches déjà au sol en V0 ; arbre + hache ensuite | 10 kg | 1 unité de bois |
| Nourriture | Caisses ou sacs | 5 kg | 1 unité de nourriture |
| Richesses | Coffre récupérable | 20 kg | 100 pièces de vente |

Minerais, perles, reliques, objets nécessitant deux porteurs et destruction avancée restent des extensions.

## 6. Pacification, temps gagné, mobilité et fuite stratégique

Cette section remplace le timer fixe de 5 minutes. Les 2 minutes initiales sont le délai avant le début de la montée des eaux, pas une mort automatique à 2 minutes.

### 6.1 Horloge commune et montée des eaux

- Tous les joueurs partagent une échéance serveur de début de submersion : `FloodStartTime = ExpeditionStartTime + 120 s`.
- Chaque zone pacifiée une première fois ajoute exactement 120 s à cette échéance, pour toute l’équipe. Aucun bonus individuel, aucune remise à zéro du timer.
- Exemple : une zone validée à t=75 s déplace le début de submersion de t=120 à t=240 ; il reste alors 165 s. Deux zones validées donnent une échéance à t=360.
- Première île : trois zones pacifiables, donc 8 minutes maximum avant montée des eaux. Ce nombre est un réglage de prototype, pas une limite universelle.
- À l’échéance, la submersion commence et devient irréversible. Les validations de zone postérieures n’ajoutent plus de temps. Alerte explicite sur l’interface.
- La montée finale dure 60 s par défaut, couvre progressivement les terres accessibles puis déclenche l’extraction automatique. Avec zéro zone pacifiée, la résolution finale arrive donc à t=180 s.
- Une réussite n’est créditée que si sa validation serveur précède strictement le début de submersion. À égalité, la montée des eaux prime. Les zones réussies dans le même tick sont traitées une fois chacune.

Le niveau d’eau de gameplay suit `WaterHeight(max(0, ServerTime - FloodStartTime))`. Avant submersion, eau basse ; ensuite interpolation monotone sur 60 s. Rendu et immersion utilisent cette même référence. Ne pas supposer que déplacer un WaterBody met à jour nage, navigation et flottabilité : prototyper ; surface dédiée et tests d’immersion autorisés en repli.

UI : temps avant submersion, notification « Zone sécurisée : +2:00 », zones restantes, progression des quotas arrimés, puis décompte d’évacuation distinct. Le jour/nuit dépend du temps réellement écoulé, pas du timer restant ; la pacification ne rembobine pas la météo. Crépuscule à 120 s, nuit à 240 s par défaut. La tempête finale dépend du début de submersion.

### 6.2 Pacifier une zone

Une zone est un objectif tactique, pas uniquement un groupe d’ennemis à éliminer. États : `Non découverte / Contestée / Sécurisation / Pacifiée / Submergée`.

Prototype : éliminer les menaces désignées, puis interagir avec le socle de zone pour planter un drapeau britannique (Union Jack, Royaume-Uni dans la fiction). Le drapeau est fourni comme équipement de mission, ne consomme pas un emplacement de cargaison et ne donne aucune récompense à lui seul. Sa plantation reste visible pour toute l’équipe.

Un joueur vivant au pied du drapeau maintient ensuite l’action « Réciter le sermon » pendant **20 secondes consécutives sans bouger**. Le sermon est une animation et une voix préenregistrée ou provisoire déclenchées dans le jeu ; il ne nécessite ni microphone ni reconnaissance vocale. Une alternative accessible Basculer est définie en section 18, sans changement des règles de jeu. Sous-titres et barre de progression visibles ; les coéquipiers entendent le sermon à proximité. Le timer global continue à diminuer.

Le joueur officiant ne peut pas marcher, courir, sauter, nager, attaquer ou porter un objet pendant le rituel. Regarder autour de lui est autorisé. Déplacement volontaire ou forcé, dégâts reçus, mort, déconnexion, relâchement de l’action ou entrée d’un ennemi désigné vivant dans le périmètre interrompent le sermon et remettent sa progression à zéro. Le drapeau reste planté ; un joueur peut recommencer depuis zéro. Tolérance technique initiale de position : 10 cm autour du point de départ, pour absorber le jitter sans permettre de se déplacer réellement ; contrôle serveur.

Un seul officiant par drapeau ; les autres protègent, combattent ou collectent. Deux joueurs ne cumulent pas leur durée et un changement d’officiant repart de zéro. Après les 20 s validées par le serveur, la zone devient pacifiée et accorde +120 s une seule fois à toute l’équipe, uniquement avant le début de submersion. En solo, les vagues et placements doivent offrir une fenêtre réalisable pour le rituel.

Les ennemis nécessaires sont identifiés au début de l’objectif : aucun ennemi supplémentaire généré à l’infini ne bloque sa complétion. Un ennemi coincé déclenche un diagnostic et une procédure de récupération, pas une validation arbitraire cachée. La zone reste pacifiée pour l’expédition ; d’autres ennemis peuvent traverser, sans nouveau bonus.

Le serveur valide les conditions et attribue le bonus une seule fois par `(ExpeditionId, ZoneId)`. La mort, déconnexion ou interruption de l’officiant réinitialise la progression du sermon. Validation simultanée par plusieurs joueurs = un seul bonus. Réussite pendant un préavis de départ ne l’annule pas et ne réinitialise pas le vote.

Chaque zone ouvre un intérêt concret : bois et passage court, nourriture et abri, richesses et accès à une crique d’extraction. Les routes sont débloquées immédiatement ; les ressources restent à récolter et transporter. La pacification seule ne remplit pas les quotas.

### 6.3 Synchronisation de l’équipe et PvE

Rôles temporaires, sans classes imposées : sécuriser/combattre, récolter, porter, protéger/repositionner la barque. Les joueurs peuvent changer de rôle à tout moment. À quatre, deux peuvent sécuriser pendant qu’un récolte et qu’un transporte ; en solo, vagues, quotas et temps de sécurisation sont ajustés par configuration.

Les ressources viennent des points de récolte et de loot physique sur les ennemis désignés. Table de loot configurable et seedée, un seul drop par ennemi. L’objet suit les mêmes règles de portage/arrimage et n’est compté que dans la barque. Le niveau doit garantir des quotas atteignables sans dépendre exclusivement d’un drop aléatoire rare.

Le combat minimal appartient désormais au noyau : un ennemi terrestre simple, une attaque de mêlée avec préparation visible, validation des coups/dégâts par le serveur, cooldown, mort et drop. Le port d’un objet encombrant empêche l’attaque ; il faut le déposer. Animations et modèles provisoires acceptés. Armes à feu, mini-boss et factions complexes restent des extensions.

Dangers environnementaux : passages étroits, zones de nage, relief, endurance et lignes de vue influencent les déplacements. La pacification sécurise un objectif, pas toute l’île ; requins, météo et submersion restent menaçants.

### 6.4 Placement stratégique de la barque

La barque amarrée immobile de la version précédente est remplacée par un repositionnement contrôlé entre trois mouillages prédéfinis : plage initiale, crique près des ressources, sortie de repli. Mouillages visibles sur carte/repères ; chaque placement échange proximité du loot contre sécurité et longueur des trajets.

Un mouillage est accessible si ses conditions sont remplies (route/zone sécurisée, profondeur et accès). Prévoir au moins un mouillage disponible pendant toute la fenêtre finale de 60 s. Depuis la barque, un joueur lance un déplacement de 15 s sur trajectoire prédéfinie, approuvé par majorité des vivants, ou directement en solo. L’interface annonce destination et arrivée à toute l’équipe.

Pendant le déplacement : cargaison arrimée et passagers se déplacent avec la barque ; arrimage, désarrimage et embarquement sont désactivés ; les joueurs sur l’île doivent rejoindre la nouvelle position. Refuser un trajet qui ne peut se terminer avant la résolution finale. Ne pas démarrer repositionnement et extraction simultanément. Si la submersion débute pendant un trajet déjà validé, celui-ci continue jusqu’au mouillage prévu. La navigation physique libre à rames reste une extension ; le choix tactique du placement appartient au prototype.

### 6.5 Quotas et départ

Le quota est un objectif partagé, affiché à partir des objets réellement arrimés. Dès qu’il est rempli, afficher « Contrat rempli : extraction conseillée » ; ne pas téléporter l’équipe ni la forcer à partir. Elle peut chercher du surplus à ses risques.

Un joueur embarque près de la barque immobile, sans objet tenu, dans un siège libre. Debout sur la coque ou dans l’eau ne compte pas comme embarqué. Désarrimer du loot peut faire repasser le quota sous sa cible avant départ.

Départ : tous les vivants embarqués ou vote majoritaire des vivants avec au moins un joueur embarqué. Préavis de 5 s, cargaison et passagers figés à la fin. Vote caduc si personne n’est embarqué. Départ autorisé même sans quota rempli, avec avertissement et contrat échoué ; une fuite d’urgence reste toujours possible.

À la fin des 60 s de submersion, le serveur extrait les passagers embarqués et le loot arrimé ; les autres meurent. L’échéance interdit toute interaction tardive. Une extraction confirmée abandonne les autres vivants et termine l’expédition pour tous. Aucun bonus de pacification ne remet en jeu une expédition résolue.

## 7. Quotas, taxe et progression

Le quota porte sur les quantités, la taxe sur l’or obtenu par vente. Séparer ces deux mécanismes dans le bilan.

Paramètres V1 : 4 bois, 3 nourritures, taxe 35 %, bonus de contrat 100 pièces lorsque tous les quotas sont remplis. Quotas adaptés au nombre de joueurs via configuration.

Ordre de calcul :

1. Retenir seulement les ressources extraites et arrimées.
2. Affecter bois/nourriture au quota ; consommer les quantités remises.
3. Ajouter les surplus de bois/nourriture à la réserve commune.
4. Vendre les richesses ; `taxe = floor(valeur_vente × taux)`.
5. Créditer `valeur_vente − taxe + bonus_contrat`. Bonus non taxé par défaut.
6. Sauvegarder le bilan une fois et montrer chaque ligne à tous.

Exemple : 500 pièces de richesses et quotas remplis donnent 175 de taxe, 100 de bonus et 425 pièces nettes. Quota manqué : pas de bonus et un avertissement. Dette royale, saisies et chasseurs de primes restent hors V1.

La monnaie et le galion sont communs à l’équipage. Seul l’hôte confirme les achats en V1, avec affichage partagé. Une sauvegarde locale de l’hôte stocke version de schéma, or, stocks, améliorations et contrats. Écriture sûre et reprise si sauvegarde corrompue ; ne pas écraser silencieusement les données. Pas de reprise en milieu d’expédition en V1 ; marqueur de départ et récupération après abandon/crash définis en section 17.

Boutique initiale : hache 100, lanterne 150, +4 emplacements de barque 300. Coûts réglables, contrôle serveur, achats insuffisamment financés refusés. Armes à feu et agrandissement visuel du galion ensuite ; attaque de mêlée simple disponible dès le noyau PvE.

## 8. Dangers, événements et voix

V1 : un requin avec états patrouille, détection, poursuite, attaque et retour. Il cible uniquement les joueurs immergés ; aucun déplacement sur terre. Paramètres : portée de détection 12 m, attaque 25 dégâts, délai 2 s. Mort, sortie d’eau et perte de cible interrompent proprement la poursuite. Les zones immergées doivent devenir navigables pour lui sans exiger un navmesh terrestre.

Lanterne/torche : lumière tenue, réduction de capacité de portage ou nécessité de la déposer pour porter un gros objet. La nuit doit être inquiétante mais les itinéraires restent perceptibles. Tonnerre et flashs limités ; prévoir contrôle de volume et option réduisant les flashs.

La pacification décrite en section 6 est une mécanique centrale, présente dès le prototype. Les événements secondaires peuvent ensuite proposer d’autres objectifs et récompenses ; ils ne donnent du temps que si leur configuration le prévoit explicitement. Variantes religieuses/politiques fictionnelles possibles. Factions complexes, mini-boss, cyclone et tsunami restent des lots séparés.

Le directeur d’événements possède un budget, un seed et des délais minimums. Il ne fait pas apparaître un danger inévitable sur un joueur et ne punit pas systématiquement la réussite. Toute menace dispose d’un signal préalable et d’une possibilité de réaction.

Rejoindre un serveur privé et voix en ligne = fonctionnalités secondaires, lot après validation du réseau gameplay. Distance courte sur l’île et canal équipage au hub, push-to-talk, mute individuel, indicateur micro, volume et choix de périphérique. La solution technique doit faire l’objet d’un prototype à deux machines, avec dépendances/licences documentées. Ne pas promettre une VoIP prête par simple activation de la réplication Unreal. V1 peut être jouée avec une voix externe en attendant ce lot.

## 9. Architecture Unreal et dépôt

`Crownfall.uproject`, `Config/`, `Content/Crownfall/`, `Source/Crownfall/`, `Docs/`, `Tools/`, `CLAUDE.md`, `.gitignore`, `.gitattributes`.

Modules fonctionnels du code : Player, Interaction, Items, Boats, Expedition, World, Economy, AI, UI, Save. Composants pour santé, endurance, interaction et portage. Pas de framework multi-module complexe sans besoin démontré.

- GameMode : transitions et décisions serveur.
- GameState : phase, heure de départ, échéance de submersion modifiable, durée finale, état/progression des zones, drapeaux et officiants, niveau d’eau, quotas arrimés et destination de barque visibles aux clients.
- PacificationZone : menaces désignées, plantation du drapeau, verrou d’officiant, position de référence, sermon ininterrompu et attribution unique du temps.
- BoatPlacementManager : mouillages, votes, trajectoires, durée et conflits avec extraction.
- PlayerState : identité, vivant/mort, prêt et embarqué.
- PlayerController : entrées, UI et requêtes serveur.
- Data Assets : ressources, paramètres d’expédition, îles et améliorations.
- SaveGame : progression du galion appartenant à l’hôte.

Les clients calculent l’affichage du timer à partir du temps serveur synchronisé ; aucun timer local ne décide de la mort. Prévoir validation serveur, pertinence réseau, limitation des requêtes et événements fiables uniquement lorsque nécessaire. Éviter les RPC fiables à chaque frame.

Git LFS pour `.uasset`, `.umap` et sources artistiques volumineuses. Ignorer Binaries, Intermediate, DerivedDataCache et Saved. Les maps et Blueprints binaires ont un seul agent responsable à la fois. Code et documentation en anglais possible, UI française. Maintenir les licences/provenances des assets.

Références officielles à reconsulter pour la version retenue :
- https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/networked-physics-overview

## 10. Personnages : contrat de livraison pour le propriétaire

Une image générée est une référence, pas un personnage 3D animé. La chaîne attendue est : références → mesh → retopologie/UV → textures → rig → skinning → animations/retargeting → import Unreal → test.

Pour chaque personnage, livrer une fiche avec identifiant, taille, rôle visuel, palette et chemins des fichiers. Générer d’abord un personnage pilote pour valider le pipeline, puis trois variantes cosmétiques. Tous ont les mêmes statistiques en V1.

Références : face, profil et dos orthographiques cohérents, pose A, fond neutre, éclairage uniforme, silhouette complète, mains et pieds visibles, aucun outil dans les mains. Fournir détails vêtements/visage séparément. Les trois vues doivent représenter exactement la même tenue.

Modèle : source Blender et export FBX avec squelette pour personnage animé ; GLB accepté pour revue mais le chemin d’import final doit être vérifié. Convention Unreal : centimètres, Z vertical, orientation avant normalisée à l’import. Taille humaine cohérente avec la capsule. Transformations appliquées, origine aux pieds. Éviter géométrie cachée excessive, doigts fusionnés et accessoires pénétrant le corps.

Budget initial proposé : 30–60k triangles en LOD0, LODs réduits à tester, textures 2K, 2–4 matériaux. Maps Base Color, Normal et canaux de rugosité/métallicité/occlusion identifiés. Aucun budget n’est une garantie de performance.

Rig : squelette partagé ou retargeting validé sur le squelette du projet ; séparation des bras FPS si nécessaire. Sockets objets main droite/main gauche et dos. Animations minimum : repos, marche, course, saut, nage, portage et mort ; provisoires acceptées. Rig facial et vêtements simulés hors V1.

Recette personnage : import sans erreur, bonne échelle, pieds au sol, aucune déformation majeure aux épaules/coudes/genoux, prise d’objet alignée, corps masqué dans la caméra FPS sans disparaître pour les autres, ombres correctes, quatre personnages affichables simultanément. Agents chargés du nettoyage et du retargeting lorsque les outils le permettent.

## 11. Lots de livraison et critères de passage

| Lot | Livrable | Critère de passage |
|---|---|---|
| L0 | Projet, version moteur, architecture, map grise, commandes build | Projet ouvre et compile ; dépendances et limites documentées |
| L1 | Déplacement, interaction, prise/dépôt | Deux joueurs peuvent prendre des objets distincts ; même objet jamais dupliqué |
| L2 | Barque, sièges, arrimage, capacité et repositionnement entre mouillages | Stock partagé sans duplication ; déplacement annoncé, cargaison conservée, nouveau point de retour visible |
| L3 | PvE simple, zones pacifiables, 120 s initiales, bonus +120 s, submersion finale 60 s, extraction | Bonus unique partagé, aucune baisse de l’eau, collecte environnement/ennemis, boucle solo puis hôte + client |
| L4 | Hub, quotas, taxe, achats, sauvegarde | Exemple économique exact, relancement conserve la progression, bilan non rejoué |
| L5 | 4 joueurs, endurance, nuit, lanterne, requin | Partie à 4 terminable, danger lisible, états cohérents sur les clients |
| L6 | Build V1 et intégration personnages | Build Windows joué hors éditeur, procédure d’installation et rapport de recette |
| L7 | Sessions internet et VoIP | Deux machines sur réseaux distincts rejoignent et communiquent ; mute et départ propres |
| L8+ | Îles variées, événements, factions, catastrophes, upgrades avancés | Lots proposés séparément avec recette et coût avant intégration |

L1–L4 forment le prototype central. L5–L6 forment la V1 coop LAN/direct-IP. L7 rend l’expérience internet complète. Ne pas confondre test réseau dans l’éditeur et session accessible sur internet. Aucune estimation calendrier ferme avant L0–L2 et mesure des obstacles.

## 12. Recette obligatoire

Tests économiques automatisés : arrondis de taxe, quota insuffisant, stock consommé, achat refusé, double résolution. Tests d’intégration et recette manuelle :

1. Solo : prendre trois ressources, arrimer, embarquer, extraire, bilan correct.
2. Hôte + client : demandes simultanées sur une ressource, un seul gagnant.
3. Déconnexion pendant portage : une seule ressource au sol, aucun porteur fantôme.
4. Mort : objets lâchés, spectateur, retour au hub suivant.
5. Barque pleine : refus clair, aucun objet perdu par l’interface.
6. Arrimage puis désarrimage répété : quantité et valeur inchangées.
7. À l’échéance : joueur embarqué survit, joueur sur plage/meuble/coque meurt.
8. Départ anticipé : vote, préavis, abandon et bilan concordent sur tous les clients.
9. Deuxième expédition : timer, zones, ennemis, votes et eau réinitialisés ; aucun loot de l’île précédente.
10. Réseau dégradé : profils 100 puis 200 ms et 1–3 % de pertes ; pas de duplication ni désaccord final, anomalies visuelles consignées.
11. Quatre joueurs : run complet puis achats et relancement.
12. Départ de l’hôte : session terminée avec message ; migration d’hôte hors scope.
13. Sans pacification : montée à 120 s et résolution à 180 s ; aucune mort instantanée à 120 s.
14. Zone validée à 75 s : échéance de montée à 240 s ; seconde zone = 360 s. Tous affichent les mêmes valeurs.
15. Double validation/sortie et retour : un seul bonus par zone ; deux zones simultanées donnent chacune +120 s.
16. Validation tardive ou à égalité avec l’échéance : aucun bonus ; eau ne redescend jamais.
17. Ennemis morts une fois : un seul drop, loot transportable et aucune ressource créditée avant arrimage.
18. Drapeau planté : aucun bonus avant sermon. Officiant immobile 20 s = validation ; mouvement, dégâts, relâchement ou changement d’officiant = remise à zéro. Regard caméra et jitter inférieur à la tolérance ne coupent pas le rituel. Pacification terminée non farmable.
19. Repositionnement : passagers/cargaison conservés, équipe à terre informée, trajet trop tardif refusé ; conflit extraction/déplacement impossible.
20. Quota rempli puis objet désarrimé : affichage actualisé ; départ sans quota possible et pénalité contractuelle correcte.
21. Solo, deux et quatre joueurs : au moins un parcours combat/collecte/extraction atteignable, démontré par playtest.

Cible de performance proposée : 60 FPS en 1080p sur machine de référence à préciser au L0, avec 4 joueurs, 50 ressources actives, un requin et les ennemis terrestres des trois zones ; mesurer leur nombre maximal au L0. Rapporter médiane et 1 % bas, configuration graphique, CPU/GPU/RAM et bande passante. Ne pas déclarer cet objectif atteint avant mesure.

## 13. Organisation des agents Claude

L’orchestrateur peut créer des agents spécialisés : architecture/intégration, gameplay, réseau/physique, monde/dangers, UI/économie, QA/build, assets. Ils travaillent sur contrats partagés, branches ou worktrees distincts. L’agent réseau révise les systèmes serveur avant fusion ; QA vérifie indépendamment les critères.

Chaque tâche comporte : ID, objectif, lot, dépendances, fichiers autorisés, contrat public, critères de recette, preuves et statut. Les agents ne modifient pas simultanément la même map ou le même Blueprint. Un intégrateur fusionne et garde le projet compilable.

Statuts : À faire / En cours / À vérifier / Validé / Bloqué. « Code écrit » n’est pas « Validé ». Le rapport distingue compilé, testé dans l’éditeur, testé en build et testé à plusieurs machines.

Documents du dépôt : README (ouvrir/jouer/compiler), architecture, décisions ADR, backlog, suivi des assets, matrice de recette, problèmes connus, guide d’intégration personnage. Une fonction absente reste signalée ; pas d’implémentation vide présentée comme terminée.

## 14. Prompt maître à transmettre à Claude Code

```text
Tu es l’orchestrateur technique de Crownfall Expeditions. Lis Docs/CAHIER_DES_CHARGES.md entièrement et applique-le comme contrat de produit. Le propriétaire produit les personnages ; utilise des placeholders tant que les assets ne sont pas intégrables.

Inspecte le dépôt et l’environnement avant toute modification. Ne remplace pas un projet existant. Identifie version Unreal, compilateur, accès à l’éditeur, plugins et méthode de build ; consigne ce qui est disponible et ce qui manque. Si aucun projet n’existe, prépare la structure et une création reproductible adaptée à la version installée.

Crée des agents spécialisés lorsque l’environnement le permet : architecture/intégration, gameplay, réseau/physique, monde/dangers, UI/économie, QA/build, assets. En l’absence de sous-agents, assure successivement ces rôles et indique-le. Affecte des périmètres sans collision, en particulier pour maps et Blueprints.

Lis également les sections 16–23 : elles définissent menus, paramètres, sauvegardes, éditeur et contrats Blueprints. Commence par L0, puis L1–L4 dans l’ordre des dépendances. Conçois la réplication dès L1. Ne développe pas les fonctionnalités L7/L8 avant validation du noyau. N’ajoute pas de plugin payant ni ne dépense d’argent sans accord ; préfère un repli documenté. Ne publie pas le dépôt ni le jeu sans demande explicite.

Avant chaque lot : tâches, contrats, critères et responsable d’intégration. Après chaque lot : compiler si possible, exécuter la recette appropriée, fournir preuves, problèmes connus et commandes reproductibles. Ne prétends jamais avoir ouvert Unreal, créé un uasset, testé un réseau ou livré un build si cela n’a pas été fait. Liste précisément les opérations d’éditeur restant à exécuter et automatise-les seulement avec des API effectivement disponibles.

Prends les décisions réversibles courantes sans interrompre le travail ; consigne-les dans les ADR. Demande un arbitrage seulement pour changement majeur de vision, coût externe ou blocage sans repli. La première réussite attendue est une expédition à deux joueurs : commencer avec 120 s avant submersion, pacifier une zone en plantant le drapeau britannique puis en récitant un sermon immobile pendant 20 s pour gagner 120 s une seule fois, récupérer du loot environnemental et ennemi, arrimer jusqu’au quota, repositionner la barque puis extraire avant résolution finale, recevoir un bilan exact et recommencer. Implémente la section 6 dès le noyau ; ne conserve aucun ancien timer fixe de 300 s.
```

## 15. Définition de fini

Le prototype est terminé lorsque la boucle L1–L4 est jouable et vérifiée en hôte + client. La V1 est terminée lorsque L5–L6 et la recette associée passent, avec défauts connus documentés. L’expérience internet/voix exige L7. Les personnages finaux constituent un livrable d’intégration distinct : leur absence n’empêche pas de démontrer le gameplay, mais leur conformité doit être vérifiée avant une livraison artistique finale.


## 16. Navigation complète et menu principal

Le menu est un port au crépuscule avec galion en arrière-plan, animé légèrement, lisible avant tout. Musique et animation respectent les réglages audio/mouvement. Afficher le nom du jeu et la version du build. Navigation utilisable souris et clavier, focus visible et retour cohérent ; prise en charge manette prévue dans les widgets, validée lorsqu’elle est intégrée.

| Entrée | Parcours | Priorité |
|---|---|---|
| Continuer | Dernière campagne valide → hub | V1, masquée sans sauvegarde |
| Nouvelle partie | Choisir Jouer ou Éditeur d’île | Jouer V1 ; éditeur E1 |
| Charger | Onglets Campagnes / Projets d’îles | Campagnes V1 ; projets E1 |
| Rejoindre | Partie privée par invitation/code ; LAN/adresse pour développement | Secondaire, L7 pour internet |
| Paramètres | Affichage, graphismes, audio, commandes, interface, accessibilité | V1, voix avec L7 |
| Crédits | Équipe, contributions, assets, musiques et licences | Secondaire, avant distribution |
| Quitter | Fermer proprement le jeu | V1 |

Une entrée différée est désactivée avec une explication courte, jamais un bouton sans effet. Continuer n’apparaît que si une campagne compatible existe ; une ancienne campagne corrompue ne doit pas empêcher de charger une autre.

Nouvelle partie → Jouer : nom de campagne (1–32 caractères), emplacement libre, personnage cosmétique, préréglage Standard ou Personnalisé, seed optionnel, mode Solo ou Héberger en privé (lorsque disponible), puis Créer. Afficher le résumé avant création ; ne jamais écraser automatiquement une campagne. Après création réussie, charger le galion et son tutoriel contextuel. Les réglages Personnalisés sont enregistrés dans la campagne et signalés au lobby : 120 s initiales, 120 s par zone, 20 s de sermon et 60 s de submersion sont les valeurs Standard ; un profil personnalisé peut les modifier dans des bornes validées.

Charger : nom, date locale de dernière sauvegarde, temps joué, or, expéditions effectuées, préréglage, version et vignette. Actions Charger, Renommer, Dupliquer, Supprimer. Suppression = confirmation nominative ; échec lecture = message et proposition de sauvegarde de secours. Sauvegarde d’une version future : refus avec raison, aucune conversion destructive. Conversion ancienne : copie de secours et migration testée.

Rejoindre : saisir invitation/code selon le fournisseur retenu, ou choisir une session LAN ; état Recherche/Connexion/Chargement/Échec. Erreurs distinctes : introuvable, complet, version incompatible, session déjà en expédition, connexion expirée, invitation non valide. Annuler ramène au menu et nettoie la connexion. Aucun navigateur public ni matchmaking requis. Une campagne appartient à son hôte ; rejoindre n’importe pas la progression commune dans la sauvegarde personnelle du client. Rejoindre seulement au hub en V1 ; arrivée en cours d’expédition différée.

Lobby intégré au galion : 4 places, noms, personnages, prêt/pas prêt, état de chargement ; invitations lorsque disponibles. Hôte sélectionne contrat/île et lance après accord des présents. Délai de chargement maximal proposé 60 s : joueur bloqué retiré avec motif, ou retour au hub si l’hôte échoue. Un joueur tardif ne lance pas le timer avant les autres. Aucun coût d’équipement n’est engagé avant succès du chargement de l’expédition.

Menu pause : Reprendre, Objectifs, Équipage, Paramètres, Retour au menu, Quitter. Solo : simulation et horloge suspendues ensemble. Multijoueur : le jeu continue, indication explicite ; ouvrir les menus ne protège pas des dégâts et n’interrompt pas l’horloge serveur. Retour au menu/fermeture volontaire en expédition demande confirmation de l’abandon. Client quittant = règles de déconnexion ; hôte quittant = fin de session pour tous.

Écran de chargement : destination, conseils pertinents et indicateur d’activité ; pourcentage seulement s’il est mesuré. Transition en échec : retour stable au menu avec bouton Réessayer. Aucun bouton ne déclenche deux chargements sur double clic.

## 17. Sauvegardes et intégrité de progression

Trois domaines indépendants : paramètres locaux du joueur, campagnes de l’hôte, projets de l’éditeur. Un projet d’île n’est jamais une sauvegarde de campagne. Identifiants internes stables, noms affichés modifiables ; ne pas utiliser les noms saisis comme chemins de fichiers.

Autosauvegarde au hub : création de campagne, achat confirmé, retour d’expédition et changement de préparation pertinent. Sauvegarde manuelle disponible au hub uniquement. Indicateur discret « Sauvegarde… / Sauvegardé / Échec ». Écriture temporaire puis remplacement atomique si supporté, conservation de la dernière copie valide et validation avant chargement. Sérialiser les écritures pour éviter les courses.

Avant départ, écrire un marqueur `ExpeditionInProgress` avec ID et équipement engagé. À la résolution, appliquer bilan et vider le marqueur en une transaction logique. Après crash/quitter en pleine expédition, reprendre au hub : expédition abandonnée, équipement engagé perdu, aucun loot crédité ; afficher un résumé. Les récompenses déjà appliquées ne sont jamais appliquées une deuxième fois. Ce choix évite de transformer Continuer en récupération gratuite des objets perdus. Crash avant engagement effectif du départ = conservation du dernier hub valide.

Paramètres graphiques/audio/commandes sont locaux et ne sont pas écrasés lors d’une connexion à l’hôte. Les changements de règles de campagne s’effectuent au hub par l’hôte et invalident les états « prêt » ; jamais en plein rituel ou expédition.

## 18. Catalogue des paramètres

Chaque réglage possède ID, type, valeur par défaut, bornes, aide, disponibilité, mode d’application et persistance. Les valeurs ci-dessous sont des propositions initiales. Les options dépendantes du matériel/plugins ne sont montrées comme activables que si réellement prises en charge. Pas de faux HDR, faux upscaler ou réglage sans effet.

| Onglet | Réglages à exposer | Défaut / comportement |
|---|---|---|
| Affichage | Écran, mode fenêtré/sans bordure/plein écran, résolution, fréquence si applicable | Écran courant, sans bordure, résolution bureau |
| Fluidité | VSync, limite FPS jeu (30/60/90/120/144/165/240/illimité), limite menu (30/60) | 60 jeu et menu ; options effectives selon mode |
| Image | Luminosité avec mire, gamma si compatible, HDR et calibration si supportés | Valeurs neutres ; ne pas modifier les matériaux du monde |
| Qualité | Préréglages Faible/Moyen/Élevé/Ultra/Personnalisé, distance, textures, ombres, effets, végétation, post-traitement | Moyen initial ; détecter/profiler avant recommandation |
| Rendu | Anti-aliasing, échelle de rendu 50–100 %, upscaling disponible et qualité, illumination/reflets supportés | Repli compatible ; combinaisons invalides désactivées |
| Effets caméra | Flou de mouvement, grain, aberration chromatique, profondeur de champ en gameplay | Désactivés par défaut pour lisibilité |
| Audio | Master, musique, effets, ambiance, dialogues, UI (0–100) | Master 80, musique 50, autres 80 |
| Sortie audio | Périphérique si API disponible, stéréo/mono, plage dynamique Nuit/Standard/Large | Périphérique système, stéréo, Standard |
| Voix L7 | Entrée/sortie, test micro local, push-to-talk/activation, touche, seuil, volume, mute personnel | Push-to-talk ; test sans diffusion aux autres |
| Souris | Sensibilité X/Y, inversion verticale/horizontale, sensibilité visée si arme disponible | X/Y identiques, aucune inversion |
| Clavier | AZERTY/QWERTY/personnalisé, remappage, conflits, restaurer touches | Détection ou choix initial ; touches affichées dynamiquement |
| Manette | Sensibilité, inversion, zones mortes, vibrations, glyphes | Afficher si support réellement intégré |
| Confort | FOV horizontal 80–110, balancement tête, secousses caméra, sprint maintenir/basculer | FOV 90, balancement faible, sprint maintenir |
| Interaction | Maintenir/basculer interaction prolongée, info portée, taille réticule | Maintenir par défaut ; alternative accessible |
| Interface | Échelle 80–150 %, taille texte, zone sûre, opacité HUD, réticule, indications d’interaction | Lisibilité en 1080p et 720p testée |
| Accessibilité | Sous-titres, taille/fond/nom locuteur, contraste, palettes daltonisme, réduction flashs/mouvement | Sous-titres activés ; codes couleur doublés par icônes/texte |
| Langue | Français ; anglais lorsque traduction complète disponible | Français ; appliquer à tous les widgets et sous-titres |
| Aide | Tutoriels contextuels, réafficher tutoriel, confirmation abandon | Tutoriels activés ; confirmations destructives conservées |
| Diagnostic | Afficher FPS, ping/perte paquets en multi, copier résumé technique | Désactivé, aucune donnée sensible copiée |

L’alternative « basculer interaction prolongée » lance le sermon avec un appui puis l’annule avec un second ; elle impose exactement les mêmes 20 s immobiles, vulnérabilité et interruptions. La règle de relâchement de section 6 s’applique seulement au mode Maintenir. Les options de confort ne donnent pas de temps supplémentaire ni de protection réseau.

Bas de page : Appliquer, Annuler, Restaurer cet onglet. Les réglages audio peuvent être préécoutés, puis restaurés à l’annulation. Une modification vidéo risquée demande confirmation pendant 15 s ; sans confirmation, retour à la configuration précédente. Les réglages exigeant redémarrage sont identifiés. Échap avec modifications non appliquées demande Appliquer/Abandonner/Rester. Sauvegarder seulement après succès ; un redémarrage du jeu restitue les valeurs.

## 19. HUD, feedback et écrans de gameplay

HUD discret : santé/endurance, objet porté et charge, action disponible, horloge « avant submersion » ou « évacuation », quotas arrimés, direction/distance de barque. Ne pas afficher la valeur globale d’objets non encore rapportés comme acquise.

Panneau de zone : état, menaces restantes, drapeau planté, officiant, progression 0–20 s et cause de dernière interruption. Bonus +2 min accompagné d’un son bref, visible par toute l’équipe. Alerte unique à 60/30/10 s avant montée ; nouveau seuil après extension ne doit pas spammer le même son en boucle.

Carte tactique : zones découvertes, routes, mouillages accessibles/verrouillés, position/destination barque, pings d’équipe avec expiration. Les zones inconnues ne révèlent pas tous leurs trésors. Pings limités en fréquence. La carte n’arrête pas le multijoueur.

Cargaison : objets, masse, capacité, attribution au quota et surplus. Boutique : prix, solde, effet avant/après, raison d’achat impossible ; attente de confirmation serveur avant décrément local.

Bilan : survivants/perdus, zones pacifiées, temps gagné, ressources extraites, quotas, taxe, bonus, pertes et argent net. Continuer ramène au hub après application unique. Défaite et abandon ont le même format explicable. Mort : cause courte, liste de coéquipiers observables, retour au hub après résolution ; aucun contrôle de monde par le spectateur.

## 20. Éditeur d’îles intégré — lot E1

Interprétation retenue : « éditeur » désigne un mode de création de niveaux dans le jeu distribué. Il est distinct de l’éditeur Unreal utilisé par les développeurs. Si le propriétaire entendait seulement ouvrir Unreal, remplacer ce lot par un outil de lancement pour développeur, sans le proposer au joueur final.

E1 est solo, local et basé sur des éléments préfabriqués. Nouvelle partie → Éditeur : nom du projet, template d’île, seed, puis Créer. Charger → Projets : Ouvrir, Dupliquer, Renommer, Supprimer avec confirmation. Aucun or ni progression de campagne n’est modifié dans ce mode.

Outils : caméra libre, sélectionner, déplacer, tourner, mise à l’échelle bornée, grille/accrochage, dupliquer, supprimer, annuler/rétablir (50 opérations), palette par catégorie et inspecteur de propriétés autorisées. Palette : morceaux de relief prédéfinis, rochers, végétation, ressources, points d’ennemis, zones de pacification/drapeaux, départs joueurs, barque/mouillages et trajectoires. Pas de sculpture terrain, import de code, plugins, assets externes ou scripts arbitraires en E1.

Le projet enregistre des données d’instances (AssetId autorisé, transform, propriétés validées, GUID) et les paramètres de mission. Ne pas enregistrer des références brutes non stables vers des acteurs temporaires ni tenter de fabriquer des maps Unreal arbitraires au runtime. Version de schéma, limites proposées 500 éléments / 30 ennemis / 5 zones, valeurs bornées. Prévisualisation de la marée et des zones inondées à différents instants.

Bouton Valider : vérifier spawn, barque, au moins une zone pacifiable, références uniques, socles accessibles, stock théorique suffisant aux quotas, au moins un mouillage final et trajectoires valides. Navigation/temps d’accès nécessitent aussi un test humain ; un contrôle automatique ne prouve pas le plaisir ni la faisabilité chronométrée.

Bouton Tester : snapshot temporaire → lancer une expédition solo avec les règles actuelles → quitter test → revenir exactement au projet non modifié. Échec validation bloquant explique l’objet en faute et le sélectionne. Sauvegarder reste possible pour un brouillon invalide, Tester exige les contraintes structurelles minimales. Sortie avec modifications : Enregistrer/Abandonner/Annuler. Autosauvegarde brouillon toutes les 60 s hors test et possibilité de restauration après crash.

E1 ne comprend ni collaboration en ligne, ni Workshop, ni partage public automatique. Ces fonctions demandent un lot distinct. L’éditeur est livré après stabilisation du noyau ; l’entrée correspondante doit indiquer clairement son indisponibilité jusque-là.

## 21. Catalogue des Blueprints, services et contrats

Les noms suivants sont un plan de création, pas la preuve que des assets existent. Chaque Blueprint assemble une base C++ lorsque la règle nécessite autorité, validation ou tests. Éviter un Level Blueprint central contenant toute la logique. Les widgets demandent des actions et observent des états ; ils ne calculent pas les récompenses.

| Asset/service prévu | Responsabilité / contrat |
|---|---|
| BP_CrownfallGameInstance | Initialisation application et accès aux services locaux ; ne pas utiliser comme état réseau partagé |
| SaveSubsystem | Lister, créer, charger, migrer, sauvegarder campagne/projet ; résultats asynchrones et erreurs |
| SettingsSubsystem | Charger préférences, prévisualiser, appliquer, annuler, restaurer |
| SessionSubsystem | Créer/rejoindre/quitter, annulation, déconnexion ; adaptateur fournisseur L7 |
| UIFlowSubsystem | Pile d’écrans, focus, curseur, mode d’entrée, modales et nettoyage au changement de monde |
| BP_MenuGameMode / BP_MenuController | Monde de menu sans personnage actif, navigation UI |
| BP_HubGameMode / BP_HubGameState | Campagne, lobby, préparation, état prêt et départ serveur |
| BP_ExpeditionGameMode / BP_ExpeditionGameState | Machine d’états, échéances, résolution, état partagé |
| BP_CrownfallPlayerController / BP_CrownfallPlayerState | Requêtes possédées, identité, statut, UI locale ; validation serveur |
| BP_ExpeditionCharacter | Déplacement et assemblage Health/Stamina/Interaction/Carry/Combat |
| ABP_ExpeditionCharacter | Repos/marche/course/nage/portage ; montages planter/sermon/attaque/mort |
| BP_CarryableResource + variantes | Identité, physique libre, portage, arrimage ; bois/nourriture/coffre |
| BP_ResourceNode | Récolte, réserve finie, outil requis, spawn unique de ressource |
| BP_PacificationZone / BP_RoyalFlag | Périmètre, ennemis, drapeau, officiant, durée, interruptions, succès unique |
| BP_Rowboat / BP_MooringPoint | Sièges, inventaire cargo, mouillages et état en déplacement |
| BP_BoatPlacementManager | Votes, trajet, passagers, heure de début/fin et destination |
| BP_TideManager / BP_DayNightManager | Niveau d’eau dérivé du temps serveur ; rendu local du ciel/eau |
| BP_EncounterDirector | Budget d’ennemis/événements, seed et activation ; pas de récompense arbitraire |
| BP_EnemyMelee / AI_EnemyController | Perception, déplacement, attaque, mort et drop unique |
| BP_Shark / AI_SharkController | Patrouille aquatique, poursuite, attaque, sortie de cible |
| BP_GalleonHub / BP_ShopTerminal / BP_ContractBoard | Interaction hub vers boutique, contrats et préparation |
| BP_EditorGameMode / BP_EditorPawn | Caméra/outils éditeur local et orchestration du test |
| BP_EditorPlacementPreview | Fantôme, validité, grille, collision et propriétés bornées |
| SG_Campaign / SG_LocalProfile / SG_IslandProject | Schémas séparés et versionnés de données sauvegardées |
| DA_ExpeditionRules / DA_Item / DA_Enemy / DA_IslandTemplate | Paramètres de règles, assets autorisés et tables de contenu |

Interfaces : `Interactable` (texte, disponibilité, requête), `Damageable`, `Carryable`. Événements : `OnPhaseChanged`, `OnFloodDeadlineChanged`, `OnZoneProgressChanged`, `OnCargoChanged`, `OnQuotaChanged`, `OnPlayerStatusChanged`, `OnSaveCompleted`, `OnSettingsApplied`. Interfaces de présentation ne contournent jamais les validations serveur. Les RPC client passent par un acteur possédé, typiquement le PlayerController, et visent des références d’acteurs que le serveur revérifie.

Widgets : WBP_MainMenu, WBP_NewGame, WBP_LoadGame, WBP_SaveSlot, WBP_JoinPrivate, WBP_Lobby, WBP_SettingsRoot et pages, WBP_Credits, WBP_ConfirmDialog, WBP_Loading, WBP_ErrorDialog, WBP_HUD, WBP_ZoneStatus, WBP_TacticalMap, WBP_Cargo, WBP_Shop, WBP_ContractBoard, WBP_Pause, WBP_Spectator, WBP_ExpeditionResult, WBP_EditorToolbar, WBP_AssetPalette, WBP_Inspector, WBP_ValidationReport.

Cartes : L_MainMenu, L_GalleonHub, L_Island_Prototype, L_EditorSandbox. Créer des références explicites pour le packaging ; une map chargée dynamiquement doit être présente dans le build. Séparer les contextes d’entrée Menu, Gameplay et Editor ; les désactiver correctement aux transitions.

## 22. Flux d’exécution de référence pour les agents

**Nouvelle campagne :** clic → validation formulaire → verrou du bouton → création transactionnelle → chargement hub → état prêt. Échec à n’importe quelle étape = erreur exploitable et déverrouillage ; ne pas laisser un faux slot jouable.

**Sermon :** interaction locale → requête serveur via contrôleur → validation joueur/zone/phase → attribution officiant et heure de début → réplication état → animation/audio/UI → contrôle interruptions → après 20 s, nouvelle validation échéance → état pacifié + bonus atomique → notification. La fin d’un son ou d’une animation côté client ne valide jamais la zone. Effacer les références si joueur/acteur détruit.

**Extraction :** demande → validation/vote → préavis → verrou de résolution → snapshot survivants et cargo → calcul bilan serveur → sauvegarde avec ExpeditionId → réplication bilan → retour hub. Nouvelle requête sur un ID déjà résolu renvoie le résultat existant.

**Réglage vidéo :** copie valeurs courantes → application provisoire → compte à rebours 15 s → confirmer et persister, ou restaurer. Une perte de focus ne doit pas bloquer le retour automatique.

**Test éditeur :** valider projet → snapshot → instancier éléments autorisés dans monde de test → lancer partie isolée → résultat sans économie persistante → restaurer contexte éditeur. Aucun objet du test ne fuit dans le projet.

Pour chaque asset, les agents tiennent un registre : parent, dépendances, variables exposées, fonctions/événements, réplication, propriétaire d’édition, création manuelle ou automatisée, compilation et test. Fournir les graphes clés ou instructions de reconstruction si l’éditeur n’est pas accessible ; ne pas livrer de simples noms de Blueprints comme fonctionnalité achevée.

## 23. Roadmap et recette supplémentaires

La répartition L0–L8 demeure, avec ces ajouts obligatoires : L0 crée le menu minimal et les cartes de démarrage ; L4 comprend Nouvelle partie/Charger/Continuer et intégrité des sauvegardes ; L5 intègre HUD, paramètres et pause ; L6 vérifie navigation et packaging. L7 couvre Rejoindre/Héberger privé et VoIP, secondaires comme demandé. E1 est l’éditeur local après L6, indépendant de L7. Crédits complets exigés avant distribution publique.

| ID | Vérification | Résultat attendu |
|---|---|---|
| UI01 | Premier lancement sans sauvegarde | Nouvelle partie possible, Continuer absent |
| UI02 | Créer → jouer → quitter hub → Continuer | Même campagne, réglages locaux conservés |
| UI03 | Double clic Créer/Charger | Une seule action et transition |
| UI04 | Chargement ou connexion échoue | Menu utilisable, erreur claire, aucune session fantôme |
| UI05 | Sauvegarde corrompue/future | Refus sûr, secours proposé si valide, aucun écrasement |
| UI06 | Changer résolution sans confirmer | Retour après 15 s, même après perte de focus |
| UI07 | Audio préécouté puis Annuler | Anciennes valeurs restaurées |
| UI08 | Remappage et navigation clavier | Conflits signalés, textes de touches corrects, aucun piège de focus |
| UI09 | Pause solo / menu multi | Horloge arrêtée seulement en solo |
| UI10 | Quitter/crash en expédition puis charger | Retour hub, abandon unique, pas de duplication d’équipement/loot |
| UI11 | Sermon mode Maintenir et Basculer | Même durée/risque ; seules commandes de maintien diffèrent |
| UI12 | 720p, 1080p, 1440p, ultralarge et texte maximal | Aucun texte tronqué ni bouton inaccessible |
| ED01 | Créer/placer/annuler/rétablir/sauver/recharger | Objets, propriétés, GUID et paramètres identiques |
| ED02 | Projet sans spawn/barque/ressources | Erreurs nommées ; test bloqué selon gravité |
| ED03 | Tester puis revenir | Projet intact, aucun gain campagne |
| ED04 | Crash éditeur | Brouillon récupérable, original conservé |
| NET01 | Privé complet, mauvaise version, hôte parti | Motif exact, retour propre, pas de migration implicite |

Definition of Done UI : chaque bouton a un effet ou une raison d’indisponibilité ; navigation aller/retour vérifiée en build ; toutes les opérations asynchrones ont succès, échec et annulation lorsque possible. Les crédits listent uniquement des contributions réelles et les licences vérifiées.
