# Les 100 angles de l'audit commercial (2026-10-04)

Chaque ligne donne un auditeur et son rapport `NN-slug.md`. Les domaines vont de A à J, avec 10 angles chacun.

## A. Boucle de jeu et gameplay
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 01 | core-loop | game designer senior (extraction coop) | Boucle complète menu → galion → île → zone sûre : rythme minute par minute, durée d'une manche, temps morts, pics d'émotion. Proposer une boucle optimisée chronométrée |
| 02 | risk-reward | designer systèmes | Tension risque/récompense (quota, submersion, abandon, cale, valeur abîmée) : décisions intéressantes, stratégies dominantes, exploits |
| 03 | pacification-sermon | designer de mécaniques | Drapeau + sermon immobile de 20 s : fun, lisibilité, coop/solo, interruptions ; alternatives meilleures |
| 04 | combat-tiki | designer combat | Combat contre les tiki (sabre, mousquet, découpe, armes lâchées, embuscades) : profondeur, ressenti, variété, lisibilité |
| 05 | loot-economy | économiste de jeu | Valeur des objets et sources de butin (camps, marae, villages, mobs, perle noire, butin commun) face à la courbe de quota ; inflation, équilibrage |
| 06 | boat-galleon-flow | designer navigation | Barque physique + galion + palan : friction, plaisir, temps de trajet, risques (requin, vagues), navette |
| 07 | difficulty-curve | designer progression | Courbe de difficulté sur les manches, ajustement solo / 4 joueurs, équité |
| 08 | coop-roles | designer coop | Interdépendance et rôles émergents à 2–4 joueurs, communication, moments sociaux |
| 09 | death-failure | designer de l'échec | Mort, abandon, défaite, spectateur, frais de recrutement : punition juste, frustration, retours possibles |
| 10 | replayability | designer roguelite | Rejouabilité (20 fragments, villages générés, contrats) : ce qui manque pour 50 h et plus |

## B. Contenu et fonctionnalités manquantes
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 11 | missing-vs-cahier | producteur exhaustif | Écart entre le cahier v1.3 (L0–L8, §16–24) et ce qui existe vraiment (code, board) : tout ce qui manque pour une partie jouable de bout en bout |
| 12 | missing-vs-genre | analyste du genre | Fonctionnalités standard du genre (Lethal Company, R.E.P.O., Content Warning, Deep Rock Galactic, Sea of Thieves) absentes ici |
| 13 | onboarding-tutorial | designer d'apprentissage | Tutoriel et apprentissage (barque, sermon, palan, vente, quota) : les 10 premières minutes |
| 14 | progression-meta | designer rétention | Méta-progression entre parties (déblocages, cosmétiques, galion amélioré, titres) |
| 15 | events-dangers | designer d'événements | Événements et dangers (tempête, requin, nuit, cyclone, tsunami, tiki nocturnes) : variété, signalement, équité |
| 16 | enemies-bosses | designer bestiaire et IA | Bestiaire (5 tiki, requin), besoin d'élites, mini-boss, factions ; qualité de l'IA |
| 17 | items-tools | designer objets | Outils et objets (hache, lanterne, torche, sabre, mousquet, trompette) : utilité, manques (grappin, carte, longue-vue, filet…) |
| 18 | social-features | designer social | Emotes, pings, voix de proximité, cosmétiques, mode photo, replays : le facteur « clip » |
| 19 | endgame | designer endgame | Après 10 manches : scores, classements, défis hebdo, seeds partagés, modes |
| 20 | hub-galleon | designer de hub | Le galion comme lieu de vie : personnalisation, Percepteur, boutique, contrats, ambiance |

## C. Flux, UX et UI
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 21 | flow-timing | designer UX de flux | Durée de chaque transition (menu, chargements, traversée, voyages) : budget idéal, suppression des temps morts |
| 22 | hud-readability | designer HUD | HUD TAPAO, compteur, barres de vie masquées : information utile ou bruit |
| 23 | menus-navigation | designer UI | Menus TAPAO, lobby à bord, paramètres, pause : friction, conventions PC |
| 24 | feedback-juice | spécialiste du game feel | Retours (sons, VFX, caméra) sur les actions clés : ramasser, vendre, couper, sermon, départ |
| 25 | accessibility | experte accessibilité | Daltonisme, sous-titres, remappage, mal des transports (barque, caméra), motion, difficulté |
| 26 | controller-deck | spécialiste manette et Steam Deck | Compatibilité manette et Steam Deck : interface, contrôles, performances |
| 27 | multiplayer-ux | designer UX multijoueur | Inviter, rejoindre, lobby, prêt, votes, déconnexions, hôte qui part |
| 28 | spectator-death-ux | designer UX | Expérience du mort et du spectateur, retour en zone sûre |
| 29 | economy-ui | designer UI économique | Vente, registre, boutique, bilan : clarté des calculs (taxe, frais, quota, commandes) |
| 30 | first-session | chercheur UX (playtests) | Parcours d'un novice de l'installation à la fin de la première manche : points d'abandon probables |

