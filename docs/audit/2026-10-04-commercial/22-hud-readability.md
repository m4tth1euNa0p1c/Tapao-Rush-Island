# 22 — Lisibilité du HUD
Domaine : C · Auditeur : designer HUD

## Verdict
Le HUD est sobre et son instinct est bon, mais il est calibré sur l'île seule du prototype : il omet des états critiques (épuisement, urgence, charge) et ignore la boucle v1.3 (quota, cale, départ, direction). Les barres de vie masquées sont justifiées ; l'horloge nue manque d'escalade. Risque commercial : le joueur ne comprend pas pourquoi il perd (noyade, naufrage, submersion), cause classique de remboursement.

## Constats

- **[Majeur] Boucle v1.3 : aucun HUD prévu (quota, cale, départ)**
  - Constat : « N $ » (valeur à bord) n'a ni libellé ni quota. L'ADR-005 ne prévoit que lobby, registre, boutique et bilan : le HUD d'expédition (quota, zones restantes, préavis de départ, cale pleine, direction du galion) n'a aucun lot.
  - Preuve : STapaoHUD.cpp:106-162, ADR-005:81, cahier §19 l.371.
  - Recommandation : lot « HUD d'expédition » ; « N $ » devient « À bord 640 / Quota 880 » (`hud_coin`, « couronnes »).
  - Effort : M

- **[Majeur] Régression TAPAO : l'épuisement a disparu**
  - Constat : `IsExhausted()` n'est lu que par les barres canvas provisoires, coupées sous TAPAO. Le sprint reste verrouillé jusqu'à 35 %, alors qu'en nage endurance 0 = noyade. Clignotement sous 11 % et torche orange sous 30 s, promis par le kit, absents.
  - Preuve : IslanderHUD.cpp:64-70, STapaoHUD.cpp:137-140, UI_TEMPLATE.md:47-48.
  - Recommandation : teinte « épuisé », clignotement sous 25 %, torche orange sous 30 s.
  - Effort : S

- **[Majeur] Jauges de 7 px, seule lecture de la santé**
  - Constat : 200 × 7 px, icône de 20 px, sans chiffre (demande propriétaire). La courbe DPI du moteur donne 4,7 px à 720p, 5,2 px sur Steam Deck (calcul).
  - Preuve : STapaoHUD.cpp:29-30,167, BaseEngine.ini:1474-1475.
  - Recommandation : 10-12 px ; jauge repliée à pleine valeur, rouverte 3 s avec son chiffre à chaque variation, pulsation sous 30 % (vignette de dégâts : 04).
  - Effort : S

- **[Majeur] Évacuation : aucune escalade, teinte la plus calme**
  - Constat : le `Presenter` n'alerte qu'avant la montée. Pendant les 60 s d'évacuation, une pulsation puis rien, et l'horloge vire au bleu clair. L'alerte sous 30 s ne tient qu'à la couleur, contre le cahier §18 (codes couleur doublés d'icônes).
  - Preuve : FloodTimerMath.cpp:101-104, SFloodTimer.cpp:82,157-160, FloodTimerWidget.cpp:169-184.
  - Recommandation : seuils 30/10/5 s en évacuation, rouge, sablier, chiffre centré à ≤ 10 s (sons : 15).
  - Effort : S

- **[Majeur] Ligne d'états : bruit pour le banal, discrète pour le grave**
  - Constat : une ligne canvas empile jour, vent, abri, posture et charge. « Jour N » change une fois au plus par manche (cycle de 18,5 min, manche de 6-12 min, calcul) ; les postures doublent l'animation du bean ; tempête et vent sont un petit texte sans direction.
  - Preuve : IslanderHUD.cpp:77-123, board T-067, DA_ExpeditionRules (300 s).
  - Recommandation : supprimer Jour et postures ; pictogramme de vent avec flèche, bandeau « Tempête ».
  - Effort : S

- **[Majeur] Quatre chemins d'affichage, trois styles de texte**
  - Constat : Slate TAPAO, widget d'horloge, canvas `AIslanderHUD` avec le hook de la barque, texte de debug du moteur (naufrage). Polices : Alegreya SC, Roboto Light, police canvas. Le canvas recopie à la main « 4 slots de 64 px, 18 px du bas ».
  - Preuve : IslanderHUD.cpp:57,221-225, STapaoHUD.cpp:109, BoatRiderComponent.cpp:122, BoatActor.cpp:945.
  - Recommandation : un seul arbre Slate (`WBP_HUD`, §21), zones d'ancrage, file de toasts.
  - Effort : M

- **[Mineur] Barres de vie des mobs masquées : bon choix, mal posé**
  - Constat : mousquet 10 000 dégâts, sabre 35 contre 60-80 PV : une barre n'aurait que 2-3 états, donc du bruit. Mais le masquage est une donnée (`MaxDistance = 0` sur 5 BP, d'après la doc), non une option ; les élites à gros PV (voir 16) en voudront.
  - Preuve : MusketRules.h:112, CutlassRules.h:78, TikiRules.h:30,94, characters-tiki.md:237.
  - Recommandation : option « Barres ennemies : jamais / élites / toujours » (défaut élites) ; widget créé à la première blessure.
  - Effort : S

## Manques
1. Anneau de sermon 0-20 s avec cause d'interruption (§19), préavis et vote de départ, cale pleine, zones restantes (voir 12, 13).
2. Marqueur galion et barque avec distance (§19) ; `compass_bar` et `minimap_frame` existent dans la skin, sans widget.
3. Charge de la barque : repère de flottaison peint sur la coque, compatible avec « pas de compteur » (voir 13).
4. File de toasts unique (zone sécurisée, valeur perdue, refus).

## Quick wins
1. Épuisement et torche : teintes et clignotement dans `STapaoHUD`.
2. Supprimer « Jour N » et les postures, icône de vent.
3. Évacuation : pulsations 30/10/5 s, rouge, sablier.
4. Jauges repliées à pleine valeur, chiffre 3 s à chaque variation.
5. Neuf captures (aucune du HUD actuel) : jour, nuit, tempête, de 1080p à 800p, contraste ≥ 4,5:1.

## Questions qui challengent l'équipe
1. Quelle est l'information vitale à chaque seconde d'une manche ? Si c'est l'horloge, pourquoi un coin, une police fine, aucun son, aucune escalade au dernier tiers ?
2. Chaque retrait (libellé, chiffres, compteur de charge, barres de vie) répond-il à une gêne observée en jeu, ou à un goût ? Un novice paie le goût au naufrage.
3. Qui possède le HUD avant que quota, cale et départ n'arrivent ?
