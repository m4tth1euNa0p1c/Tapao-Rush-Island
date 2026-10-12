# 87 — Communauté et accès anticipé
Domaine : I · Auditeur : community manager jeu vidéo

## Verdict
Aucune infrastructure communautaire : `docs/` hors audits ne compte aucun devlog, réseau social ni accès anticipé, et le jeu n'a ni nom libre (78), ni signalement, ni version affichée. Le plan avance par portes de maturité, non par dates, et l'accès anticipé sort de ces six mois. Risque : un Discord public avant qu'un inconnu puisse lancer le jeu, rejoindre un ami et signaler un bug fabrique la première mauvaise réputation.

## Constats
Sources web : 6 appels, résumés. 83 et 84 absents.

- **[Bloquant commercial] Aucune boucle de retours**
  - Constat : ni version au menu (exigée §16:305), ni bouton Signaler, ni journaux Shipping. Un rapport de testeur est inexploitable, alors qu'un correctif se package en 2–4 min.
  - Preuve : grep version : 0 ; 37 ; `v1-rec-investisseurs.md:10`.
  - Recommandation : Signaler : zip local (journal, version, hash, fragment, graine, GPU, capture) déposé par le joueur ; forum Discord #bugs au gabarit `bug-report.md` ; formulaire de 5 questions ; triage dominical 1 h vers `T-NNN`, pré-classé par les agents.
  - Effort : M

- **[Majeur] Discord : deux temps, petit, présent**
  - Constat : serveur vide ou ingérable, crédibilité perdue. Les sources prônent départ minimal et 15 min de présence quotidienne, pas des bots. Sans matchmaking (§16:323), il sert de recherche d'équipage et de vocal (§8:187).
  - Preuve : bugnet.io/blog/how-to-set-up-a-discord-server-for-your-game.
  - Recommandation : Atelier privé au 1er test externe (portes : nom, build lancé sur 3 PC neufs, rejoindre un ami (56), Signaler), public avec la page Steam (porte : un inconnu finit une manche en moins de 15 min). Salons : règles, annonces, général, bugs, retours, clips, équipage, vocaux. Rôles : Capitaine, Modérateur, Testeur, Matelot. AutoMod pour seul bot ; six règles, aucune moquerie des insulaires ; annonces FR+EN.
  - Effort : S

- **[Majeur] Roadmap : horizons, pas promesses**
  - Constat : Valve interdit les « promesses spécifiques sur des événements futurs » ; `backlog.md:34-37` affiche L7, E1, L8+.
  - Preuve : partner.steamgames.com/doc/store/earlyaccess.
  - Recommandation : page Cap (Maintenant, Ensuite, Plus tard), sans date, avec les statuts de preuve (CLAUDE.md) ; E1 et L8 en explorations. Devlog tous les 15 jours (Discord, puis Steam News) tiré de `journal.md` : l'agent rédige, l'humain signe ; `board.md` reste privé.
  - Effort : S

- **[Majeur] Réseaux : peu, en français d'abord**
  - Constat : interface française seule (2,1 % des clients Steam, 79) ; r/IndieDev (≈ 260 k, développeurs), r/CoOpGaming (≈ 40 k, joueurs) ; bean muet, sermon absent (18).
  - Preuve : fungies.io/indie-game-marketing ; steampageanalyzer.com/blog/reddit-indie-game-marketing ; gummysearch.com/r/CoOpGaming.
  - Recommandation : TikTok + Shorts, 2 clips/sem. ; X 1 post/sem., Bluesky en miroir (supposé) ; Reddit : GIF de dev, 1 post/3 sem., commentaires d'abord (règle 9:1, empirique). Premiers clips : noix sur la tête, trompette, naufrage (18, après ses quick wins), légendés build dev ; ni marae ni découpe (72). Anglais quand l'UI l'est.
  - Effort : M

- **[Majeur] Accès anticipé et critiques : contrat, protocole**
  - Constat : Valve exige un jeu jouable, aucune promesse précise, attentes posées partout ; sous 40 % d'avis positifs, moins de mise en avant ; solliciter un avis depuis le jeu est interdit. Ici : manches et vente non codées, multijoueur jamais testé à deux, français seul.
  - Preuve : partner.steamgames.com/doc/store/reviews ; board T-112 → T-116 `a-faire` ; 30.
  - Recommandation : contrat public en brouillon dès M6 : état réel, langues, modes testés, sauvegardes réinitialisables, rythme en intention. Réponses : bug → corrigé en vX ; design → noté, plus tard ou non car ; polémique polynésienne → réponse préparée, 24 h de recul.
  - Effort : S (texte), L (conditions)

- **[Majeur] Un humain, ≈ 150 h : plan 6 mois**
  - Constat : seul humain, testeur et décideur ; aucune tâche communautaire au chantier (`board.md` : 0).
  - Preuve : BRIEF ; heures supposées.
  - Recommandation : plafond horaire (au-delà, canal coupé), 2 permanences de 45 min, jamais de DM, 2 modérateurs bénévoles dès M5. Les agents trient et rédigent, l'humain signe et publie. Cibles, porte manquée = décalage :
    - M1 oct. (3 h/sem.) : nom clearé, pseudos, règles, Signaler ; rien de public.
    - M2 nov. (4 h) : Atelier, 10–20 testeurs, build par itch.io privé ou Steam Playtest, VPN virtuel sans L7a (supposé), 10 clips en banque.
    - M3 déc. (6 h) : page Steam (84), Discord public, devlog.
    - M4 janv. (6 h) : soirée d'essai bimensuelle, page Cap publiée.
    - M5 févr. (8 h) : démo (84), créateurs (83), modérateurs.
    - M6 mars (8 h) : sondage, contrat, go/no-go.
  - Effort : L

## Manques
1. Nom clearé, pseudos.
2. Signaler, version, journaux Shipping.
3. Rejoindre un ami (56), anglais (79).

## Quick wins
1. OBS : tampon de rejeu de 30 s à chaque playtest.
2. Version et hash au menu.
3. Règles du serveur (6 lignes), lignes rouges (1 page).
4. Gabarits #bugs et formulaire (`bug-report.md`, `playtest-report.md`).

## Questions qui challengent l'équipe
1. Répondrez-vous en public de Raraka, des marae et des tiki, et qui valide la réponse avant le premier clip ?
2. Quelles 150 h retirez-vous à T-112 → T-116, et que coupez-vous quand le plafond saute ?
3. Si dix inconnus ne peuvent pas rejoindre un ami, ouvrez-vous la page Steam ?
