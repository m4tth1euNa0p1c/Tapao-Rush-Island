# 93 — Registre des risques
Domaine : J · Auditeur : gestionnaire de risques

## Verdict
Treize risques, dont quatre à 20/25 : disque saturé, aucune copie hors machine, coop jamais jouée à deux, boucle non bouclée. Tout converge vers des points de défaillance uniques : le disque C:, le seul humain, le terrain réel sous licences non prouvées, des agents sans verrou. Risque commercial principal : perdre ou exposer un jeu jamais joué sur deux machines; les parades les moins chères tiennent en une semaine.

## Constats
Échelle: P (probabilité avant l'accès anticipé), I (impact), estimés de 1 à 5; rang selon P×I, ex æquo par urgence puis irréversibilité. « Vu »: mesuré le 2026-10-04 vers 05 h; (n): rapport n. Propriétaire: décideur; exécutant.

|#|Risque|P×I|Preuve|Signal d'alerte|Parade (effort)|Propriétaire|
|---|---|---|---|---|---|---|
|1|**[Majeur] Disque C: saturé**|5×4=20|Vu: 23 Go libres, creux de 13,7 Go vers 05 h 10, ≈40 à 03 h (39); .worktrees 53,2 Go, 21 arbres dont 15 fusionnés (36,6 Go).|Moins de 15 Go libres (déjà franchi).|Supprimer les 15 arbres fusionnés propres; gc en gate INT; plafond de 6 arbres (S).|Humain; ue-build-engineer|
|2|**[Bloquant commercial] Aucune copie hors machine**|4×5=20|Vu: remote Git vide, un seul SSD, 70 fichiers Content/Source non suivis, dépôt né il y a 24 h (93 commits, 1 auteur).|Dernière copie de plus de 24 h.|Copie nocturne (bundle Git, .git/lfs, Content) hors C: (S); remote privé (M).|Humain; ue-integration-lead|
|3|**[Bloquant commercial] Coop jamais jouée sur deux machines**|4×5=20|0 spec réseau (36); le seul PIE à 2 depuis L_Boot a perdu son client, non retesté (journal:253-255); réplication physique non choisie (52); deux voyages seamless par manche (54).|Aucun test LAN daté.|PIE à 2 avec 100 ms aujourd'hui (S); LAN sur le 2e PC avant T-113 (M).|ue-network-engineer|
|4|**[Bloquant commercial] Boucle non bouclée, intégration sans propriétaire**|4×5=20|Vu: sermon, arrimage, sauvegarde (T-054/055/057) hors main depuis 23 h; barre, vente et manches à faire (T-112 à T-116).|Plus de 15 tâches en review.|Fusion dans integration aujourd'hui (39); gel des nouveautés; jalon « une manche à deux » (M).|Humain; ue-integration-lead|
|5|**[Majeur] Carte monolithique, performance jamais mesurée**|4×4=16|Lvl_Raraka 110 Mo, 816 000 instances; 1 seul L_Frag_* sur 20; PC de dev sous 60 FPS en Ultra (45).|Cook de mesure: plus de 8 Go RAM, 20 s à froid ou 16,6 ms (43).|Finir et mesurer FRG_01 avant les 19 autres; Lvl_Raraka hors cook (L).|ue-performance-engineer|
|6|**[Majeur] Réputation: lieu réel, sacré, villageois tuables**|4×4=16|Marae pillé, « tiki » monstres, villageois tués, atoll habité noyé, aucune consultation (72, 73); PEGI 16 à 18 possible (80).|Première capture publique sans relecture polynésienne.|Villageois non létaux, marae NoGrab, « Gardiens », noms réels retirés (S–M); deux conseillers payés.|Humain; ue-narrative-designer|
|7|**[Majeur] Marché: lancement au plancher**|4×4=16|19 008 sorties Steam en 2025, 49% sous 10 avis; plancher de 7 000 wishlists, soit 5–8 k€ (90); français ≈2% (82); ni page ni AppID.|Moins de 7 000 wishlists à J−60.|Tranche verticale, nom clearé, page; anglais d'abord; règle go/no-go chiffrée (L).|Humain|
|8|**[Majeur] Éditeur et dépôt partagés (5 sessions Claude, Codex)**|5×3=15|Vu: 8 plantages en 25 h; éditeur relancé 2 fois en 30 min; test contaminé par une requête MCP externe (journal:326); règle « pas de Live Coding en PIE » absente du dépôt.|Plus d'un plantage par 8 h.|lock.py (à créer: bail, TTL); deux fenêtres de build par jour; règles dans AGENTS.md (S).|Humain; ue-integration-lead|
|9|**[Majeur] Un seul humain, validations en file, contrat mouvant**|5×3=15|Vu: 83 tâches review; cahier v1.2 à v1.4 en 28 h; délai avant déluge 120, 300 puis 540 s (board:108,133), tests restés à 120.|Validation en plus de 24 h.|Recette de 15 min tirée des notes « non testé »; gel du cahier 2 semaines; deux testeurs (M).|Humain|
|10|**[Bloquant commercial] Licences du terrain réel**|3×4=12|Cocoteraies DAG « recherche/éducation » (≥22 431 cocotiers); masques sur imagerie Maxar; atlas IRD sans licence (70, 73).|Pas de réponse sous 30 j.|Deux courriers (DAG, DAF); geler tout nouvel usage; plan B procédural (S, puis M).|Humain; ue-gis-cartographer|

Rangs 11 à 13:

**11. Nom « Crownfall »** (4×3=12): jeu Steam de 2017, événement Dota 2 (78). Signal: AppID ou logo avant clearance. Parade: geler, pré-tri de 2 h, INPI (S). Humain.

**12. Dépendance à l'IA** (3×4=12): planches générées sans registre (70), divulgation Steam requise (partner.steamgames.com/doc/gettingstarted/contentsurvey), code écrit par agents. Signal: refus Steam, agents indisponibles. Parade: PROVENANCE.md, avocat PI, relecture externe (M). Humain.

**13. Distribution Windows** (3×4=12): binaires non signés; Smart App Control a bloqué la DLL éditeur le 10-03 (48); effet sur les joueurs: supposé. Signal: échec sur un 2e PC sain. Parade: essai SAC actif, certificat (M). ue-build-engineer.

## Manques
Restauration testée (aucune sauvegarde n'existe), intégrateur nommé, seuils d'alerte automatiques (`chantier.py --env`), plans B datés (terrain procédural, variante neutre, 90), budget juridique et consultants.

## Quick wins
1. Bundle Git et copie hors C: ce soir; supprimer les 15 worktrees fusionnés.
2. Fusionner T-054/055/057 dans integration; lancer leurs 69 specs et un PIE à 2.
3. Écrire à la DAG et à la DAF; retirer « Raraka » et « tiki » des textes publics.
4. Copier les règles d'éditeur (mémoire locale) dans AGENTS.md et CLAUDE.md.

## Questions qui challengent l'équipe
1. Qui, hors vous, restaure, compile et valide une manche si vous êtes absent quinze jours?
2. Pourquoi villages, marae et camps (T-064, T-075, T-096, T-102) ont-ils précédé la fusion du sermon (T-054)?
3. Quel événement (licence refusée, mairie, hôte qui plante) vous ferait renoncer à l'atoll réel, et quel plan B daté avez-vous?

Web: 5 appels, dont une lecture (Steam Content Survey).
