# 85 — Prix et modèle économique
Domaine : I · Auditeur : business developer

## Verdict
Aucun prix, modèle économique ni statut juridique n'est décidé (cahier §1). Recommandation : premium en accès anticipé à 7,99 $ (7,49 €), 9,99 $ annoncé pour la 1.0, zéro microtransaction, cosmétiques gagnés, DLC après la 1.0. Risque : le développeur touche ~38 % du prix affiché ; les souhaits Steam décident (an 1 : ~2 k$ prudent, ~14 k$ médian avec démo et Next Fest).

## Constats

- **[Bloquant commercial] Accès anticipé non vendable, 1.0 hors de portée**
  - Constat : aucune manche ne se termine ; ni invitation Steam ni test à deux machines (11, 12, 56). La semaine de lancement pèse ~1/3 de l'année et ne se rejoue pas.
  - Preuve : `Build.cs:49,51` (OnlineSubsystem commenté), `.uproject` sans Steam, source 3.
  - Recommandation : n'entrer qu'avec RUN01–RUN13 verts, une partie à 4 sur deux réseaux, un préréglage « bas » (45), un tutoriel (13).
  - Effort : L

- **[Majeur] Prix : fenêtre 8–10 $, contenu trop mince pour plus**
  - Constat : Lethal Company et R.E.P.O. 9,99 $ (accès anticipé), Content Warning gratuit puis 7,99 $, Peak 7,99 $. Un équipage de 4 paie 32 $ à 7,99 $, 60 $ à 14,99 $. Steam demande 30 jours sans changement de prix après la sortie ; toute hausse bloque les remises 30 jours. Prix régionaux facultatifs, peu utiles sans anglais (cahier §1, 79).
  - Preuve : source 5 ; pages Steam, contenu mince (12), rendu lourd (45).
  - Recommandation : 7,99 $ au lancement ; 9,99 $ à la 1.0 si le plancher de contenu de 12 est atteint (+25 % par exemplaire, rentable si les ventes reculent de moins de 20 %), hors soldes ; 12,99–14,99 $ seulement avec ≥ 85 % d'avis positifs. Remises : aucune avant J+30, ≤ −20 % en an 1 (supposé). Pas de jour gratuit : il suppose une audience.
  - Effort : S

- **[Majeur] Net ~38 % du prix, projections modestes**
  - Constat : 100 ventes à 7,99 $ : 799 $ affichés, 551 $ hors TVA (−16,7 % en France) et prix régionaux (ratio 0,69 calibré sur PEAK : 17 M$ pour 3,1 M ex.), 469 $ après soldes (−15 %), 431 $ après remboursements (−8 %), 302 $ après Steam (30 %) : 3,02 $ par exemplaire, avant impôt. An 1 ~3 × semaine 1 ; semaine 1 = souhaits × 0,10 (prudent), 0,15 (médian), 0,25 (optimiste). Prudent (2 000 souhaits) ~600 ex., ~1,8 k$, sous les frais fixes (~2 k€, supposé : AppID 100 $, nom, certificat, capsule). Médian (10 000, avec page, démo, Next Fest) ~4 600 ex., ~14 k$. Optimiste (40 000) ~30 000 ex., ~90 k$.
  - Preuve : sources 1–4 ; soldes, remboursements, ratios supposés.
  - Recommandation : publier à ≥ 10 000 souhaits, pas sous 5 000 (supposé).
  - Effort : S

- **[Majeur] Remboursements : la fenêtre de 2 h contient le premier échec**
  - Constat : Steam rembourse sous 2 h de jeu et 14 jours. Le mur du quota tombe aux manches 5–7 (19), soit ~60–105 min (manche ~12–15 min, supposé) : dans la fenêtre. Hôte qui rame (51), rendu non mesuré (45) : motifs de remboursement. Hypothèse 8 % ; 15–20 % si instable (supposé).
  - Preuve : source 1 ; 19, 45, 51.
  - Recommandation : un moment de fierté avant 60 min ; alerte si remboursements > 15 % en deux semaines.
  - Effort : M

- **[Majeur] Modèle : premium sans microtransaction ; pas de 4-pack**
  - Constat : Semiwork promet que R.E.P.O. ne sera ni gratuit ni à microtransactions (cosmétiques gagnés) ; nos skins, « façon R.E.P.O. » et sans effet de jeu (fiche bean §3), s'y prêtent. Le pack payant de 76 vaut ~1,5 % du revenu (3,99 $, 3 % d'acheteurs, supposé) et exige profil, PlayerState, Steam, absents (T-028 `a-faire`). Valve ne créerait plus de 4-packs (Steam Community, non confirmé).
  - Preuve : sources 6, 7.
  - Recommandation : rien de payant en accès anticipé, ne jamais vendre de « couronnes » (elles entrent dans le quota). Après la 1.0 et ≥ 85 % d'avis positifs : « Pack Couronne » 3,99 $ (tenues, chapeaux), puis DLC de contenu 4,99 $. Pack d'amis : vérifier dans Steamworks ; sinon achat-cadeau et remise « équipage » −15 %.
  - Effort : L

## Manques
1. Statut juridique et fiscal, compte Steamworks, accord de publication (CLAUDE.md).
2. Modèle financier, cible de souhaits, questionnaire d'accès anticipé.
3. Profil, PlayerState, plugin Steam (14, 56).

Sources web (secondaires, sauf [5]) :
[1] https://presskit.gg/field-guides/how-much-does-steam-take
[2] https://newsletter.gamediscover.co/p/steam-the-new-wishlists-to-first
[3] https://gameworldobserver.com/2021/09/23/how-games-first-week-revenue-on-steam-correlates-with-year-one-revenue
[4] https://alineaanalytics.com/blog/steam_june_2025/
[5] https://partner.steamgames.com/doc/store/pricing
[6] https://egw.news/gaming/news/27420/repo-wont-be-free-to-play-and-thats-a-win-for-ever-lKaUdAy5m
[7] https://steamcommunity.com/discussions/forum/0/601903104843117770

## Quick wins
1. ADR « modèle économique » (½ j).
2. Tableur de projection (½ j).
3. Au playtest : quatre questions de prix (Van Westendorp), « rembourseriez-vous ? ».
4. Nom (78) fixé avant l'AppID.

## Questions qui challengent l'équipe
1. Combien de souhaits avant de publier, et que faites-vous à J-30 avec 2 000 ?
2. Quel plaisir précède le premier échec, avant la fin des 2 h de remboursement ?
3. 14 k$ en an 1 justifient-ils des mois de travail ? Quel chiffre dirait « réussi » ?
