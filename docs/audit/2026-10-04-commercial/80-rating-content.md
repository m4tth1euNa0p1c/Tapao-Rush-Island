# 80 — Classification par âge (PEGI / ESRB / USK / IARC)
Domaine : H · Auditeur : conseil en classification

## Verdict
Sans sang ni humain réaliste (la coupe se ferme sur du bois nu), le jeu relève plausiblement de PEGI 12, ESRB T, USK 12 : estimation, aucun organisme consulté. Risque commercial : villageois sans défense tués et découpés, ou drapeau PEGI « Discrimination » (18 automatique), feraient passer à 16 voire 18. Aucun dossier de classification n'existe (grep `docs/` hors audit : zéro occurrence).

## Constats
- **[Majeur] Cible : PEGI 12, ESRB T, USK 12 ; plafond de risque 16/M/16**
  - Constat : sabre, mousquet et découpe écartent PEGI 7 et ESRB E10+ (supposé). PEGI 12 tolère la violence envers « personnages fantastiques » ou non réaliste envers humanoïdes ; ESRB « Violence » admet « un démembrement sans sang » ; USK « Fantasy-Gewalt » exige un cadre irréel. Requin, noyade et nuit restent sans gore (ragdoll de bean, tirs alliés désactivés) ; la peur ne vaut PEGI 16 que si « intense et soutenue ».
  - Preuve : https://pegi.info/what-do-the-labels-mean ; https://www.esrb.org/ratings-guide/ ; https://usk.de/en/?p=63389896 (résumé) ; `CutlassRules.h:157`, `shark.md:14-16`
  - Recommandation : viser 12/T/12 (E10+ exigerait de supprimer la découpe) ; valider par le Content Survey Steam avant tout trailer.
  - Effort : S

- **[Majeur] Zéro sang : à fixer en règle de DA**
  - Constat : aucun code ni nom d'asset de sang (grep) ; la coupe est fermée par `MI_Minka_Timber`, bois nu. ESRB « Blood and Gore » inclut la « mutilation » : un bras tranché sur un être qui crie peut compter (supposé), et le HUD célèbre la coupe (« TRANCHÉ ! »).
  - Preuve : `TikiRules.h:213-219`, `DA_TikiRules.uasset`, `IslanderHUD.cpp:315-317`
  - Recommandation : « sève » ambre ou verte, copeaux, feuilles ; coupe en bois à cernes ; interdire le rouge organique ; HUD neutre.
  - Effort : S

- **[Majeur] Villageois sans défense tuables, coupables, brûlables**
  - Constat : le villageois (0,95 m, effrayé, panier) meurt au sabre, mousquet ou torche, perd un bras, crie d'une voix humaine. PEGI 18 vise la « violence envers des personnages sans défense » ; PEGI/USK 16 en jeu (supposé). L'alarme part déjà au premier coup : la mort est superflue.
  - Preuve : `TikiVillager.cpp:395-427`, `TikiCreature.cpp:443`, `TorchItem.cpp:283-299`, `art/SOURCES.md:125`
  - Recommandation : villageois non létaux (sonné 20 s, alarme conservée), sans coupe ni brûlure, cris non humains.
  - Effort : S

- **[Majeur] Leviers : ce qui baisse la classe, ou non**
  - Constat : « sève » : acquise. Villageois non létaux : sécurise 12/T/12. Option « violence réduite » : confort, mais les organismes notent le contenu accessible, donc pas de baisse officielle (supposé). Seule la suppression de la découpe ouvre E10+ (supposé).
  - Preuve : aucune option de violence dans `Settings/` ni au cahier §18 ; `TikiRules.h:163,203`
  - Recommandation : « Violence réduite » (`LimbCutChance` 0, cadavres en feuilles) pour le confort ; trancher les villageois avant Steam.
  - Effort : S (villageois), M (option)

- **[Majeur] Satire coloniale : « Discrimination » impose PEGI 18**
  - Constat : PEGI définit « Discrimination » comme des stéréotypes ethniques, religieux ou nationaux « susceptibles d'encourager la haine », et impose 18. Un examinateur lit l'écran, pas le cahier : colons britanniques, sermon (prévu), « tiki » monstres, villageois tués. Risque faible, impact maximal (supposé) ; fond en 72.
  - Preuve : https://pegi.info/what-do-the-labels-mean ; cahier §6.2:108-112
  - Recommandation : page « la Couronne est la cible » au dossier ; aucun marqueur ethnique réel (tatouage, masque).
  - Effort : S

- **[Majeur] Steam : questionnaire obligatoire, Allemagne, IARC non confirmé**
  - Constat : le Content Survey précède la revue de page ; « General » génère des classements régionaux, « Mature » règle la visibilité. Depuis le 15 nov. 2024, la vente en Allemagne exige un classement USK ou Valve. L'extrait Steamworks lu ne cite pas l'IARC (questionnaire unique d'autres boutiques ; équivalence supposée).
  - Preuve : https://partner.steamgames.com/doc/gettingstarted/contentsurvey ; https://www.gamingonlinux.com/2024/10/from-november-15-all-steam-games-sold-in-germany-will-need-an-age-rating/ (résumé)
  - Recommandation : répondre au pire cas jouable (violence, démembrement, voix : oui ; sang : non) avec vidéo témoin des scènes dures.
  - Effort : S

- **[Majeur] Voix (L7) : « Users Interact » sans modération**
  - Constat : ESRB « Users Interact » = contenu utilisateur non filtré, communications comprises ; PEGI liste « Unrestricted communication » (définition non lue). Push-to-talk et mute prévus au cahier, rien de codé.
  - Preuve : https://www.esrb.org/ratings-guide/ ; https://pegi.info/page/pegi-age-ratings ; cahier §8:185, §18:353
  - Recommandation : voix réservée à l'équipage invité, push-to-talk par défaut, blocage persistant, « signaler », interrupteur global.
  - Effort : M

## Manques
1. Fiche de classification (contenus, réponses, cible, lignes rouges).
2. Option « Violence réduite » et avertissement de satire.
3. Modération vocale L7.

## Quick wins
1. Villageois non létaux et sans coupe (`OnDamaged` garde l'alarme).
2. Règle « zéro rouge organique » ; HUD de coupe neutre.
3. Pré-remplir les réponses du Content Survey dans la fiche, avant trailer et page.

## Questions qui challengent l'équipe
1. Tuer un villageois : plaisir voulu ou piège de satire ? Sinon, pourquoi risquer 16 ?
2. Visez-vous les 13 ans ou les 16 ans et plus : la découpe est-elle le « clip » du jeu ?
3. Qui répondra des abus vocaux d'un inconnu en partie publique (L7) ?
