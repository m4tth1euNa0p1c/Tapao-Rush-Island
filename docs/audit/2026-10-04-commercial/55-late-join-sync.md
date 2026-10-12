# 55 — Synchronisation des arrivants (late join)
Domaine : F · Auditeur : ingénieur netcode

## Verdict
Horloge, vent, jour/nuit, minuteur, galion, camps, marae, butin ramassé et villages sont écrits pour l'arrivant tardif : état répliqué, fonctions pures, double chemin `BeginPlay` / `OnRep`. Le reste ne l'est pas : effets rejoués, sol de crue relu par chaque machine, cargaison recalculée par chaque client, retardataire sans rattachement. Rien n'a tourné à deux machines (villages-japonais.md:179), or le chargement de 110 Mo fait de chaque ami un arrivant tardif.

## Constats

- **[Majeur] Compteurs « événement » : l'historique rejoué à chaque création d'acteur**
  - Constat : `FireCount`, `PunchCount`, `PlayCount` rejouent tir, coup, fanfare dans leur OnRep. La valeur initiale ≠ 0 déclenche l'effet (son compris) chez un client, à l'arrivée et à chaque rentrée à moins de 150 m (pertinence par défaut). Même motif : `OnRep_Sunk` rejoue le naufrage.
  - Preuve : MusketWeapon.cpp:426 ; IslanderCombatComponent.cpp:138 ; TrumpetEmoteComponent.cpp:166 ; BoatActor.cpp:939 ; moteur DataChannel.cpp:3518→3537, Actor.cpp:312. Effet supposé.
  - Recommandation : `if (!HasActorBegunPlay()) return;` en tête de chaque OnRep de compteur ; test « arrivant ».
  - Effort : S

- **[Majeur] La cargaison affichée est recalculée par chaque client, sur des données partielles**
  - Constat : le HUD appelle `GetCargoValue()` : recouvrement physique local, objets répliqués seulement (150 m), valeur remise à `BaseValue` par `BeginPlay`. Le galion est à 324–413 m de la plage : un client à terre voit 0 $, l'hôte la cale pleine ; un arrivant, tout au prix neuf.
  - Preuve : STapaoHUD.cpp:260 ; GalleonActor.cpp:178-206 ; ValuableComponent.cpp:27 ; README raraka:131.
  - Recommandation : le serveur calcule et réplique `CargoValue` ; `Value` initialisée sous autorité ; `FHoldEntry` (ADR-005:54) porte la valeur courante.
  - Effort : S

- **[Majeur] Sol de la crue relu par chaque machine, à son heure, galion et camps compris**
  - Constat : `ReadGroundBatch` trace en WorldStatic complexe et ne saute que les ISM et le plan d'eau : coque du galion, décors de camp et pierres de marae (BlockAll = WorldStatic) comptent comme sol. L'hôte lit ses 102 400 cases environ 7 images après l'arrivée, avant camps (1 s) et galion (2 s, défauts du code) ; un client tardif lit tout (l'hôte même dépend de la cadence au démarrage). Chez lui, le galion n'est plus sur « mer ouverte » et ne suit pas la crue. Rattrapage : 4 pas par image, 17 s à 60 FPS pour 210 s de crue.
  - Preuve : FloodSimulationActor.cpp:366-399, 439 ; FloodRules.h:60,102,118,122 ; GalleonActor.cpp:41,141-155 ; CampSite.cpp:96 ; MaraeSite.cpp:82 ; BaseEngine.ini:3104. Effet supposé.
  - Recommandation : tag `NoFloodGround` ignoré par les traces ; hash du sol répliqué, écart signalé ; pas adaptatif si le retard dépasse 1 s.
  - Effort : M

- **[Majeur] Le retardataire n'est rattaché à rien**
  - Constat : ni `PreLogin` ni `PlayerState` propre : pion neuf (santé pleine, inventaire de départ, mort effacée : `bPermadeath` contournable, supposé). `OnPostLogin` l'assoit dans la barque d'arrivée où qu'elle soit ; barque coulée, il apparaît au PlayerStart de Motutapu, à plus de 10 km. Le pont ne reçoit que les joueurs présents à +2 s.
  - Preuve : BoatStartSubsystem.cpp:294-307, 356-360 ; BoatStartRules.h:71 ; GalleonSpawner.cpp:136-147 ; README raraka:139 ; storm-flood.md:65 ; grep `PreLogin` : 0.
  - Recommandation : refus hors zone sûre (RUN03) ; en attendant, `ChoosePlayerStart` sur `bean_spawn` du galion.
  - Effort : S

- **[Mineur] Cadavres : formes rigides saines, ragdoll rejoué depuis la pose de mort**
  - Constat : `ATikiCorpse` et ses moitiés se reconstruisent des données initiales : bon patron. `ATikiRagdollCorpse` replace les os à la pose de mort à chaque création ; ses membres, simulés par chaque machine, ne sont jamais répliqués : les poses divergent.
  - Preuve : TikiRagdollCorpse.cpp:158-175 ; TikiCorpse.cpp:189-193.
  - Recommandation : figer le ragdoll au repos en `ATikiCorpse`.
  - Effort : M

- **[Mineur] Villages et végétation : cohérents par construction, jamais contrôlés**
  - Constat : opérations monotones (cacher, jamais retirer) : l'ordre importe peu. Mais aucun digest : un `DA_Villages` ou un build différent donne des maisons solides chez l'un, absentes chez l'autre. `FSiteClearing::Clear` ne filtre pas (contrairement à `IsClearable`) : un camp à moins de 9 m enterrerait aussi des maisons. Le cache de buissons du client (bâti au premier mob, jamais refait) peut précéder le dégagement.
  - Preuve : VillageSpawner.cpp:262 ; SiteClearing.cpp:23-53 ; MobHealthBarComponent.cpp:78-85 ; TikiDirector.cpp:104-106, 295-299. Effets supposés.
  - Recommandation : répliquer un digest (instances, plantes cachées), logger l'écart ; filtrer `Clear` ; reconstruire le cache à `OnVillagesBuilt`.
  - Effort : S

## Manques
1. Test d'arrivée : PIE à 2, client entrant à T+30 s, reconnexion, `Net PktLag=120`.
2. `PlayerState` avec reprise ; refus « partie en expédition » (voir 27).
3. Mort et ragdoll répliqués pour les arrivants (voir 34).

## Quick wins
1. Garde `HasActorBegunPlay()` dans les 4 OnRep de compteur (1 h).
2. `CargoValue` répliqué ; `Value` et `Health` sous autorité (3 h).
3. Galion, camps et marae hors du tracé du sol de la crue (2 h).
4. `ChoosePlayerStart` sur le pont du galion pour tout pion tardif (2 h).

## Questions qui challengent l'équipe
1. Quand deux machines ont-elles joué ensemble ? 296 tests verts, aucun ne simule un arrivant.
2. Un ami dont le disque charge 110 Mo en 40 s perd-il 40 s des 120 s ?
3. En permadeath, un joueur qui plante revient-il vivant, refusé ou mort ? Qui décide ?
