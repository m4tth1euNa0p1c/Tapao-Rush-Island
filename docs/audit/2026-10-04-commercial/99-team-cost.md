# 99 — Compétences manquantes, budget et sous-traitance
Domaine : J · Auditeur : directeur de studio

## Verdict
Le studio produit du code vérifiable (≈ 300 tests) mais n'a aucun titulaire pour ce qu'un agent ne peut ni entendre, ni juger, ni signer. Une early access crédible coûte ≈ 17 k€ (plancher), 28 k€ (recommandé), 50 k€ (confort). Risque principal : 28 k€ de dépenses pour un plancher de 5 à 8 k€ en semaine 1 (90), sous un mandat qui exclut la vente.

## Constats

- **[Bloquant commercial] Le mandat exclut la vente et soumet la dépense à accord**
  - Constat : le cahier exclut la publication commerciale ; ni budget ni prestataire au backlog ; aucune entité vendeuse (97).
  - Preuve : CAHIER:28, :291 ; ADR-001:51 ; backlog.md : 0 « € », 0 « budget ».
  - Recommandation : ADR « passage commercial » signé (enveloppe en trois tranches ; éditeur : micro-entreprise ou SAS, 97), avenant au cahier §1.
  - Effort : S

- **[Bloquant commercial] Neuf métiers sans titulaire : goût, responsabilité, présence**
  - Constat : aucun des 29 rôles n'est DA, compositeur, comédien, juriste ni community manager ; le VFX n'est qu'une ligne de `ue-tech-artist`. Goût (DA, musique, voix, VFX), responsabilité (avocat, conseillers polynésiens, commune), présence (QA multi-machines, playtests, Discord, marketing) : hors d'un agent.
  - Preuve : `.claude/agents` (29 fichiers, 0 occurrence community, marketing, juridique) ; ue-tech-artist.md:12 ; rapports 61, 65, 66, 72.
  - Recommandation : garder le vérifiable (C++, tests, pipelines, modèles procéduraux) ; sous-traiter le reste au forfait : brief écrit par l'usine, cession écrite des droits (CPI L131-3, de mémoire), clause « aucune IA générative ». Communauté : propriétaire 4–6 h/sem, modérateurs bénévoles, 0 € de publicité (seuil EA : 7 000 souhaits, 90).
  - Effort : M

- **[Majeur] Budget : 17 / 28 / 50 k€, versé par preuves**
  - Constat : k€ (recommandé) : musique 10–12 min 3–8 (5) ; voix (Percepteur) et sound design 2–6 (3,5) ; VFX héros (8–30 j) 2,5–12 (4) ; DA, key art, logo 2,5–6 (4) ; trailer 0,8–3 (1,5) ; avocat, comptable, marque, CGU 2–6 (3) ; 2 conseillers culturels 1–3 (2) ; revue réseau externe et testeurs 2,2–4,4 (3,3) ; matériel et relecture EN 0,8–2 (1,3) ; Steam Direct 100 $, recouvrable à 1 000 $. Hors total : abonnements IA, temps du propriétaire, localisation (79). La semaine 1 rapporte 5 à 8 k€ nets à 7 000 souhaits (90) : 28 k€ valent 3,5 à 5,6 fois ce plancher.
  - Preuve : résumés de recherche (non relus) : musique 200–500 $/min https://ninichimusic.com/blog/understanding-how-much-an-indie-game-music-composer-costs ; comédien 413 $ les 2 h (SAG indie) https://twinfinite.net/news/new-sag-aftra-agreement-opens-door-for-use-of-unionized-voice-actors-in-indie-games/ ; VFX Niagara 320–400 €/j https://www.freelancermap.de/projekt/unreal-engine-developer-vfx ; Steam https://partner.steamgames.com/doc/gettingstarted/appfee ; avocat 150–450 €/h https://www.plateya.fr/blog/detail/cout-avocat-propriete-intellectuelle-et-marques-tarifs-2026-et-guide-complet-pla. Supposé : trailer, DA et key art, culture, testeurs, matériel.
  - Recommandation : A (≈ 2,5 k€, avant la porte « fun » du 2026-12-06, 92) : AppID, PC plancher, nom (78), premiers testeurs. B (≈ 11 k€) : DA et key art, avocat, culture, revue réseau, testeurs. C (≈ 14 k€, après l'alpha du 2027-02-19) : musique, voix, VFX, trailer 75 s, EN.
  - Effort : M

- **[Majeur] Vérification humaine : les agents se relisent entre eux**
  - Constat : 43 des 83 `review` attendent un test humain, aucun test à deux machines (92) ; les relecteurs de code et de réseau sont des agents Claude, face à 21 RPC serveur sans `_Validate`.
  - Preuve : board.md (83 review, 22 fait) ; FACTORY.md:17 ; rapports 40, 92.
  - Recommandation : 80 h de testeur payé (15–30 €/h supposé = 1,2–2,4 k€) = 13 semaines de test du propriétaire (6 h/sem, 92) ; PC plancher (GTX 1660, 8 Go, 82) ; 2–3 j de développeur UE externe (≈ 42 $/h en Europe, https://lemon.io/rate-calculator/unreal-engine-developers/) pour relire la réplication.
  - Effort : M

- **[Majeur] Dépendance IA : coût inconnu, fournisseur quasi unique, droits non prouvés**
  - Constat : 29 rôles et 35 skills vivent dans `.claude/` ; Codex (6 profils) : plan B partiel. Coût mensuel non suivi (supposé 2,4–6 k€/an). Hotfix 72 h (89) : agents requis. Planches IA (base du bean et des tiki) sans registre, déclaration Steam exigée, CGU des fournisseurs non vérifiées.
  - Preuve : codex.md:99-113 ; CAHIER:214 ; docs/ : aucune ligne de coût d'usage (grep) ; rapports 69, 70, 89, 97.
  - Recommandation : relever la facture, plafonner ; `PROVENANCE.md` (70) ; ni musique, ni voix, ni 3D générative dans le livré ; développeur externe : build et tests en 1 j depuis le README, puis astreinte 8 h/mois (≈ 0,3–0,65 k€) ; CGU relues par l'avocat.
  - Effort : M

## Manques
1. Éditeur juridique, enveloppe signée.
2. Compositeur, comédien, sound designer, DA garant, avocat PI, conseillers polynésiens.
3. Testeurs payés, PC plancher.

## Quick wins
1. ADR « passage commercial », tranches A/B/C (2 h).
2. Briefs par l'usine, trois devis par poste : musique (6–8 sem.) et VFX (5–6 sem.) bornent l'EA.
3. Facture IA du mois consignée (1 h).

## Questions qui challengent l'équipe
1. Les investisseurs de T-120 financent-ils les ≈ 13,5 k€ des tranches A et B, alors que la semaine 1 plancher rapporte 5 à 8 k€ ?
2. Combien d'heures par semaine pour tester et diriger dix prestataires ?
3. Si l'abonnement IA saute le jour d'un crash, quel humain a lu ce code ?