## D. Architecture et code
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 31 | architecture | architecte C++ UE | Structure du module (dossiers, dépendances, couplage, subsystems/acteurs), dette technique |
| 32 | run-architecture | architecte gameplay | ADR-005 (URunSubsystem, ARunGameMode, voyages, manifeste de cale) : risques et améliorations avant implémentation |
| 33 | data-driven | ingénieur data | Valeurs en DataAsset ou en dur ; cohérence et duplications entre DA_* |
| 34 | replication-authority | ingénieur réseau UE | Autorité serveur et réplication (tiki, objets physiques, barque, galion, villages) : erreurs probables |
| 35 | save-system | ingénieur sauvegarde | Sauvegardes (TAPAO, campagne, SG_Run prévu) : robustesse, versionnage, migration |
| 36 | tests-quality | ingénieur QA | Couverture des tests, ce qui n'est pas testé (multijoueur), stratégie |
| 37 | error-handling | ingénieur fiabilité | Erreurs, journaux, crashs (crash éditeur par Live Coding), robustesse |
| 38 | tooling-pipelines | ingénieur outils | Pipelines Python (carto, villages, végétation, import MCP) : reproductibilité, maintenance |
| 39 | team-workflow | lead technique | Workflow multi-sessions (éditeur partagé, fenêtres de build, worktrees, commits) : goulots et risques |
| 40 | security-cheat | ingénieur sécurité | Triche et sécurité (listen server, RPC, options d'URL, sauvegardes) |

## E. Performance et place (disque, RAM, chargement)
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 41 | disk-footprint | ingénieur build | Taille sur disque du projet et d'un build (Content 1,2 Go, map 110 Mo, textures, audio) : plan de réduction chiffré |
| 42 | ram-vram | ingénieur mémoire | RAM et VRAM (816 000 instances, Nanite, Lumen, villages, ragdolls, cadavres) : budget et plan |
| 43 | map-structure | level tech | Lvl_Raraka monolithique ou L_Frag_* (ADR-004) ; World Partition, streaming, HLOD |
| 44 | cpu-ai-ticks | ingénieur perf CPU | IA tiki (36–54 villageois, gardiens, embuscades), ticks, physique (ragdolls, cadavres permanents), vent |
| 45 | rendering | ingénieur rendu | Lumen, ombres, eau, 128–166 lumières par fragment, Nanite végétation : 60 FPS en 1080p ? |
| 46 | network-bandwidth | ingénieur réseau | Bande passante (objets physiques, cadavres, 5 000 butins communs, tiki), relevancy, dormance |
| 47 | loading-times | ingénieur chargement | Temps de chargement (map de 110 Mo, construction au lancement) : objectif < 15 s |
| 48 | build-packaging | ingénieur release | Cook et packaging (maps dynamiques, références souples, plugins, Shipping), taille du build |
| 49 | content-budgets | tech artist | Budgets par catégorie (triangles, textures, sons, LOD) et recommandations |
| 50 | profiling-plan | ingénieur perf | Plan de profilage et métriques (CSV, Insights), benchmarks automatiques par fragment |

## F. Réseau et multijoueur
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 51 | listen-server-limits | ingénieur netcode | Listen server à 4 : charge de l'hôte (IA, physique, rendu), risques |
| 52 | physics-replication | ingénieur physique réseau | Objets portés, barque, cadavres, armes lâchées : risques de désynchronisation |
| 53 | latency | ingénieur netcode | Jeu à 100–200 ms (sabre, sermon, portage, votes) : compensation |
| 54 | travel-sessions | ingénieur sessions | Voyages galion ↔ île, arrivée tardive, reconnexion, perte de l'hôte |
| 55 | late-join-sync | ingénieur netcode | Synchronisation des arrivants (villages, végétation déplacée, cale, cadavres) |
| 56 | online-services | ingénieur services en ligne | Sessions Steam, invitations, NAT, VoIP (L7) : plan et coûts |
| 57 | anti-griefing | designer coop | Griefing (vol de butin, barque coulée, vote de départ, kill d'équipe) |
| 58 | netcode-tests | QA réseau | Tests réseau (PIE à 2, -game, profils dégradés) : stratégie concrète |
| 59 | scalability-8p | producteur technique | 4 ou 8 joueurs : impacts design et technique |
| 60 | dedicated-servers | ingénieur infra | Serveurs dédiés : utiles ou non, coûts |

## G. Art, audio et direction
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 61 | art-direction-coherence | directeur artistique | Atoll réaliste 1:1 + beans stylisés + villages japonais + tiki + galion anglais : une identité visuelle ? |
| 62 | readability-silhouettes | DA gameplay | Lisibilité des ennemis, objets de valeur et interactifs dans une végétation dense |
| 63 | environment-variety | environment artist | Variété visuelle des 20 fragments du même atoll : risque de monotonie |
| 64 | lighting-mood | lighting artist | Ambiance lumineuse, nuit, tempête, peur |
| 65 | audio-design | sound designer | Sons (voix tiki CC0, pas, ambiance), musique adaptative absente |
| 66 | vfx | artiste VFX | Effets (eau, coupes, sermon, palan, frégate) : manques |
| 67 | animation-feel | animateur | Beans en ragdoll actif, crossfades des tiki, rames : ressenti |
| 68 | ui-art | DA UI | Direction de l'UI TAPAO : cohérence avec le thème |
| 69 | capsule-key-art | DA marketing | Capsule Steam, key art, logo : quoi produire, avec quels moments du jeu |
| 70 | asset-licensing | juriste assets | Licences (CC0, Poly Haven, rubberduck, Te Fenua Etalab, Maxar) : risques pour une vente commerciale |

## H. Narratif, fiction et sensibilités
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 71 | premise-hook | narrative designer | Pitch et accroche (Couronne fictive, satire bureaucratique) : est-ce mémorable ? |
| 72 | cultural-sensitivity | consultante culturelle (Pacifique) | Atoll réel, village réel de Motutapu, marae sacrés, tiki en monstres, colons britanniques, villages japonais : risques de controverse et recommandations |
| 73 | real-place-legal | juriste | Usage d'un lieu réel (Raraka), données Te Fenua, image, communauté locale |
| 74 | tone-humor | auteur comique | Ton (satire, peur nocturne, chaos coop) : cohérence |
| 75 | world-lore | worldbuilder | Pourquoi des villages japonais aux Tuamotu, pourquoi des tiki : justification du monde |
| 76 | characters-identity | character designer | Beans soldats britanniques : identification, cosmétiques, diversité |
| 77 | quests-contracts | quest designer | Contrats et missions : variété, narration émergente |
| 78 | naming-ip | conseil en marque | « Crownfall Expeditions » : marque existante, risques, alternatives |
| 79 | localization | chef de projet localisation | Langues (FR/EN et plus), String Tables, coûts, marchés |
| 80 | rating-content | conseil classification | PEGI/ESRB : découpe de corps, sang, violence sur des « villageois » |

## I. Marketing et business
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 81 | positioning-usp | stratège produit | Positionnement et USP face à Lethal Company, R.E.P.O., Content Warning, Sea of Thieves, Raft, Dredge |
| 82 | target-audience | analyste marché | Audience, segments, plateformes, personas joueurs |
| 83 | streamer-appeal | expert influence | Moments viraux, clips TikTok, chaos, proximité vocale : potentiel streamers |
| 84 | steam-strategy | spécialiste Steam | Page, tags, wishlists, Next Fest, démo, playtest |
| 85 | pricing-business-model | business developer | Prix, premium ou cosmétiques, DLC, fenêtre de prix |
| 86 | trailer-hooks | monteur de trailers | Les 5 premières secondes, séquences fortes disponibles ou à créer |
| 87 | community | community manager | Discord, roadmap publique, retours, early access |
| 88 | competitor-features | analyste concurrence | Comparatif fonctionnalité par fonctionnalité avec 6 concurrents |
| 89 | release-plan | producteur release | Plan de sortie (EA, 1.0, mises à jour), MVP vendable |
| 90 | market-risk | analyste risques marché | Saturation du genre, différenciation, découvrabilité |

## J. Production, qualité et risques
| NN | Slug | Persona | Mission |
|---|---|---|---|
| 91 | scope-mvp | producteur exécutif | Scope minimal vendable face au backlog : quoi couper |
| 92 | roadmap | producteur | Roadmap réaliste vers l'early access (jalons, dépendances) |
| 93 | risk-register | gestionnaire de risques | Registre des risques (technique, juridique, marché, équipe) |
| 94 | qa-playtests | lead QA | Playtests internes et externes, protocole, métriques |
| 95 | telemetry-analytics | data analyst | Télémétrie (funnels, morts, durées), RGPD |
| 96 | crash-support | ingénieur live ops | Crash reporting, support, patchs, symboles |
| 97 | legal-compliance | juriste | RGPD, EULA, mentions, licences, données géographiques |
| 98 | platform-certification | ingénieur plateformes | Steam, Steam Deck, consoles futures : exigences |
| 99 | team-cost | directeur de studio | Compétences manquantes, budget, sous-traitance |
| 100 | devil-advocate | avocat du diable | Pourquoi ce jeu échouerait commercialement, et comment l'éviter |
