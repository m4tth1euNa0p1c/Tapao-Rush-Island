# 92 — Roadmap réaliste vers l'early access
Domaine : J · Auditeur : producteur

## Verdict
Le code avance en heures, la preuve en semaines : 85 tâches sur 126 en `review`, 0 test à deux machines, sermon, arrimage et sauvegarde hors `main`. Cibles : manches solo 2026-11-22, internet 2027-01-08, démo 2027-06-14, EA 2027-08-17 (limite 09-28). Risque : un seul testeur, un disque à 99 %, un calendrier Steam par fenêtres de 4 mois.

## Constats

- **[Majeur] Aucun jalon daté au chantier**
  - Constat : le cahier refuse une date avant « mesure des obstacles », les audits la fournissent. Hypothèses : 6 h de test par semaine, 2 allers-retours par jalon, fêtes à mi-vitesse. J1–J4 (89), périmètre (91).
  - Preuve : cahier §11 l.242, board.md (aucune échéance) ; dates : supposé.
  - Recommandation : adopter ce calendrier (cible = médiane, limite = marge incluse).

  Jalon | Cible (limite) | Preuve
  --- | --- | ---
  J0 tronc propre, disque, ADR passage commercial | 2026-10-07 (10-11) | éditeur
  S0 test 2 machines | 10-11 (10-18) | multi-machines
  M1 noyau solo | 10-18 (10-25) | éditeur
  M2 manches solo (T-111–116) | 11-22 (12-06) | build
  J1 noyau validé, porte « fun » (89, 11-30) | 12-06 (12-20) | multi-machines
  M3 L7a sessions Steam | 2027-01-08 (01-29) | multi-machines
  M4 alpha, contenu gelé | 02-19 (03-19) | multi-machines
  J2 playtest passé | 04-09 (04-26) | externe
  J3 démo soumise 05-10, Next Fest | 06-14 (fixe) | externe
  J4 EA | 08-17 (09-28) | externe

  - Effort : S

- **[Bloquant commercial] Chemin critique séquentiel**
  - Constat : ordre : fusion T-054/055/057 (~69 specs jamais lancées), zones câblées, T-111, T-112 ∥ T-113, T-114, T-115, T-116, LAN, L7a, alpha. Chaque maillon : code 1–3 j, revue réseau, INT, test humain, ~4 j (supposé) ; quatre maillons = 3 semaines après M1, +2 de recette RUN et d'aléas.
  - Preuve : board.md T-111 à T-116, `git branch --no-merged main`, 36, 39.
  - Recommandation : INT#1 d'abord, T-088 absorbé par T-113, gel du monde jusqu'à RUN10 (91) : 3 `L_Frag_*`, les 2 autres après licences et J1 ; cale, palan, Balance en blockouts jusqu'à M4.
  - Effort : L

- **[Bloquant commercial] Capacité : un testeur, un éditeur, deux PC, un disque**
  - Constat : le code n'est pas le goulot (T-109 : 12 min jusqu'à `review`), 43 des 85 `review` portent un reste à tester. Éditeur exclusif (sessions Claude, Codex) : 8 rapports de plantage en 25 h ; UBT éditeur exige l'éditeur fermé, un package prend 2 à 4 min. Aucun test à deux machines, PC2 sert Blender. C: 22 Go libres, package ≥15 Go (48, 93).
  - Preuve : journal.md:280-290, `Saved/Crashes`, `df /c`, FACTORY.md:27, 39, 58.
  - Recommandation : INT quotidien (éditeur fermé 1 h), 3 créneaux humains par semaine, imports de nuit, PIE de jour, PC2 réservé un après-midi par semaine.
  - Effort : S

- **[Majeur] L7a : ~5 semaines après J1, aucune tâche au chantier**
  - Constat : L7a 2–3 semaines + fêtes (56). L7b voix : décision à M4, livrée 2 semaines après (03-05), sinon EA +1 mois (89, 91).
  - Preuve : 56, 89, 91, board.md.
  - Recommandation : créer les tâches L7a, L7b ; spike Steam ≤2 j pendant M2 (56), exception à valider ; cercles C et D de 94 par pont VPN avant L7a.
  - Effort : L

- **[Bloquant commercial] Chaînes externes à latence inconnue**
  - Constat : accord de publier (84), entité et Steamworks (21 jours + fiscalité 10–15 j ouvrés, 97), nom « Crownfall » pris, licences DAG et DAF/Maxar, revue culturelle, musique et VFX sous-traités (6–8 et 5–6 semaines, 99) : sans date. Plan B procédural : ~3–4 semaines (supposé).
  - Preuve : 70, 72, 73, 78, 97, 99.
  - Recommandation : courriers, entité et pré-tri du nom cette semaine ; bascule au plan B le 2026-11-30 sans réponse ; six portes fermées le 2027-01-04, page publique 2027-01-18 (84, limite 02-15) ; commande musique et VFX avant M4.
  - Effort : S (lancer), M (plan B)

- **[Majeur] Ce qui fait glisser, en semaines (supposé)**
  - Constat : seamless et PlayerState à refaire +2–3, semaine sans test humain +1, plan B terrain +3–4, 60 FPS manqués +2–4, boucle à refaire (J1–J3) +2–4. Février exclu : page, démo et quatre portes avant le 10 janvier (89). J2 après 2027-04-26 : démo hors juin, EA ≥ novembre.
  - Preuve : 32, 43, 45, 54, 89, 93.
  - Recommandation : couper plutôt que décaler (91) ; go/no-go EA le 2027-06-21, seuil 7 000 souhaits (93) ; sans Next Fest, EA dès ~2027-05-07 (supposé), sans pic de souhaits.
  - Effort : S

## Manques
1. Tâches datées : L7a, localisation EN, tutoriel, spectateur, licences, entité, page Steam.

## Quick wins
1. Aujourd'hui : retirer les worktrees fusionnés (≈ −35 Go), `git bundle` hors C:.
2. INT#1 : fusionner T-054/055/057, lancer leurs specs.
3. S0 : package T-124 sur PC2, `open <ip>`, UDP 7777.
4. Courriers DAG, DAF, mairie, entité ; pré-tri du nom (2 h).

## Questions qui challengent l'équipe
1. Quel jour jouez-vous à deux machines ? Sans preuve avant 2026-10-18, qui accepte de livrer J1 ?
2. Si J2 manque le 2027-04-26, coupez-vous du périmètre pour garder juin ?
3. Si DAG et DAF refusent : atoll procédural (+3–4 semaines) ou page repoussée ? Qui est l'éditeur juridique ?
