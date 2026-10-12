# 100 — Avocat du diable : pourquoi ce jeu échouerait
Domaine : J · Auditeur : avocat du diable

## Verdict
Fondations saines et image vendable (bean rameur, lagon), mais personne n'a joué la promesse: sermon, Percepteur, vente et quota manquent dans `main`, et quatre amis ne peuvent pas se rejoindre. L'effort est allé à ce que l'acheteur ne voit pas (atoll 1:1, 29 agents). Risque: un prototype brillant mais vide, un seul testeur, des défauts révélés par les avis Steam.

Légende: «vu» = lu par moi; (NN) = rapport NN, non revérifié; scénario: supposé.

## Scénario d'échec
Si rien ne change:
- **2027-01-18**: page «Bientôt» (84), captures d'éditeur, logo «TAPAO» (69), «Lethal Company en bateau» (71), français seul (79); «Crownfall» renvoie au jeu de 2017 (78).
- **2027-06-14**: Next Fest unique (84, 89). Sermon: 20 s d'attente sans enjeu (03), une perle suffit au quota (05); aucun streamer ne peut inviter (27, 56). ≈ 4 000 wishlists (supposé), sous 7 000 (90).
- **2027-06-29**: accès anticipé à 7,99 $, en pleine Summer Sale (84, 85, 92). Semaine 1: 400 à 800 ventes, ≈ 1,2 à 2,4 k$ nets (85, 90), contre 17 k€ de plancher (99).
- **2027-07-06**: 25 avis «Mitigées» (90): «impossible d'inviter» (27), «mort à la minute 1, dix minutes d'attente» (28), «même île ×20» (63), «plante sur 8 Go» (42).
- **2027-07-20**: la presse polynésienne (supposé) relève un atoll habité, des «tiki» monstres, un marae pillé (72, 73); la satire, absente de l'écran (74), ne protège rien: trailer retiré.
- **2027-10**: moins de 30 joueurs simultanés (supposé), un clone paru (90). Variante plus fréquente: le silence, moins de 10 avis (49 % des sorties, 90).

## Constats
Cinq causes racines, parades en Recommandation.

- **[Bloquant commercial] 1. La promesse n'est pas jouable**
  - Constat : le seul différenciateur (sermon, Percepteur) n'existe qu'en notes; aucune manche ne se termine.
  - Preuve : 03, 11; vu: `AddZoneBonus` sans aucun appelant, 0 fichier Sermon, 0 «Percepteur».
  - Recommandation : fusionner T-054/055/057 (0 conflit, 39), rendre UNE zone jouable et la chronométrer avant T-112.
  - Effort : S (fusion), M (zone)
- **[Bloquant commercial] 2. Quatre amis ne peuvent pas jouer ensemble**
  - Constat : ni Rejoindre, ni invitation, ni voix; zéro test à deux machines.
  - Preuve : 27, 56, 58; vu: ni Steam ni Online dans `.uproject`, 0 `PreLogin`, 0 `NetworkFailure`.
  - Recommandation : LAN de 2 h sur `build/rec-T124` (PC2), `MaxPlayers=4`, spike Steam (AppID 480); L7a avant L6 (89).
  - Effort : S (test), L (L7a)
- **[Bloquant commercial] 3. La première heure mène au remboursement**
  - Constat : zéro tutoriel; le mort attend 8 à 11 min; défaite sans bilan; vingt îles, un seul décor.
  - Preuve : 13, 28, 30, 63, 85; remboursement: supposé.
  - Recommandation : première partie non éliminatoire sur FRG_01, NAUFRAGÉ si équipage anéanti seulement, «Réessayer» (09, 30).
  - Effort : S
- **[Bloquant commercial] 4. Le public ne peut ni trouver ni lancer le jeu**
  - Constat : nom pris, français seul, Ultra sous 60 FPS (éditeur), ni page ni wishlist.
  - Preuve : 78, 79, 45, 90; vu: `ProjectName=First Person Template`.
  - Recommandation : renommer (pré-tri de 2 h, 78), défaut «Moyen», test sur PC 8 Go, anglais d'abord.
  - Effort : S (nom), M (anglais)
- **[Majeur] 5. La polémique est lisible, la satire non**
  - Constat : lieu réel, «tiki», marae pillé, villageois létaux à l'écran; l'antidote reste en notes.
  - Preuve : 72, 73, 80; vu: 44 lignes de code citent Raraka/Motutapu.
  - Recommandation : villageois non létaux, `NoGrab` sur marae, «Veilleurs», aucun nom réel public, courriers DAG/DAF/mairie.
  - Effort : S

### Ce que l'équipe refuse probablement de voir
- **Outillage**: 29 agents, 35 skills, 110 scripts (vu); T-109 passe `review` en 12 min (journal), mais 83 `review` attendent un testeur qui joue ≈ 6 h par semaine (supposé, 92). Les relecteurs sont aussi des agents (99). Cet audit ajoute plus de 530 constats (92). Plafond de 15 `review` (91).
- **Atoll 1:1**: payé cinq fois (42, 47, 63, 70, 72), invisible à l'acheteur (73); le cahier §1 voulait des îles faites à la main (vu). Trois îles suffisent au MVP (91).
- **Satire coloniale**: PEGI, streamers et presse lisent l'écran, pas le cahier (80).
- **Multijoueur**: 30 RPC, 27 classes répliquées (36), aucune jouée à deux machines: un jeu solo, pour l'instant.
- **Seul testeur**: sur 22 `fait`, deux tâches de jeu «testées par l'utilisateur» (vu); délai avant déluge 120, 300 puis 540 s sans partie chronométrée (01, 93); aucune télémétrie (95).

## Manques
Un inconnu qui joue; un ami qui rejoint; une vente qui clôt la manche; un regard polynésien.

## Quick wins
1. Copier le dépôt hors C:; supprimer 15 worktrees fusionnés: seul exemplaire, disque à 99 % (vu, 93).
2. LAN hôte + client sur `build/rec-T124` (58, 94).
3. Fusionner T-054/055/057, lancer leurs ≈ 69 specs (39).
4. Geler la diffusion des builds de recette et «Crownfall», «Raraka», «tiki» en public (78, 93, 97).
5. Trois inconnus, 20 min, facilitateur muet (13, 94).

## Questions qui challengent l'équipe
1. Les builds T-120/122/124 servent à montrer: qui les a joués à quatre?
2. Dans 30 jours, que publiez-vous, et pourquoi n'est-ce pas déjà ce que vous construisez?
3. Quel chiffre dirait «réussi», lequel vous ferait arrêter? Fixez-les avant le test.
