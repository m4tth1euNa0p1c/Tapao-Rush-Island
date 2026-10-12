# 57 — Anti-griefing en coop
Domaine : F · Auditeur : designer coop (anti-griefing)

## Verdict
Mousquet et sabre épargnent les équipiers, pas le poing ; ni propriété du butin, ni auteur tracé, ni expulsion, ni vote codé. La mer tue sur le coup et la mort dure 8 à 11 min (28) : un farceur ruine l'expédition de trois inconnus (avis « injouable en public », supposé). Règle : tout effet entre joueurs est réversible en 10 s, attribué, et ne retire jamais un joueur ; lancer, chiper, surcharger restent libres.

## Constats

- **[Majeur] Le poing seul ignore le tir ami**
  - Constat : mousquet et sabre (`bFriendlyFire=false`) épargnent les joueurs ; le poing frappe tout pion : 20 PV, recul 3,5 m/s, 5 coups tuent, un seul brise le sermon. Tir ami activé, le mousquet tuerait en un tir (10 000 en code, 70 en fiche), le combo du sabre fait 115.
  - Preuve : IslanderCombatComponent.cpp:197-231 (aucun filtre) ; MusketWeapon.cpp:326-331 ; MusketRules.h:112,136.
  - Recommandation : `EFriendlyFireMode` (DataAsset), affiché au lobby. Off (public) : aucun effet entre joueurs, balles traversantes. Slapstick (amis) : `Knockout`, qui lâche l'objet porté, zéro PV. Réel : privé, ≤ 49 par coup, frais de recrutement non facturés.
  - Effort : S

- **[Majeur] Mer et barque : le grief tue**
  - Constat : à 0 d'endurance dans l'eau on meurt sur le coup ; 100 d'endurance donnent environ 30 m de nage (galion à 25 m, 250–400 m au cahier). Pousser hors de la barque (supposé) ou la couler (100 kg de trop : 6 s, cargaison perdue, secours en L8) revient à tuer.
  - Preuve : IslanderMovementComponent.h:66,120 ; BoatRules.h:41-61 ; cahier §24.3.
  - Recommandation : aucun recul entre joueurs à bord ou à l'eau ; naufrage en ≥ 20 s (DA) avec craquement et écopage ; barque de secours avancée de L8 à L4.
  - Effort : S (DA, recul), M (secours)

- **[Majeur] Butin sans propriétaire ni auteur**
  - Constat : la saisie partagée permet de prendre l'objet d'un équipier (contre §5), seul remède aussi contre qui le garde. Lancée à 9 m/s, la Perle (1 500 $ > quotas 1 à 3) perd 359 $ par choc (défauts), 5 chocs la vident ; la cale peut se vider à la mer (supposé) ; aucun auteur noté.
  - Preuve : IslanderGrabComponent.cpp:300,379-385 ; ValuableComponent.h:32-49 ; marae-des-perles.md:16.
  - Recommandation : garder la saisie partagée ; `LastHolder`, `LastInstigator` sur `UValuableComponent`, lus au registre (« Perle abîmée par X ») ; pas de lancer dans `AboardVolume` du galion.
  - Effort : S

- **[Majeur] Sermon sabotable par un équipier**
  - Constat : un officiant par drapeau, nul ne le relève ; tout dégât ou recul remet à zéro. Un équipier frappe, ou relâche à 19 s et recommence : jusqu'à 20 s perdues par essai (T-054 non fusionnée).
  - Preuve : PacificationZone.cpp:206-209,558-567 (`.worktrees/T-054`) ; PacificationRules.h:48-50.
  - Recommandation : l'officiant ignore dégâts et recul d'un équipier (pas ceux des tiki ni des noix) ; arrêt volontaire = même joueur bloqué 15 s.
  - Effort : S, avant fusion

- **[Majeur] Départ, signature, relance : peu décident pour tous**
  - Constat : à 3 sur 4 on abandonne le quatrième après 10 s de préavis ; le seuil compte les vivants, un meurtre l'abaisse. L'hôte seul signe un quota manqué ; l'hôte mort a un « Recommencer » qui ouvre le niveau sans vote (clients coupés, supposé). Vendre seul nuit peu (monnaie commune).
  - Preuve : cahier §24.2, §24.5 ; TapaoUISubsystem.cpp:501-504,644 ; 09, 27.
  - Recommandation : à arbitrer : pas d'abandon par vote avant la montée sauf cible inactive 45 s ; une proposition par joueur et par 45 s ; « Recommencer » confirmé et grisé tant que des équipiers vivent.
  - Effort : S, avant T-112 et T-114

- **[Bloquant commercial] Aucune modération**
  - Constat : ni expulsion, ni bannissement, ni silence ; `MaxPlayers` laissé à 16 ; aucun nom de joueur à l'écran, donc aucun vote nominatif ; l'hôte qui part clôt la session. Cahier §16 : invitation ou code seulement.
  - Preuve : 40, 08, 18 ; zéro `KickPlayer`, `PreLogin` ou `MaxPlayers` dans Source et Config.
  - Recommandation : `MaxPlayers=4` ; « Expulser » de l'hôte (`AGameSession::KickPlayer`), confirmé, motif affiché, avec bannissement de session ; vote d'expulsion dès 3 joueurs ; mute individuel, voix de proximité seule ; noms d'abord.
  - Effort : S (expulsion), M (vote)

## Manques
1. Noms de joueurs, expulsion, vote d'expulsion, mute.
2. Auteur tracé (objets, morts, naufrages) et registre.
3. Barque de secours, écopage.
4. Détection d'inactivité (rames, votes).

## Quick wins
1. `ApplyPunchHit` : ni dégâts ni recul sur un joueur, avec test.
2. Sermon : ignorer les effets d'un équipier, verrou de 15 s.
3. `Release(true)` sans impulsion dans le galion ; `LastHolder` sur `UValuableComponent`.
4. `DA_BoatRules` : couler en ≥ 20 s ; `MaxPlayers=4`.
5. « Recommencer » confirmé, grisé si des équipiers vivent.

## Questions qui challengent l'équipe
1. Parties publiques ? Sans, un bouton Expulser suffit ; avec, qui répond de l'inconnu qui noie trois joueurs ?
2. Quel budget de grief : une noyade coûte 8 à 11 min, un clic d'hôte archive la partie ?
3. Votre chaos : le cadavre d'un ami ou le bean qui lâche la perle ? Le second se clippe.
