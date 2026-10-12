# 67 — Ressenti des animations
Domaine : G · Auditeur : animateur gameplay

## Verdict
Les moteurs de chaos existent (ragdoll, chapeau, tiki tranchés) et le sabre est synchronisé, mais le « ragdoll actif façon Fall Guys » du cahier n'est pas fait (T-027) et personne n'a vu le reste à l'œil (T-077/078 « NON vérifié », T-080 « NON testé »). Risque commercial : un comique physique promis, avec un bean qui pédale, se téléporte debout et rame hors phase.

## Constats
- **[Majeur] « Ragdoll actif » : un ragdoll passif du mannequin adulte**
  - Constat : aucun `UPhysicalAnimationComponent` ; en chute, le pantin recopie le mannequin caché de 1,80 m sur un bean de 1,05 m ; chaque machine simule le sien (voir 34, 52).
  - Preuve : BeanPuppetComponent.h:52 (« Stopgap… T-027 ») ; board.md:33-34 « a-faire » ; grep PhysicalAnimation : 0.
  - Recommandation : assumer le passif (R.E.P.O.) ou faire T-027 avant Steam ; sinon retirer « Fall Guys » du pitch.
  - Effort : L

- **[Majeur] Relevé = téléportation debout**
  - Constat : après 2,5–8 s au sol, simulation coupée et capsule téléportée : couché puis debout en une image, à chaque noix, culbute ou choc. Aucun clip de relevé parmi les 41.
  - Preuve : IslanderCharacter.cpp:607-648 ; character-bean-soldier.md:94.
  - Recommandation : fondre 0,4 s la pose du pantin vers Idle, pop d'échelle et son ; clip GetUp ensuite.
  - Effort : S (fondu) / M (clip)

- **[Majeur] Foulée ×3 et pas sonores décorrélés**
  - Constat : Run authoré à 112 cm/s (4 appuis/s) joué à ×3,1 au jog (350 cm/s) : ≈ 12 appuis/s (calcul) contre 2,6 pas sonores/s ; au sprint, plafond ×3,2 : 40 % de glissement. Tiki : clips pour 28–100 cm/s, jeu à 110–400.
  - Preuve : BeanAnimLibrary.h:203-222 ; FootstepRules.h:118-122 ; characters-tiki.md:179.
  - Recommandation : pas déclenchés par la phase du clip ; cartoon assumé ou foulée allongée.
  - Effort : S (sons) / M (foulée)

- **[Majeur] Rame : trois horloges**
  - Constat : poussée sur 1,2 s, clip rameur 1,583 s, courbe des rames 39 images = 1,625 s : un quart de cycle de dérive en ≈ 15 s (calcul). La pose « rame » suit la vitesse de la barque, pas la touche : ≈ 10 s de rame après avoir lâché (calcul). Animation la plus vue de son bean.
  - Preuve : BoatActor.cpp:630-663 ; BeanAnimDriverComponent.cpp:575-581 ; BoatRules.h:130.
  - Recommandation : `StrokePeriod` 1,583 s, phase à 0 à l'appui, modulo 38, `RowAlpha` sur l'entrée.
  - Effort : S

- **[Majeur] Nage, accroupi, lancer, échelle : aucun clip**
  - Constat : en nage le mannequin reprend le corps (marche du template, T-017 à faire) : trajet entre îles et scène du requin. L'accroupi est un écrasement d'échelle. Nage = minimum du cahier §10.
  - Preuve : BeanAnimDriverComponent.cpp:476 ; board.md:24.
  - Recommandation : commander Swim, CrouchWalk, Throw, Ladder, GetUp à la maquette (2e PC).
  - Effort : M

- **[Majeur] Clips jamais joués, 240 notifies ignorés**
  - Constat : Interact, EatCoconut, Wave, Celebrate n'ont aucun appelant ; boire une noix n'a aucune animation ; Death est écrasé en 0,2 s par le ragdoll (calcul). 0 `AnimNotify` dans le module pour 240 décrits : chaque action est du C++ par touche.
  - Preuve : BeanAnimDriverComponent.cpp:220,267,476 ; import_bean_library.py:7.
  - Recommandation : `UBeanActionComponent` générique (modèle Trumpet, ActionId répliqué, notifies en événements) : emotes, sermon, palan, vente.
  - Effort : M

- **[Mineur] Bras et portage : permutations sèches**
  - Constat : en 1re personne, bras du corps (mains libres, sabre, torche) et bras statiques (mousquet) permutent sans fondu. Supposé : les bras de portage visent l'objet et écrasent les clips Carry ; `Pickup` en corps entier fige les jambes ≈ 1 s en courant.
  - Preuve : BeanAnimDriverComponent.cpp:267,431-452,524-539.
  - Recommandation : fondu de 0,15 s ; Pickup en couche haute.
  - Effort : S

- **[Mineur] Tiki : fondus corrects, village pauvre**
  - Constat : fondu natif correct ; mais Pray joue le panier, Rest l'Idle, Read et Watch rien ; chaque fondu repart de t = 0.
  - Preuve : TikiCreature.cpp:329-339 ; TikiVillager.cpp:72-79 ; TikiAnimInstance.cpp:33.
  - Recommandation : clips Kneel / Sit / Read ; phases synchronisées.
  - Effort : M

## Manques
1. Relevé, nage, accroupi, lancer, échelle, noyade.
2. Montages du cahier : planter, sermon, palan, vente au Percepteur, barre.
3. Roue d'emotes (Wave, Celebrate livrés), 3e personne forcée (voir 18).
4. Expressions : bean sans bouche ; peur et douleur par un paramètre de matériau des yeux.

## Quick wins
1. Filmer 60 s (course, sprint, rame, relevé, sabre, tiki en fuite) et arbitrer `MaxPlayRate`, `StrokePeriod`.
2. Rame : période 1,583 s, phase à 0 à l'appui, modulo 38.
3. Pas déclenchés depuis `GroundPhase`.
4. Relevé : fondu de pose 0,4 s, pop d'échelle, son.
5. Brancher Wave, Celebrate, EatCoconut sur le schéma Trumpet.

## Questions qui challengent l'équipe
1. « Fall Guys » ou « R.E.P.O. » ? Le cahier promet l'un, le code livre l'autre : T-027 avant Steam, ou on retire la promesse ?
2. Pourquoi 41 clips et cinq sources de poses avant qu'un humain ait regardé 60 s de vidéo ?
3. En 1re personne (ADR-002 P-03), le joueur ne voit pas son jeu d'acteur : comment rend-on les gags clipables ?
