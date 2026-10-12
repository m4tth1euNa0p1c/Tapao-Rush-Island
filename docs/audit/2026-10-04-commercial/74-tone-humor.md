# 74 — Ton et humour : satire, peur, chaos
Domaine : H · Auditeur : auteur comique

## Verdict
Sur le papier, les registres ne se heurtent que s'ils partagent un moment ou un personnage ; la boucle en manches les sépare (île : peur et chaos ; galion : satire), mais la build parle pirate-arcade (« TAPAO : Rush Island », « $ »). La satire écrite n'est affichée nulle part. Risque commercial : vendre une satire sans une réplique du Percepteur à l'écran.

## Constats
- **[Bloquant commercial] La satire n'est pas à l'écran**
  - Constat : pitchs d'îles (« un Bureau qui veut des chiffres avant le thé ») sans champ C++ ni écran ; îles 09–20 : « Fragment NN » ; huit conseils de chargement jamais lus par le code, deux obsolètes (§24.3).
  - Preuve : `ExpeditionIslandDefinition.h:47`, DA_Island_Raraka_09–20, `TapaoUISkin.h:133`, `STapaoScreens.cpp:322-372`.
  - Recommandation : afficher `Name`, `CrownName`, `Pitch` au chargement ; écrire 12 îles et ~30 répliques du Percepteur (≈ 160 lignes, supposé).
  - Effort : S/M
- **[Majeur] L'habillage parle une autre langue**
  - Constat : logo, version et dialogue de sortie disent « TAPAO : Rush Island » (E-04 : Crownfall) ; « Île conquise ! » prend la conquête au premier degré, la Couronne dit « pacifier » ; « $ » au lieu de « couronnes ».
  - Preuve : `TapaoUISubsystem.cpp:511-517,638`, `STapaoHUD.cpp:148`, `IslanderHUD.cpp:157`, ADR-001, cahier §24.4, screens/09_chargement.png.
  - Recommandation : « CONTRAT REMPLI », « ÎLE RÉGULARISÉE », « couronnes ».
  - Effort : S
- **[Majeur] Trois voix mélangées, sans charte**
  - Constat : conseils au « tu », alerte « Tu te noies ! », pitchs au « vous », HUD à l'infinitif ; aucune bible.
  - Preuve : DA_TapaoUISkin, `BoatRiderComponent.cpp:335`, `islands.md:13`.
  - Recommandation : trois voix.
    - **Bureau** (Percepteur, circulaires, frégate) : vouvoie, 3e personne, passif, litote, chiffres exacts, sans calembour ; vise Couronne et équipage, jamais insulaires, marae ni foi. « Art. 7 : l'île est informée qu'elle appartient à la Couronne. L'île n'a pas répondu. » « Recrutement : 50 couronnes. Cause : imprudence. »
    - **Bord** (HUD, dangers) : infinitifs, six mots, aucune blague pendant le danger ; la chute vient après, par le Bureau. « Noyade : regagner la barque. »
    - **Équipage** : cris aigus sans mots ; la physique fait le chaos, le texte le légende après coup. « Perle du Gouverneur : intacte. Au fond du lagon. »
  - Effort : S
- **[Majeur] Villageois à trois émotions, peur sans durée**
  - Constat : le villageois salue, peut être tué puis tranché, « TRANCHÉ ! » : ni drôle, ni effrayant, ni satirique ; contredit « jamais d'humour aux dépens des habitants ». Voix « cartoon » : aucune (bean muet ; tiki et villageois en banque libre, « monster/troll », « human_/die_ ») ; nuit de 75 s dans une boucle de 3 min.
  - Preuve : `characters-tiki.md:115-135,175`, `art/SOURCES.md:119,125`, `IslanderHUD.cpp:315`, `islands.md:40`, ADR-001 E-06.
  - Recommandation : guerriers = peur (nuit continue ≥ 90 s, silence avant le premier rugissement), Couronne = rire, beans = chaos. Villageois : panier qui tombe au lieu du cri, « Incident consigné », « TRANCHÉ ! » réservé aux guerriers ; coupe à arbitrer avec #80.
  - Effort : S/M
- **[Majeur] Les gags du chaos sont en debug ou sans effet**
  - Constat : le naufrage par surcharge s'affiche en message de debug ; la fanfare s'entend de loin sans effet sur les tiki : ni risque ni chute.
  - Preuve : `BoatActor.cpp:959`, `TrumpetEmoteRules.h:71`, aucun `MakeNoise` hors `Variant_Shooter`.
  - Recommandation : annonce « Surcharge constatée. La Couronne déplore la perte de la marchandise. » ; 8 cris de bean ; la nuit, la fanfare attire les tiki.
  - Effort : S/M
- **[Majeur] Aucune fenêtre de lecture réservée à l'humour**
  - Constat : fenêtres sans stress : chargement, sermon (immobile 20 s, sous-titres obligatoires), Balance, mort, défaite. Interdits : nuit, combat, rame, alertes d'eau. Le sermon, seul point où satire et peur se rencontrent, se joue au premier degré : texte absurde, voix solennelle, danger réel.
  - Preuve : cahier §6.2 l.110, §24.4, audit 09.
  - Recommandation : un édit par sermon ; une appréciation par objet (« Cuillère en bois : 1 couronne, classée ustensile de culte faute de mieux. ») ; tampon rouge (APPROUVÉ / TRAÎTRE) et son « thunk ».
  - Effort : M

## Manques
1. Bible de ton (`docs/narrative/bible.md` absente).
2. Percepteur : borborygme, tampon sonore, ~30 répliques ; cris du bean, tiki non verbaux.
3. 12 îles, nom du galion, édits, appréciations, palmarès.

## Quick wins
1. Afficher les huit conseils réécrits en « Circulaires » : « Circulaire n° 12 : la torche s'éteint dans l'eau. Le Bureau décline toute responsabilité. »
2. Rebaptiser : « Quitter Crownfall Expeditions ? », « CONTRAT REMPLI », « couronnes » pour « $ ».
3. Écrire `docs/narrative/ton.md` : charte et 25 appréciations d'objets.

## Questions qui challengent l'équipe
1. Le joueur est-il le héros ou l'instrument de la Couronne que vous moquez ? Où le jeu le lui dit-il quand il tranche un villageois qui l'a salué ?
2. Qui écrit, relit et traduit ≈ 160 lignes, et tranche quand un gag touche les insulaires ?
3. La peur exige du silence, le chaos coop le détruit : lequel est le plat principal ?
