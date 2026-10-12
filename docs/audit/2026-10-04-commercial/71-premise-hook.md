# 71 — Prémisse et accroche
Domaine : H · Auditeur : narrative designer

## Verdict
Lisible en une ligne, pas encore mémorable : « Lethal Company en bateau » (supposé, de mémoire) tant que le sermon, la satire du Bureau et la défaite « traître » restent hors du pitch et du jeu jouable. Risque commercial : la démo investisseurs présente un survival pirate générique (supposé).

## Constats

- **[Bloquant commercial] La façade livrée contredit la prémisse**
  - Constat : le menu affiche « TAPAO : Rush Island » ; les rubans disent NAUFRAGÉ ! et ÎLE CONQUISE ! (on ne conquiert pas une île qui sombre) ; fond de plage sans galion (§16) ; kit sans sceau, tampon ni couronne. « Crownfall », « Percepteur », « traître » : 0 occurrence dans Source et Content.
  - Preuve : art/ui/tapao/screens/01, 07 ; TapaoUISubsystem.cpp:517,638 ; ADR-001 E-04.
  - Recommandation : nom arbitré par ADR, portant la Couronne ; rubans « CONTRAT REMPLI », « AVIS DE DÉCHÉANCE » ; quitter et version à la voix du Bureau.
  - Effort : M

- **[Majeur] Le pitch dit la boucle, pas l'accroche**
  - Constat : §1 décrit encore « barque » et « hub » (avant v1.3) : aucun pitch à jour. Phrase de streamer : « Lethal Company sur des îles qui coulent, où l'on gagne du temps en lisant un discours, immobile » ; seul le sermon est neuf (supposé). Haricot contre noix de coco : image déjà dessinée, la plus forte.
  - Preuve : CAHIER §1, §6.2 (un dégât interrompt le sermon) ; storm-flood.md (noix) ; sprite/player-1.png, mob-1.png.
  - Recommandation : un pitch, trois formats.
    1. Phrase : « Jusqu'à quatre haricots de la Couronne, une île qui coule, un sermon qui fait patienter la mer, un Percepteur qui prend 35 % : rapportez le quota ou soyez déclarés traîtres. »
    2. Paragraphe : « La mer obéit au Registre : une terre non proclamée est une terre submergée. Le Bureau des Possessions lointaines envoie des haricots en habit rouge. Sur chaque parcelle, un officiant lit l'acte de possession vingt secondes, immobile, tandis que l'équipage tient l'île à distance : la mer patiente deux minutes. Au retour, le Percepteur taxe, ou déclare traîtres les équipages trop légers. »
    3. Trailer : « Tampon : CONDAMNÉE. Un haricot plante un drapeau, lit, immobile. Une noix de coco lui tombe dessus. Voix : "Acte nul. Veuillez recommencer." La mer monte. Carton : "Tout doit disparaître. L'île aussi." »
  - Effort : S

- **[Majeur] La fiction ne répond à aucun « pourquoi »**
  - Constat : « condamnée » n'apparaît qu'une fois (§1), sans cause ; pas de bible ; E-03 ne règle que réel contre fictif. Pourtant les `CrownName` (« non cadastré », « en attente du sceau », « pétition en cours ») disent déjà : le Registre.
  - Preuve : docs/narrative/islands.md:13 ; ST_Islands.csv:25,57,65 ; characters-tiki.md:115-120.
  - Recommandation : un ADR, une ligne par réponse (propositions). Îles : la mer obéit au Registre (surnaturel, non climatique : Raraka culmine à 2–4 m). Sursis : le sermon proclame la parcelle ; interrompu, « vice de forme ». Tiki : les défenses de l'île (noix, carapace, cendre, ronces), qui sortent quand on touche un villageois (T-072 le fait déjà). Butin : le Bureau « recouvre » ses pertes (T-092).
  - Effort : S

- **[Majeur] Les deux moments-clips n'existent ni en texte ni dans main**
  - Constat : le sermon dort sur `dev/T-054` (specs non exécutées), sans texte ni voix ; la défaite « traître » n'est codée nulle part ; le HUD dit « Zone sécurisée : +2:00 ».
  - Preuve : board T-054, T-114 ; FloodTimerWidget.cpp:246.
  - Recommandation : six sermons de 50 mots ; interruptions en « vice de forme » (« Acte nul : officiant déplacé ») ; « Parcelle proclamée : sursis +2:00 » ; carte « AVIS DE DÉCHÉANCE » (« il manquait 40 »).
  - Effort : S

- **[Majeur] La satire écrite n'est lue par rien**
  - Constat : `ST_Islands` (70 clés, 8 îles sur 20) n'a aucun lecteur : `UExpeditionIslandDefinition` n'a que `DisplayName` comme texte de fiction, lu nulle part. Les 8 astuces du skin sont des notices (« mob »), jamais affichées (13).
  - Preuve : ExpeditionIslandDefinition.h:41-79 ; STapaoScreens.cpp:322-372.
  - Recommandation : champs `CrownName`, `Pitch`, `Rumor` affichés au chargement (20) ; astuces en circulaires du Bureau.
  - Effort : M

## Manques
1. Voix du Percepteur (20), sermons, carte de déchéance.
2. Bible d'une page ; 12 identités d'îles restantes (10).
3. Arc de partie : « Crownfall » promet une chute (19, 77).

## Quick wins
1. Avant la démo : rubans, quitter, version, « Zone sécurisée » en vocabulaire du Bureau.
2. `docs/narrative/bible.md` : règles de ton (reprendre islands.md §1) et réponses aux « pourquoi » ; §1 amendé par ADR.
3. Six sermons et la carte de déchéance, pour T-054 et T-114.

## Questions qui challengent l'équipe
1. « TAPAO : Rush Island » (UI) ou « Crownfall Expeditions » (E-04) : quel nom le joueur retient-il ? Aucun ADR ne tranche.
2. Quelle image de trois secondes vendrait le jeu, et est-elle jouable sur main ?
3. Joueur complice de la Couronne (« les méchants mal payés ») ou saboteur ? Pitch, ton et classement (72, 80) en dépendent.
