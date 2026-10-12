# 04 — Combat contre les tiki
Domaine : A · Auditeur : designer combat

## Verdict
Fondations solides (serveur autoritaire, télégraphe, combo, parade, découpe), mais le 1 contre 1 est résolu d'avance, la défense est passive et les groupes arrivent sans chorégraphie. Risque commercial : un combat spectaculaire sans courbe de maîtrise, qui lasse avant la manche 5. Bestiaire, directeur, armes et butin : voir 16, 15, 17, 05, non répétés ici.

## Constats
- **[Majeur] Le 1 contre 1 est résolu d'avance**
  - Constat : tout coup de sabre (≥ 35) fait trébucher le guerrier 1,33 s et annule son coup ; le combo (0,44–0,54 s par coup) le tue en ≈ 1,3 s sans qu'il rejoue (calcul). Il s'immobilise à 1,6 m pour frapper, hors de portée du sabre (≈ 1,15 m) mais 0,45 s avant l'impact : un pas en avant suffit à frapper le premier.
  - Preuve : TikiCreature.cpp:431 ; TikiWarrior.cpp:98,165,216 ; MobAnimManifest.json (Stagger 1,33 s) ; CutlassPoses.inl:7-43
  - Recommandation : stagger par seuil cumulé, super-armure du lourd, fente qui avance, riposte du guerrier après un coup paré.
  - Effort : M
- **[Majeur] Défense passive, endurance à tout faire**
  - Constat : la parade se maintient sans fenêtre de timing ; face à un seul guerrier, à l'arrêt, elle tient indéfiniment (régénération 16 > coût 11 entre deux coups, calcul). Aucune esquive (grep). L'endurance paie coups (9 par barre), parades, sprint et nage : épuisé en nageant, on meurt aussitôt, donc fuir à la nage après un combat est risqué.
  - Preuve : CutlassWeapon.cpp:498-515 ; CutlassRules.h:113 ; IslanderMovementComponent.h:82,86,120
  - Recommandation : parade active de 0,25 s qui étourdit le guerrier, esquive à 20 % d'endurance, plancher d'endurance en nage.
  - Effort : M
- **[Majeur] Le sabre frappe les innocents**
  - Constat : l'arc de 110° touche tout pion non joueur ; un villageois à portée de lame déclenche l'alarme au moindre coup perdu (4 guerriers). Le mousquet de même. Sans avertissement ni tolérance, se battre au village, où se tient le sermon, expose à l'alarme involontaire (supposé : villageois à portée).
  - Preuve : CutlassWeapon.cpp:407-421 ; TikiVillager.cpp:395-406
  - Recommandation : PNJ non hostiles ignorés par l'arc sauf visée volontaire ; alarme au deuxième coup.
  - Effort : S
- **[Majeur] Groupes sans chorégraphie**
  - Constat : un meurtre fait converger jusqu'à 12 poursuivants (9 défenseurs + 3 gardiens, calcul), dont 3 sur un anneau de 8 m autour du tueur, chacun avec son minuteur de 1,6 s : volées possibles de 15 dégâts dans le dos, que parade et arc ne couvrent pas.
  - Preuve : TikiRules.h:392,406 ; TikiDirector.cpp:529-541 ; TikiWarrior.cpp:165
  - Recommandation : jetons d'attaque (2 coups simultanés par joueur), cercle à 2 m, décalage aléatoire des windups.
  - Effort : S
- **[Majeur] Télégraphe fragile en réseau**
  - Constat : le départ du coup est un multicast non fiable ; le dégât tombe à 0,45 s côté serveur (écart avec les clips : voir 16). À 150 ms d'aller-retour il reste ≈ 0,3 s pour réagir (calcul), au bord du temps de réaction humain.
  - Preuve : TikiCreature.h:381 ; TikiWarrior.cpp:89-96 ; TikiRules.h:118
  - Recommandation : RPC fiable avec temps serveur, windup de 0,6 s, cône au sol pour le lourd ; recette à 150 ms.
  - Effort : S
- **[Majeur] Pas de retour de touche**
  - Constat : barres de vie masquées (demande du propriétaire) ; seul le sabre affiche des chiffres, mousquet et poing ne confirment rien. Le joueur touché n'a ni secousse, ni flash, ni direction, ni cri (grep), seulement recul, animation de corps et chapeau qui tombe.
  - Preuve : characters-tiki.md:237 ; IslanderHUD.cpp:299 ; MusketWeapon.cpp:330
  - Recommandation : tick et son de touché pour toutes les armes ; vignette, secousse et cri à l'impact reçu ; hitstop de 60 ms sur coup lourd.
  - Effort : S
- **[Mineur] Découpe et cadavres sans fonction**
  - Constat : corps de 40 kg sans valeur, 80 corps physiques au plus ; le seul effet de la découpe est −50 % de dégâts d'un guerrier manchot. Aucun code d'économie, de quête ni de danger ne lit un cadavre (grep).
  - Preuve : TikiRules.h:166,178,198 ; grep Economy, Run, Expedition : 0
  - Recommandation : cadavre comme appât (requin, voir 16), offrande ou barricade ; sinon retrait après 30 s.
  - Effort : M

## Manques
1. Matchups arme-ennemi (feu contre bois, tir à la tête, bouclier à briser).
2. Coups de zone télégraphiés au sol (onde de choc du Crabe, déjà animée).
3. Désengagement légitime : feinte, fumigène, torche qui effraie.

## Quick wins
1. Arc du sabre : ignorer les PNJ non hostiles.
2. Tick et son de touché pour mousquet et poing ; cri du joueur touché.
3. Seuil de stagger cumulé (60 dégâts) : le combo ne verrouille plus.
4. Jeton d'attaque : 2 attaquants simultanés au plus par joueur.
5. Attack en multicast fiable.

## Questions qui challengent l'équipe
1. Que fait un expert que le novice ne fait pas contre un guerrier ? Si c'est « frapper le premier », où est la maîtrise à 50 h ?
2. Cherchez-vous la puissance (trancher) ou la tension (survivre à la foule) ? Le sabre donne l'une, la foule n'apporte pas l'autre.
3. Un coup de sabre perdu sur un villageois doit-il vraiment coûter quatre guerriers ?
