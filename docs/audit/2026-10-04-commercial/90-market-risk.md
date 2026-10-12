# 90 — Risques marché
Domaine : I · Auditeur : analyste des risques marché

## Verdict
La niche croît (82), mais près d'une sortie Steam sur deux reste sous 10 avis : la visibilité se gagne en wishlists. Rien n'est amorcé côté marché (nom, page, internet) et trois quasi-jumeaux existent déjà (81). Risque principal : un lancement au plancher (5 à 8 k€ en semaine 1) ou une polémique sur un lieu réel.

## Constats
Sources web : 6 appels, une page lue (GamingOnLinux), le reste en résumés : ordres de grandeur.

- **[Bloquant commercial] Découvrabilité : sans wishlists, pas de vente**
  - Constat : 19 008 jeux sortis en 2025 au 12/12, ~55 par jour ; 9 269 (48,8 %) sous 10 avis. « Popular Upcoming » ≈ 7 000 wishlists ; conversion 12 % (jour 1) à 20 % (semaine 1). Plancher : 7 000 → 840–1 400 ventes → 5–8 k€ nets à 10 € (TVA, part Steam 30 % : supposé).
  - Preuve : https://www.gamingonlinux.com/2025/12/over-19000-games-have-released-on-steam-in-2025-with-nearly-half-seeing-fewer-than-10-reviews/ (SteamDB), backlog.md (0 « Steam »), https://howtomarketagame.com/2026/06/25/archive-how-many-wishlists-should-i-have-when-i-launch-my-game/, https://gamedevreports.substack.com/p/gamediscoverco-the-state-of-steam
  - Recommandation : règle go/no-go : accès anticipé seulement à ≥ 7 000 wishlists à J-60, sinon report.
  - Effort : L

- **[Majeur] Saturation et copie : le risque est l'identité, pas l'offre**
  - Constat : « friendslop » naît en mars 2025, moquerie des clones. Hits : Lethal Company (oct. 2023), Content Warning (avr. 2024), R.E.P.O. (fév. 2025), Peak (juin 2025) : un tous les 4 à 10 mois, chacun avec une idée neuve, pour un public commun (88) qui zappe (82). Un gros éditeur est improbable à court terme (supposé), un clone de petite équipe non (81). Nombre de sorties du genre : introuvable.
  - Preuve : https://www.avclub.com/not-every-co-op-game-is-friendslop, https://gamegeeker.com/editors-picks/best-co-op-horror-games-on-steam?market=DE, 81, 82, 88
  - Recommandation : révéler marque et satire d'abord, mécaniques ensuite ; rien de public avant une tranche de 3 min montrant le sermon.
  - Effort : S

- **[Majeur] Fenêtre : la page est le chemin critique**
  - Constat : 89 propose Next Fest (fév. ou juin 2027) puis accès anticipé en août–sept. 2027, soit ~4 ans après Lethal Company. 7 000 wishlists à J-60 (début juillet) exigent ≈ 40 par jour si la page ouvre en janvier, ≈ 140 en mai (calcul). Le cahier exclut la publication commerciale (§1), sans lot pour nom, page ni trailer.
  - Preuve : 89, cahier §1:28, §11:238-242, calcul : EA 1er sept., page 10 janv. (174 j) ou 15 mai (49 j)
  - Recommandation : page ouverte en janvier, après les 4 portes de 89 ; ADR de passage commercial (99) ; L7a avant L6 pour une démo coop (89).
  - Effort : M

- **[Majeur] Streamers : pas de moteur de clips, plan sans viralité**
  - Constat : R.E.P.O. a pris la tête des plus-streamés dès sa sortie, mais les créateurs amplifient aussi un coop cassé (83). Ici ni voix de proximité, ni spectateur (12, 18), et la scène-signature, le sermon, n'est pas dans `main` (81). La viralité n'est pas pilotable : hors plan financier.
  - Preuve : https://gamegeeker.com/editors-picks/best-co-op-horror-games-on-steam?market=DE, 12, 18, 81, 83
  - Recommandation : budgéter sur le plancher ; aucune clé avant la barrière de 83 ; mesurer en playtest les parties donnant un moment partagé.
  - Effort : M

- **[Bloquant commercial] Nom : trancher avant la page**
  - Constat : « Crownfall » est pris (78). Ses remplaçants « Sinking Fund » et « Drowned Assets » frôlent le champ lexical de deux quasi-jumeaux (81) : confusion de recherche possible (supposé). Changer coûte ~0 aujourd'hui (0 occurrence dans le code, 71), cher après page, logo et trailer.
  - Preuve : 78, 81, 71
  - Recommandation : la recherche exacte du nom (Steam, YouTube, TikTok) doit renvoyer notre jeu en tête ; décision sous 14 jours, avant tout asset public.
  - Effort : S

- **[Majeur] Réputation : risque asymétrique au lancement**
  - Constat : un clip lu au premier degré (colons, atoll habité qui coule, villageois tués) peut lancer la polémique, la satire n'étant pas jouable (72, 73, 81). Avec 10 à 50 avis, quelques négatifs ramènent la note à « Mitigées » (supposé) ; PEGI 18 si « Discrimination » (80).
  - Preuve : 72, 73, 80
  - Recommandation : aux 4 portes de 89, ajouter Content Survey à blanc, test des 10 secondes (8 sur 10 désignent la Couronne comme cible) et plan B de 7 jours (variante neutre pilotée par DataAsset).
  - Effort : M

## Manques
1. Objectifs de wishlists par jalon (J-180, J-60) et décompte du genre.
2. Plan de crise : FAQ, porte-parole, variante neutre.

## Quick wins
1. Étendre le script de veille de 88 au décompte du genre et aux nouvelles sorties « sinking, flood, quota » : 2 h.
2. Test de recherche exacte des noms candidats (Steam, YouTube, TikTok) : 2 h.
3. Séance investisseurs (T-120) : trois scénarios (plancher, médian, hit), jamais le hit seul.
4. Inscrire la règle go/no-go au lot L9 de 89 : 1 h.

## Questions qui challengent l'équipe
1. À 7 000 wishlists, la semaine 1 rapporte 5 à 8 k€ : quel scénario montrez-vous aux investisseurs ?
2. Que ne copie pas un clone de 6 mois, et est-ce jouable aujourd'hui ?
3. Qui retire le trailer le jour J, vers quelle variante de repli ?
