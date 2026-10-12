# 82 — Audience cible : segments, marché, personas
Domaine : I · Auditeur : analyste marché

## Verdict
Aucun document ne définit l'audience ; seuls « PC, clavier-souris, français, 1–4 » sont décidés. Ce cadrage vise de fait le segment le plus porteur du genre (bandes d'amis en vocal, niche de 366 000 simultanés) sans ses prérequis : invitation, voix, PC modeste, anglais. Risque : fenêtre courte, face à un jeu que les amis ne peuvent ni rejoindre, ni lire, ni faire tourner.

## Constats
- **[Bloquant commercial] Choix actuels : cœur de cible non servi**
  - Constat : audience non définie hors audits (grep). 1–4 joueurs : juste (norme, 59) ; tester aussi 2 et 3. Listen server : juste (0 € d'infra), mais sans Héberger, Rejoindre ni voix, et seul l'hôte garde la partie. Clavier-souris : juste pour un cœur PC ; manette et Deck secondaires (26). Français seul : insuffisant.
  - Preuve : STapaoScreens.cpp:143-148 ; cahier §7, §9, §16 ; ADR-005 §3 ; 12, 14, 27, 56.
  - Recommandation : `docs/audience.md` d'une page ; recette « quatre amis, deux réseaux, partie lancée en moins de 2 min ».
  - Effort : S (brief), L (L7)
- **[Majeur] Marché réel, à hits, fenêtre courte**
  - Constat : niche « friendslop » : 280 000 simultanés (mars 2025), 366 000 (juin 2026). R.E.P.O., Peak, Lethal Company ≈ 46 M d'exemplaires cumulés. Les joueurs zappent : un hit capte plus de 50 % de l'audience au lancement, ≈ 10 % après 3 à 9 mois. Mage Arena (11,75 % de part) : 624 000 exemplaires en une semaine. Viser 5 à 12 % = 14 000 à 44 000 simultanés (calcul).
  - Preuve : gameworldobserver.com (2026-08-03) ; nettosgameroom.com, pcgames.de (à recouper).
  - Recommandation : n'ouvrir l'accès anticipé qu'après invitation, voix, tutoriel (démo avant) ; une mise à jour par trimestre.
  - Effort : M
- **[Majeur] Trois personas (hypothèses à valider)**
  - Constat :
    - **Léo, hôte de la bande** (cœur) : 16–28 ans, PC moyen, Discord ; soirées de 1 à 3 h ; veut rires et anecdotes ; frustré par le lancement, l'ami au PC faible, la partie de 1,5 à 5 h que seul l'hôte garde.
    - **Maëlle, créatrice** : Twitch, YouTube, TikTok ; PC puissant + OBS, hôte surchargé (51) ; direct de 2 à 4 h, clips de 30 s ; veut moments lisibles, monétisables ; frustrée par la démonétisation (18, 80), un contenu épuisé en 10 h.
    - **Karim, vétéran de l'extraction** : solo ou duo, Discord, PC moyen ; 45 à 90 min ; veut maîtrise, « encore une manche » ; frustré sans lobby public (§16) ni solo équilibré (07).
  - Preuve : supposé ; recoupé avec 12, 18, 19, 51.
  - Recommandation : 5 entretiens ; un persona = un critère de recette.
  - Effort : S
- **[Bloquant commercial] Français seul : ≈ 2 % de Steam**
  - Constat : français 2,1–2,3 % des joueurs de l'enquête Steam, anglais 34–36 %, chinois simplifié 27–31 % (juin 2025 ; sept. 2026 selon 79). Au score de Mage Arena, FR seul ≈ 13 000 à 14 500 exemplaires (calcul, proportionnel supposé). Zones (supposé) : Europe francophone et Québec en tête de pont (créateurs, humour « Bureau ») ; volume : anglais, chinois, russe.
  - Preuve : uhiyama-lab.com ; 79 ; cahier §18.
  - Recommandation : FR langue d'écriture ; EN livré avec la démo, page Steam EN d'abord ; zh-Hans après l'accès anticipé.
  - Effort : M (EN), L (zh)
- **[Majeur] Solo : secondaire à la vente, requis à la recette**
  - Constat : aucune donnée publique trouvée sur la part des solos (supposé : minorité des ventes, source d'avis « injouable seul »). Aujourd'hui, seul le quota varie avec l'équipage (07). Sans matchmaking public (§16), c'est le repli des joueurs sans bande.
  - Preuve : cahier §1, §6.3, §12 ; 01, 07.
  - Recommandation : « solo possible » sur la page, pas « solo-friendly » ; table DA joueurs → quota, sermon, gardiens ; aucun contenu solo dédié.
  - Effort : M
- **[Majeur] Le groupe joue sur le PC de son maillon faible**
  - Constat : les quatre amis doivent tous faire tourner le jeu ; 60 FPS non démontré (RTX 5060 Ti sous 60 en Ultra, éditeur), 4 à 5 Go de RAM estimés : audience réduite aux bandes dont tous les PC suivent (supposé).
  - Preuve : 42, 45.
  - Recommandation : « PC de l'ami » (GTX 1660, 8 Go) en test d'acceptation ; palier Faible.
  - Effort : L

## Manques
1. Version anglaise.
2. Invitation et voix (12, 56).
3. Démo rejoignable par l'hôte (supposé faisable) : lève « tous achètent ».
4. Lobby public ; mode « stream-safe ».

## Quick wins
1. Brief d'audience d'une page (½ j).
2. Sondage de cinq questions sur deux Discords friendslop (½ j).
3. Page Steam en EN d'abord (½ j).

## Questions qui challengent l'équipe
1. Nommez trois acheteurs réels : lequel a un PC suffisant et trois amis libres jeudi soir ?
2. Les clips circuleront en anglais : pourquoi écrire la satire en français sans traduction prévue ?
3. Si 30 % des acheteurs jouent seuls (supposé), acceptez-vous leurs avis « injouable seul » ?

Sources : https://gameworldobserver.com/2026/08/03/analyst-there-are-many-games-in-the-friendslap-niche-but-it-is-not-oversaturated, https://uhiyama-lab.com/en/blog/gamedev/steam-game-localization-language/, https://www.nettosgameroom.com/2025/11/the-rise-of-friendslop-games.html, https://www.pcgames.de/Spiele-Thema-239104/News/Steam-Friendslop-Multiplayer-Koop-Meccha-Chameleon-1545449/
