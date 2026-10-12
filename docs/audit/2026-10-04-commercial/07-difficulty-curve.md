# 07 — Courbe de difficulté, solo / quatre joueurs, équité
Domaine : A · Auditeur : designer progression et difficulté

## Verdict
La difficulté par manche n'existe qu'en arithmétique : le +10 %/manche n'est lu par aucun ennemi, et le quota reste le seul levier, sans mesure ni réglage. Solo et équipage de quatre ne diffèrent que par ce quota, alors que gardiens, nuit, horloge et cale ne bougent pas. Risque commercial : une partie perdue d'un coup sur un tirage, sans preuve que la courbe se gagne (perle, cale, horloge : voir 05 et 01).

## Constats
- **[Majeur] Le +10 % par manche n'agit sur rien, et ne se verrait pas**
  - Constat : seuls les tests appellent `DifficultyForRound` ; `ATikiDirector` ne lit aucune difficulté (ADR-005 §12 la prévoit). « +10 % » de quoi ? À +50 %, 80 PV deviennent 120 : 4 coups de sabre au lieu de 3 (combo de 115), toujours 2 balles de mousquet (70) ; de M1 à M5 le nombre de coups ne change pas ; 3 gardiens font 3, 3, 4, 4, 4, 5. Plafond à M6, ensuite seul le quota monte. À temps fixe, si une zone coûte déjà ses 120 s (01, 03), +50 % d'ennemis la rend déficitaire (supposé).
  - Preuve : RunCalculator.cpp:48-52, RunSpec.cpp:54-56, TikiRules.h:70,94, cutlass.md:17, musket.md:13, cahier §24.6 l.582.
  - Recommandation : table DA de paliers par manche (gardiens 3 → 5, embuscade 18 → 12 s, plafond de nuit 3 → 6, Crabe dès M3, Veilleur dès M5), puis modificateurs de contrat (voir 10).
  - Effort : M
- **[Majeur] Plus d'ennemis, plus de butin : la difficulté finance le joueur**
  - Constat : chaque guerrier tué lâche une arme de 30, 45 ou 50 $. La nuit (≈ 40 % du temps), 4 guerriers sont maintenus vivants et un nouveau sort toutes les 6 s : ≈ 500–2 000 $ d'armes par manche (supposé). +10 % d'ennemis agrandit ce gisement, et « armes ×3 » (05) en ferait la stratégie dominante.
  - Preuve : TikiDirector.cpp:654, TikiRules.h:310-314, BP_Drop_* lus en binaire, characters-tiki.md:188.
  - Recommandation : drops réservés aux 9 gardiens désignés ou plafonnés par manche ; faire monter la menace par ennemi (dégâts, zone télégraphiée), pas leur nombre.
  - Effort : S
- **[Majeur] Solo / quatre : seule la cible bouge**
  - Constat : quota ×(1 + 0,25 × (joueurs − 1)), figé au lancement : 530 par tête à quatre en M5 contre 1 210 en solo (−56 %). Gardiens (3 par zone), horloge, cale et offre ne bougent pas ; la nuit plafonne à 4 guerriers pour tout l'équipage, donc plus on est nombreux, moins chacun est pressé. Deux morts précoces laissent deux bras pour la cible de quatre. Le solo manque de bras, l'équipe d'offre (05, 08).
  - Preuve : RunCalculator.cpp:44, RunTypes.h:117, TikiDirector.cpp:654, TikiRules.h:70,310, cahier §6.3 l.124.
  - Recommandation : table DA joueurs → quota, cale, plafond de nuit, durée du sermon ; échelle sous-linéaire à mesurer ; recette n°21 jouée à 1, 2 et 4.
  - Effort : M
- **[Majeur] Éliminatoire sur tirage, sans filet ni réglage**
  - Constat : tirage uniforme parmi 19 îles de 17 à 71 ha, heure de départ aléatoire, `IndicativeDifficulty` laissé à 1 et jamais lu : deux manches de même quota ne se valent pas. Pourtant une seule ratée clôt la partie, non continuable (≈ 1 h perdue à M5, supposé). Le §16 ne règle que des durées : ni préréglage Novice/Expert, ni relance.
  - Preuve : ExpeditionIslandValidation.cpp:182-200, ExpeditionIslandDefinition.h:65-67, DayNightRules.h:48-55, cahier §24.7 l.589, §16 l.317.
  - Recommandation : cote mesurée par fragment et tirage par palier (01, 10), une relance de manche par partie (09), préréglages quota/horloge/ennemis exclus des décorations (14).
  - Effort : M
- **[Majeur] Aucune validation chiffrée de la courbe**
  - Constat : les tests couvrent l'arithmétique (RUN14), pas la faisabilité ; recette n°21 sans playtest ; `DA_RunRules` sans valeur surchargée ; toute calibration doit partir des DA réelles (300 s, barque 11 m/s : voir 01), pas du cahier.
  - Preuve : RunSpec.cpp, DA_RunRules.uasset (13 octets, lu en binaire), cahier §12 n°21.
  - Recommandation : simulateur Monte-Carlo lisant la DA, cibles proposées (90 % M1, 50 % M5, 15 % M8), `RoundSummary` (marge vendu/quota) à la signature, puis bots chronométrant les 20 fragments.
  - Effort : M (bots : L)

## Manques
1. Table DA de difficulté par manche et par nombre de joueurs.
2. Simulateur d'équilibrage, télémétrie de marge.
3. Préréglages d'assistance, relance de manche.
4. Bots de mesure des 20 fragments.

## Quick wins
1. `monke.SetRound n` : quota et difficulté jouables sans la boucle complète.
2. Faire lire la manche à `GuardiansPerVillage` et `NightWarriorCount`, arrondi, derrière la DA.
3. `balance_run.py` : tableau quota × joueurs.
4. Limiter les drops d'armes aux 9 gardiens, avant tout « armes ×3 ».

## Questions qui challengent l'équipe
1. Quelle part des joueurs doit atteindre M5 ? Sans cible chiffrée, +10 % par manche et 0,25 par joueur sont des paris.
2. Un solo et un équipage de quatre doivent-ils échouer au même endroit de la courbe, ou la coop est-elle une récompense ?
3. Perdre en M5 sur une île de 17 ha tirée à 18 h : défaite méritée ? Sinon, quel filet ?
