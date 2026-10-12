# Îles d'expédition : noms et identités des fragments

| Champ | Valeur |
|---|---|
| Tâche | T-092 (feature `fragments`), reprise du 2026-10-10 |
| Lots | L3 : `FRG_Raraka_01` (carte dédiée) et les 19 fragments joués dans `Lvl_Raraka` · L4 : carte d'arrivée (T-172), écran de chargement (§16), tableau de contrats · L8 : îles variées |
| Recette | UI12 (texte maximal sans troncature, FR et EN) · §12 n° 9 (deuxième expédition : les joueurs savent qu'ils ont changé d'île) · RUN01 (arrivée au mouillage du fragment tiré) · §16 (écran de chargement : destination) |
| Décisions | ADR-001 E-03 (aucun lieu, village ou habitant réel) · ADR-004 (fragments séparés par la mer, Motutapu exclu) · ADR-007 (carte d'arrivée) · ADR-008 (MVP : îles héros, anglais D6) · T-139 en attente (« tiki », « Raraka », marae pillables) |
| Specs liées | `docs/features/fragments-tuamotu.md`, `villages-japonais.md`, `marae-des-perles.md`, `campagne-dix-jours.md` ; bible `docs/narrative/BIBLE.md` |
| String Table | `/Game/MonkeIsland/UI/Text/ST_Islands` (asset existant, référencé par les DA d'île) ; CSV `docs/narrative/ST_Islands.csv`, **généré depuis ce document** (section 4) |
| Statut | textes écrits ; CSV généré et contrôlé hors éditeur par `scripts/editor/island_names_ue.py` ; asset et DA **non mis à jour** : action éditeur `islandnames` à lancer par le lead |

Ce document est le registre des noms de lieux ; la bible y renvoie. Les tableaux « Clé | FR | EN » sont la **source** des textes : on corrige ici, puis on régénère le CSV (section 4).

## Reprise du 2026-10-10 : ce qui change
- **Vingt identités** au lieu de huit. Les huit premières avaient été écrites sur des profils attendus, avant le gel du catalogue (T-082). Le catalogue est gelé : chaque identité va désormais au fragment dont le terrain lui correspond.

  | Identité (1re version) | Écrite pour | Attribuée à | Motif |
  |---|---|---|---|
  | Les Trois Fumées | FRG_01 | FRG_01 | inchangé |
  | Les Pas-sur-l'Eau | FRG_03 | FRG_02 | FRG_02 est l'archipel aux îlots reliés à gué |
  | La Longue Échine | FRG_02 | FRG_11 | FRG_11 est un motu long, étroit et presque droit |
  | Le Poing Fermé | FRG_04 | FRG_18 | FRG_18 est l'île la plus courte, en lobes ronds |
  | Le Miroir | FRG_05 | FRG_09 | dix criques côté lagon |
  | Les Fourrés Qui Écoutent | FRG_07 | FRG_16 | fourré et cocoteraie serrés (40 % et 43 %) |
  | Le Dos de la Houle, renommé **Le Ressac** | FRG_06 | FRG_17 | arrivée côté large, long platier ; « Dos » doublait l'image d'« Échine » |
  | L'Anneau Brisé, renommé **La Lune Couchée** | FRG_08 | FRG_20 | la plus grande île ; « l'Anneau » est le nom que la bible propose pour l'archipel |

- **Deux champs ajoutés** par île : `Epithet` (sous-titre factuel) et `Tagline` (phrase d'ambiance).
- **Clés `Marae` retirées** : chaque fragment pose désormais 12 marae (`DA_MaraeRules.CountPerFragment`, T-144). Un nom par fragment ne désigne plus un lieu (section 1.5).
- **Villages** : les noms suivent ceux du générateur (`fragment_villages.py`), sauf 27 noms contraires à la règle 1.4 (pin, cèdre, cerisier, Fuji, tortue ou requin comme vivres…). Correspondance en section 2.
- `Island.Theme.Food` devient « Nourriture et abri », comme le cahier §6.2.
- **Écart à la consigne de la vague** (« nom polynésien plausible mais inventé ») : non retenu, voir 1.1.

## 1. Règles de nommage

### 1.1 Principe : deux noms par île, aucun mot inventé « à la polynésienne »
Chaque fragment porte deux noms, qui racontent la satire à eux seuls.

1. **Nom affiché** (`Name`) : le nom que lui donnent ses habitants, **traduit en français** sous forme d'image simple (« Le Peigne », « Les Pas-sur-l'Eau »). C'est le nom principal : carte d'arrivée, chargement, bilan, contrat.
2. **Désignation de la Couronne** (`CrownName`) : le nom administratif collé par le Bureau sans demander l'avis des insulaires. Numéros, parenthèses, procédures en cours. Il s'affiche en petit, comme une étiquette de registre.

Pourquoi pas un nom « polynésien plausible mais inventé » :
- **Invérifiable.** Un mot fabriqué pour « sonner pa'umotu » peut coïncider avec un vrai mot : nom de motu ou de lignée, mot sacré, grossièreté. Personne dans l'équipe ne peut le vérifier sans un locuteur. La bible (T-137) range ce cas dans « À éviter » : « mot pa'umotu ou tahitien inventé ou pris pour nom propre ».
- **Cliché.** Un nom exotique fabriqué est la forme la plus courante de l'exotisme que le projet s'interdit (rôle narratif, cahier §1).
- **Robuste.** Une image traduite ne peut pas coïncider avec un toponyme des Tuamotu, se traduit (EN : « The Comb ») et ne dépend ni de « tiki » ni de « Raraka » (T-139).
- **Cohérent.** La fiction est déjà un archipel imaginaire : villages japonais, gens-cocos, Couronne d'opérette.

Si le propriétaire veut malgré tout des noms à consonance polynésienne, il faut une relecture par un locuteur pa'umotu ou par l'Académie tahitienne avant tout affichage. Aucune liste de candidats n'est fournie ici, pour qu'aucun mot non vérifié ne soit intégré par erreur.

### 1.2 Nom affiché (`Name`)
- Une image concrète de l'île vue par ceux qui y vivent : forme, eau, végétation, lumière. Article défini, 1 à 4 mots, **28 caractères au plus**.
- Interdits :
  - tout toponyme réel : Raraka, Motutapu, passe Manureva, noms d'atolls ou de motu des Tuamotu, noms de marae réels ;
  - les anciens noms européens des atolls, eux aussi réels : île de la Harpe et Bow Island (Hao), Chain Island (Anaa), île des Lanciers (Akiaki), les Quatre Facardins et Lagoon Island (Vahitahi), Crescent Island (Temoe, Gambier), archipel Dangereux, îles Basses, Pernicieuses, Désappointement, noms russes de Bellingshausen (Lazarev, Raevski…) ; donc pas de « Harpe », « Arc », « Chaîne », « Croissant », « Lagon » comme nom ;
  - tout mot pa'umotu, tahitien ou hawaïen pris comme nom propre, les noms de divinités, le vocabulaire du sacré (tapu, atua, mana…), les légendes réelles (anguille de Hina, hameçon de Maui) ;
  - les clichés de carte postale (« paradis », « perle du Pacifique ») ;
  - « l'Anneau », réservé par la bible à l'archipel (décision 4 en attente).
- Deux îles ne partagent ni mot principal ni image.
- `Raraka` ne figure que dans l'identifiant technique `FRG_Raraka_NN`, **jamais** affiché.
- Vérification (2026-10-10) : Wikipédia [Tuamotu](https://fr.wikipedia.org/wiki/Tuamotu) (anciens noms : archipel Dangereux, îles Basses, San Pablo, Quatre Facardins, île des Lanciers, île de la Harpe), [Raraka](https://en.wikipedia.org/wiki/Raraka), [Tuanake](https://en.wikipedia.org/wiki/Tuanake) (noms de Bellingshausen). Aucun des vingt noms ne correspond à un lieu relevé.

### 1.3 Désignation de la Couronne (`CrownName`)
- Registre administratif, ton du Bureau (bible) : « Possession n° 1 », « Lagon réglementaire n° 9 ». **60 caractères au plus.** Le numéro, quand il y en a un, est celui du fragment.
- Une parenthèse comique au plus, qui moque la bureaucratie : procédure en cours, sceau attendu, nom contesté, frais.
- Officiels **fictifs** et récurrents : le Vice-Contrôleur Pembury, Lord Pettifer, l'amiral Fenwick. Institutions inventées : le Bureau des Possessions lointaines, le Lord Cartographe, le Trésor, l'Amirauté. Jamais une personne réelle, historique ou vivante, ni un explorateur réel.
- Jamais d'humour aux dépens des habitants, de leur langue ou de leur foi : le ridicule vient de la Couronne qui nomme, mesure et taxe.

### 1.4 Villages (trois par fragment, un par zone de pacification)
- Forme : `<mot japonais>-mura`, romaji Hepburn sans macron, **16 caractères au plus**. Invariable dans toutes les langues.
- Le thème suit le rôle de la zone (cahier §6.2) : richesses (et crique d'extraction), bois (et passage court), nourriture (et abri). Le générateur attribue les thèmes par fragment (`index.json`) ; la clé `Village.Riches`, `Village.Wood` ou `Village.Food` suit ce thème, quel que soit l'identifiant `Fxx_Z1..Z3`.
- Le mot doit être plausible sur un atoll : pas d'arbre ni de plante des pays tempérés (pin, cèdre, cyprès, cerisier, érable, saule, bouleau, chêne…), pas de neige, de montagne ni de rizière. Pour les vivres, ni tortue ni requin (espèces protégées et chargées de sens en Polynésie).
- Pas de nom de lieu ou de marque célèbre (Fuji, Asahi, Minato), pas de référence culturelle connue (Konoha) ni de mot au double sens fâcheux.
- Le mot dit de préférence le thème : trésor, perle, pièce pour les richesses ; plante d'atoll, bois flotté, radeau pour le bois ; poisson, sel, puits pour la nourriture. Les noms neutres du générateur (lumière, étoile, lune, brume, cap) sont gardés quand ils respectent le reste.
- Chaque nom est **unique dans le catalogue**, hameaux compris (`-sato`, non affichés aujourd'hui). Les villages de `Lvl_Raraka` près de Motutapu (Takara-mura, Mori-mura, Kura-mura) ne sont pas réutilisés.

### 1.5 Marae
- Chaque fragment pose 12 marae (T-144), en plus du temple reliquaire. On ne nomme **ni** l'ensemble **ni** chaque marae : inventer un nom propre de marae, c'est inventer un nom sacré.
- Le mot « marae » reste un nom commun. Le marae est un lieu calme, jamais l'objet de la satire. Seule l'est l'obsession du Bureau à l'inventorier.
- Les huit noms de la 1re version (« Marae des Perles »…) sont retirés de `ST_Islands`. « Marae des Perles » reste le nom de travail du modèle (`marae-des-perles.md`).

### 1.6 Vocabulaire géographique admis
Noms communs de la géographie des atolls, déjà définis dans les specs : **motu**, **hoa**, **lagon**, **platier**, **crique**. Toujours en minuscules, jamais comme nom propre, invariables (« deux motu », « les hoa »).

### 1.7 Le butin des rumeurs
- Une rumeur (`Rumor`) promet ce que la Couronne a **elle-même** perdu ou laissé : caisses d'un caboteur, coffre d'un percepteur, solde de déserteurs, argenterie d'un amiral. Jamais le pillage des habitants ou de leurs lieux sacrés.
- L'indication reste honnête et vague : îlot, crique, platier, palmes. Elle ne promet ni objet précis ni mécanique absente (rien d'enterré : on ne creuse pas ; pas de marée basse : l'eau ne fait que monter). Le butin reste tiré et réglable (§6.2) : `ue-game-designer` valide chaque rumeur contre la table de butin.
- **Perle du Gouverneur** (`marae-des-perles.md`, fiction validée le 2026-10-03) : un bijou de la Couronne laissé sur un autel par une expédition. La rumeur de FRG_01 en dépend, ainsi que de la décision T-139 (a) sur les marae non pillables.

### 1.8 Longueurs (UI12, 1080p, texte maximal, FR et EN)
| Champ | Maximum | Voix |
|---|---|---|
| `Name` | 28 | nom des habitants, traduit |
| `Epithet` | 40 | neutre, factuel : ce que le joueur va trouver (îlots, gués, criques, fourré) |
| `Tagline` | 100 | narrateur, présent, sans plaisanterie |
| `CrownName` | 60 | Bureau |
| `Pitch` | 110 | Bureau : vouvoiement ou impersonnel, litote, sans « ! » |
| `Rumor` | 130 | ouï-dire, conditionnel ; la chute vise la Couronne |
| `Village.*` | 16 | romaji |

Le script de contrôle vérifie ces maxima en FR et en EN, les mots interdits (« Raraka », « tiki », « $ »…), l'unicité des noms et des villages, et la couverture des vingt fragments.

### 1.9 Pas de mécanique inventée
Les textes ne promettent que ce qui existe : terrain réel du fragment (`fragments.json`), nuit et lanternes des villages, gués et nage, montée des eaux, chute des noix, bruit de course entendu la nuit (furtivité, T-262). Aucune île n'est déclarée « plus dure » : la difficulté dépend du jour de campagne (`DA_RunRules.DifficultyByDay`), pas de l'île.

## 2. Les vingt identités
Données : `fragments.json` (T-082) et son tableau dans `fragments-tuamotu.md` ; aperçus `docs/reference/map/raraka/villages/fragments/preview_FRG_Raraka_NN.png` (villages et zones). « Arrivée » : côté et distance du point d'arrivée de la barque, puis longueur de l'approche finale sur le platier. « Coco / fourré » : parts de cocoteraie (DAG 2023) et de fourré.

| Fragment | Nom | Épithète | Type, surface | Arrivée | Coco / fourré |
|---|---|---|---|---|---|
| 01 | Les Trois Fumées | Trois villages reliés à pied | naturel, 52 ha | lagon, 90 m | 22 / 26 |
| 02 | Les Pas-sur-l'Eau | Quatre îlots reliés à gué | archipel, 21 ha | lagon, 120 m | 45 / 26 |
| 03 | Les Maillons | Quatre motu séparés par des hoa étroits | naturel, 42 ha | océan, 120 m | 11 / 33 |
| 04 | Le Bras Tendu | Long motu prolongé d'un îlot | motu scindé, 47 ha | lagon, 90 m | 32 / 32 |
| 05 | Le Convoi | Îlots en file, accostage côté large | naturel, 24 ha | océan, 35 m | 0 / 34 |
| 06 | Le Peigne | Long motu aux douze criques | motu scindé, 32 ha | lagon, 60 m | 9 / 29 |
| 07 | La Comète | Grand motu et traîne d'îlots | motu scindé, 43 ha | océan, 35 m | 26 / 34 |
| 08 | Les Griffes | Motu griffé de langues de sable | motu scindé, 27 ha | lagon, 90 m | 0 / 31 |
| 09 | Le Miroir | Dix criques côté lagon | motu scindé, 28 ha | lagon, 90 m | 0 / 25 |
| 10 | La Poignée de Sable | Îlots épars, la mer entre les villages | archipel, 19 ha | océan, 90 m | 8 / 27 |
| 11 | La Longue Échine | Motu étroit de près de deux kilomètres | motu scindé, 39 ha | lagon, 35 m | 22 / 48 |
| 12 | Les Rames | Îlots longs et parallèles, côté large | naturel, 17 ha | océan, 120 m | 0 / 26 |
| 13 | Le Toit de Palmes | Cocoteraie dense d'un rivage à l'autre | motu scindé, 51 ha | lagon, 90 m | 57 / 18 |
| 14 | La Grande Allée | Motu droit, trois villages en file | motu scindé, 45 ha | lagon, 90 m | 20 / 24 |
| 15 | La Coque Renversée | Large motu ouvert sur le lagon | motu scindé, 49 ha | lagon, 90 m | 20 / 25 |
| 16 | Les Fourrés Qui Écoutent | Fourrés et cocotiers serrés | motu scindé, 32 ha | océan, 90 m | 43 / 40 |
| 17 | Le Ressac | Accostage côté large, long platier | motu scindé, 35 ha | océan, 120 m | 24 / 39 |
| 18 | Le Poing Fermé | Motu ronds et serrés, île courte | motu scindé, 33 ha | océan, 35 m | 11 / 32 |
| 19 | Le Collier | Onze îlots sur plus de deux kilomètres | archipel, 17 ha | océan, 120 m | 0 / 30 |
| 20 | La Lune Couchée | La plus grande île, villages éloignés | motu scindé, 71 ha | océan, 35 m | 52 / 37 |

Pour chaque île : terrain, image de chargement suggérée (`PreviewImage` du DA), villages (zone du générateur, nom du générateur s'il change), dépendances, puis les textes. Les noms de villages sont les mêmes en FR et en EN.

### FRG_Raraka_01 · Les Trois Fumées
- **Terrain** : naturel, 52 ha sur 2,2 km, un seul groupe (l'îlot nord se rejoint à gué) ; arrivée côté lagon à 274 m ; 6 criques, 3 mouillages. Seule île à carte dédiée (`L_Frag_Raraka_01`) : île héros du MVP.
- **Image** : depuis la barque à l'arrivée, les toits des villages au-dessus de la plage.
- **Villages** : richesses F01_Z3 (îlot nord) · bois F01_Z2 · nourriture F01_Z1.
- **Dépend de** : T-139 (a) pour la rumeur (Perle du Gouverneur sur un autel de marae).

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_01.Name` | Les Trois Fumées | The Three Smokes |
| `Island.FRG_Raraka_01.Epithet` | Trois villages reliés à pied | Three villages within walking reach |
| `Island.FRG_Raraka_01.Tagline` | Trois villages sur un croissant de sable ; à la nuit, leurs lanternes s'allument. | Three villages on a crescent of sand; at nightfall, their lanterns come alight. |
| `Island.FRG_Raraka_01.CrownName` | Possession n° 1, dite « l'Échantillon » | Possession No. 1, known as "the Sample" |
| `Island.FRG_Raraka_01.Pitch` | Île sans détour : trois villages à pied, crique d'extraction côté lagon. Le Bureau la juge « pédagogique ». | No detours: three villages on foot, an extraction cove on the lagoon. The Bureau calls it "instructive". |
| `Island.FRG_Raraka_01.Rumor` | Une expédition aurait laissé la Perle du Gouverneur sur l'autel d'un marae. Le Bureau la croit perdue ; les gens d'ici savent où. | An expedition left the Governor's Pearl on a marae altar. The Bureau thinks it lost; the locals know where. |
| `Island.FRG_Raraka_01.Village.Riches` | Kogane-mura | Kogane-mura |
| `Island.FRG_Raraka_01.Village.Wood` | Take-mura | Take-mura |
| `Island.FRG_Raraka_01.Village.Food` | Uo-mura | Uo-mura |

### FRG_Raraka_02 · Les Pas-sur-l'Eau
- **Terrain** : archipel, 21 ha sur 1,8 km, quatre groupes ; chaque village sur son îlot, séparés par le platier à traverser à gué ; arrivée côté lagon ; 7 criques, 3 mouillages.
- **Image** : vue basse de la file d'îlots, platier clair entre eux.
- **Villages** : richesses F02_Z3 · bois F02_Z2 (générateur : Matsu-mura, le pin) · nourriture F02_Z1.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_02.Name` | Les Pas-sur-l'Eau | The Water Steps |
| `Island.FRG_Raraka_02.Epithet` | Quatre îlots reliés à gué | Four islets linked by fords |
| `Island.FRG_Raraka_02.Tagline` | D'un îlot à l'autre, on traverse le platier à gué ; quand la mer monte, les gués disparaissent. | Islet to islet, you wade across the reef flat; when the sea rises, the fords disappear. |
| `Island.FRG_Raraka_02.CrownName` | Banc n° 2, non cadastré (le géomètre refuse de se mouiller) | Bank No. 2, unsurveyed (the surveyor refused to get wet) |
| `Island.FRG_Raraka_02.Pitch` | Des îlots reliés par des gués. Pieds mouillés garantis, indemnité refusée d'avance. | Islets linked by fords. Wet feet guaranteed, compensation refused in advance. |
| `Island.FRG_Raraka_02.Rumor` | Une expédition aurait semé ses coffres d'îlot en îlot en fuyant la marée. Son rapport parle d'un « repli ordonné ». | An expedition dropped its chests islet by islet while fleeing the tide. Its report calls it an "orderly withdrawal". |
| `Island.FRG_Raraka_02.Village.Riches` | Shinju-mura | Shinju-mura |
| `Island.FRG_Raraka_02.Village.Wood` | Ikada-mura | Ikada-mura |
| `Island.FRG_Raraka_02.Village.Food` | Kani-mura | Kani-mura |

### FRG_Raraka_03 · Les Maillons
- **Terrain** : naturel, 42 ha sur 1,85 km, un seul groupe en quatre lobes séparés par des hoa étroits ; arrivée côté océan avec 120 m de platier ; 8 criques, 3 mouillages.
- **Image** : depuis la barque, les quatre lobes en enfilade.
- **Villages** : richesses F03_Z3 · bois F03_Z2 (générateur : Sugi-mura, le cèdre) · nourriture F03_Z1.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_03.Name` | Les Maillons | The Links |
| `Island.FRG_Raraka_03.Epithet` | Quatre motu séparés par des hoa étroits | Four motu split by narrow channels |
| `Island.FRG_Raraka_03.Tagline` | Quatre motu tenus ensemble par des hoa si étroits qu'on les passe à gué. | Four motu held together by channels so narrow you can wade across. |
| `Island.FRG_Raraka_03.CrownName` | Possessions 3-A à 3-D (une seule île, quatre impôts) | Possessions 3-A to 3-D (one island, four taxes) |
| `Island.FRG_Raraka_03.Pitch` | Quatre motu, quatre registres, quatre fois l'impôt. Les hoa se passent à gué ; les formulaires, non. | Four motu, four registers, four lots of tax. The channels can be waded; the forms cannot. |
| `Island.FRG_Raraka_03.Rumor` | Les arpenteurs du Lord Cartographe auraient laissé leurs coffres sur le quatrième motu, le seul qu'ils n'ont pas mesuré. | The Lord Cartographer's surveyors left their chests on the fourth motu, the only one they never measured. |
| `Island.FRG_Raraka_03.Village.Riches` | Tama-mura | Tama-mura |
| `Island.FRG_Raraka_03.Village.Wood` | Yabu-mura | Yabu-mura |
| `Island.FRG_Raraka_03.Village.Food` | Ebi-mura | Ebi-mura |

### FRG_Raraka_04 · Le Bras Tendu
- **Terrain** : motu scindé, 47 ha sur 1,65 km : un long motu courbe et, au sud, un îlot derrière un chenal étroit ; arrivée côté lagon, la plus lointaine de ce côté (292 m) ; 6 criques, 3 mouillages. Cabanes réelles près du chenal (exclusion de 50 m) : aucun texte ne les évoque.
- **Image** : le long motu et l'îlot au bout, vus du galion.
- **Villages** : richesses F04_Z3 (l'îlot sud) · bois F04_Z1 (générateur : Hinoki-mura, le cyprès) · nourriture F04_Z2.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_04.Name` | Le Bras Tendu | The Outstretched Arm |
| `Island.FRG_Raraka_04.Epithet` | Long motu prolongé d'un îlot | A long motu ending in an islet |
| `Island.FRG_Raraka_04.Tagline` | Un long motu tend le bras vers un îlot ; le village des richesses attend au bout des doigts. | A long motu reaches out to an islet; the village of riches waits at its fingertips. |
| `Island.FRG_Raraka_04.CrownName` | Prolongement n° 4 (l'îlot attend un formulaire distinct) | Extension No. 4 (the islet awaits a separate form) |
| `Island.FRG_Raraka_04.Pitch` | Un long motu et son îlot. Le Bureau rappelle que la richesse se trouve souvent au bout du bras. | A long motu and its islet. The Bureau observes that wealth tends to sit at arm's end. |
| `Island.FRG_Raraka_04.Rumor` | Un commis de la Couronne aurait laissé ses coffres sur l'îlot, le temps d'aller chercher un tampon. Il n'est jamais revenu. | A Crown clerk left his chests on the islet while he went for a stamp. He never came back. |
| `Island.FRG_Raraka_04.Village.Riches` | Gin-mura | Gin-mura |
| `Island.FRG_Raraka_04.Village.Wood` | Hayashi-mura | Hayashi-mura |
| `Island.FRG_Raraka_04.Village.Food` | Ami-mura | Ami-mura |

### FRG_Raraka_05 · Le Convoi
- **Terrain** : naturel, 24 ha sur 1,7 km, deux groupes d'îlots alignés en diagonale ; arrivée côté océan, approche courte (35 m) ; 5 criques, 2 mouillages ; aucune cocoteraie relevée.
- **Image** : la file d'îlots vue du large, comme une ligne de navires.
- **Villages** : richesses F05_Z2 · bois F05_Z1 (générateur : Kusu-mura, le camphrier) · nourriture F05_Z3.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_05.Name` | Le Convoi | The Convoy |
| `Island.FRG_Raraka_05.Epithet` | Îlots en file, accostage côté large | Islets in a line, ocean-side landing |
| `Island.FRG_Raraka_05.Tagline` | Une file d'îlots qui semble faire route vers le large, sans jamais lever l'ancre. | A line of islets that seems bound for the open sea, yet never weighs anchor. |
| `Island.FRG_Raraka_05.CrownName` | Escadre n° 5, immobile (rapport de mer en attente) | Squadron No. 5, stationary (log report pending) |
| `Island.FRG_Raraka_05.Pitch` | Des îlots en file, accostage côté large. Le Bureau attend toujours le rapport de l'escadre. | Islets in a line, landing on the ocean side. The Bureau still awaits the squadron's report. |
| `Island.FRG_Raraka_05.Rumor` | Un ravitailleur de l'escadre aurait laissé ses caisses sur les îlots. L'Amirauté nie l'escadre, mais réclame les caisses. | A squadron supply ship left its crates on the islets. The Admiralty denies the squadron but claims the crates. |
| `Island.FRG_Raraka_05.Village.Riches` | Akari-mura | Akari-mura |
| `Island.FRG_Raraka_05.Village.Wood` | Shigemi-mura | Shigemi-mura |
| `Island.FRG_Raraka_05.Village.Food` | Nori-mura | Nori-mura |

### FRG_Raraka_06 · Le Peigne
- **Terrain** : motu scindé, 32 ha sur 1,9 km : moitié ouest pleine, moitié est découpée en dents de sable ; 12 criques (record du catalogue), 3 mouillages ; arrivée côté lagon.
- **Image** : vue rasante sur les dents de sable et les criques.
- **Villages** : richesses F06_Z3 (générateur : Hikari-mura, doublon de sens avec Akari) · bois F06_Z2 (générateur : Yanagi-mura, le saule) · nourriture F06_Z1.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_06.Name` | Le Peigne | The Comb |
| `Island.FRG_Raraka_06.Epithet` | Long motu aux douze criques | A long motu with twelve coves |
| `Island.FRG_Raraka_06.Tagline` | À l'est, la terre se défait en dents de sable ; entre deux dents, une crique calme. | To the east, the land frays into teeth of sand; between two teeth lies a quiet cove. |
| `Island.FRG_Raraka_06.CrownName` | Domaine des criques n° 6 (douze criques, un percepteur) | Cove Estate No. 6 (twelve coves, one collector) |
| `Island.FRG_Raraka_06.Pitch` | Douze criques pour un seul percepteur. Le Bureau vous saura gré de n'en oublier aucune. | Twelve coves for a single collector. The Bureau would be obliged if you missed none. |
| `Island.FRG_Raraka_06.Rumor` | Une barque du Bureau aurait égaré ses caisses dans l'une des douze criques. Le rapport ne précise pas laquelle. | A Bureau boat mislaid its crates in one of the twelve coves. The report does not say which. |
| `Island.FRG_Raraka_06.Village.Riches` | Hoseki-mura | Hoseki-mura |
| `Island.FRG_Raraka_06.Village.Wood` | Eda-mura | Eda-mura |
| `Island.FRG_Raraka_06.Village.Food` | Kai-mura | Kai-mura |

### FRG_Raraka_07 · La Comète
- **Terrain** : motu scindé, 43 ha sur 1,9 km : un grand motu au nord, puis une traîne de motu et d'îlots vers le sud ; arrivée côté océan, approche 35 m ; 8 criques, aucun mouillage relevé.
- **Image** : vue aérienne de la tête et de la traîne.
- **Villages** : richesses F07_Z3 · bois F07_Z1 (générateur : Kaba-mura, le bouleau) · nourriture F07_Z2.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_07.Name` | La Comète | The Comet |
| `Island.FRG_Raraka_07.Epithet` | Grand motu et traîne d'îlots | A large motu trailing islets |
| `Island.FRG_Raraka_07.Tagline` | Un grand motu en tête, puis une traîne d'îlots qui s'effiloche vers le sud. | A large motu at the head, then a tail of islets fraying away to the south. |
| `Island.FRG_Raraka_07.CrownName` | Phénomène céleste n° 7, déclaré terre ferme par le Trésor | Celestial Phenomenon No. 7, declared dry land by Treasury |
| `Island.FRG_Raraka_07.Pitch` | Un grand motu et sa traîne d'îlots. Le Trésor a déclaré la comète terre ferme ; vérifiez quand même. | A large motu and its tail of islets. The Treasury declared the comet dry land; check anyway. |
| `Island.FRG_Raraka_07.Rumor` | Des astronomes de la Couronne y attendaient la comète. La comète est passée ; leurs caisses d'instruments sont restées. | Crown astronomers waited here for the comet. The comet passed; their crates of instruments stayed. |
| `Island.FRG_Raraka_07.Village.Riches` | Hoshi-mura | Hoshi-mura |
| `Island.FRG_Raraka_07.Village.Wood` | Kodachi-mura | Kodachi-mura |
| `Island.FRG_Raraka_07.Village.Food` | Tako-mura | Tako-mura |

### FRG_Raraka_08 · Les Griffes
- **Terrain** : motu scindé, 27 ha sur 1,7 km, côte du lagon découpée en longues langues de sable ; arrivée côté lagon, la plus proche du catalogue (201 m) ; 7 criques, 3 mouillages ; aucune cocoteraie relevée.
- **Image** : les langues de sable au crépuscule.
- **Villages** : richesses F08_Z3 · bois F08_Z2 (générateur : Momiji-mura, l'érable) · nourriture F08_Z1 (générateur : Kame-mura, la tortue).

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_08.Name` | Les Griffes | The Claws |
| `Island.FRG_Raraka_08.Epithet` | Motu griffé de langues de sable | A motu clawed by spits of sand |
| `Island.FRG_Raraka_08.Tagline` | Côté lagon, de longues langues de sable griffent l'eau. La nuit, on préfère ne pas savoir pourquoi. | On the lagoon side, long spits of sand claw at the water. At night, you would rather not know why. |
| `Island.FRG_Raraka_08.CrownName` | Parcelle griffée n° 8 (réparations aux frais de l'équipage) | Clawed Plot No. 8 (repairs charged to the crew) |
| `Island.FRG_Raraka_08.Pitch` | Côté lagon, la côte se découpe en langues de sable. Le Bureau décline toute responsabilité nocturne. | On the lagoon side, the shore splits into spits of sand. The Bureau declines all liability after dark. |
| `Island.FRG_Raraka_08.Rumor` | Des déserteurs de l'Amirauté y auraient caché leur solde. L'Amirauté les cherche encore ; leur solde, surtout. | Admiralty deserters hid their pay here. The Admiralty is still looking for them; for the pay, mostly. |
| `Island.FRG_Raraka_08.Village.Riches` | Tsuki-mura | Tsuki-mura |
| `Island.FRG_Raraka_08.Village.Wood` | Takigi-mura | Takigi-mura |
| `Island.FRG_Raraka_08.Village.Food` | Himono-mura | Himono-mura |

### FRG_Raraka_09 · Le Miroir
- **Terrain** : motu scindé, 28 ha sur 1,8 km, côte du lagon en criques plates ; 10 criques, 3 mouillages ; arrivée côté lagon ; fourré clair (25 %), aucune cocoteraie relevée.
- **Image** : une crique plate au coucher du soleil, le ciel dans l'eau.
- **Villages** : richesses F09_Z3 (générateur : Asahi-mura, marque connue) · bois F09_Z2 (générateur : Sakura-mura, le cerisier) · nourriture F09_Z1 (générateur : Same-mura, le requin).

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_09.Name` | Le Miroir | The Mirror |
| `Island.FRG_Raraka_09.Epithet` | Dix criques côté lagon | Ten coves on the lagoon side |
| `Island.FRG_Raraka_09.Tagline` | Dix criques plates où le ciel se pose ; par temps calme, on ne sait plus où finit l'eau. | Ten flat coves where the sky comes to rest; on calm days, you cannot tell where the water ends. |
| `Island.FRG_Raraka_09.CrownName` | Lagon réglementaire n° 9 (eau jugée non conforme) | Regulation Lagoon No. 9 (water found non-compliant) |
| `Island.FRG_Raraka_09.Pitch` | Un lagon si calme que le Bureau le soupçonne de cacher quelque chose. Pour une fois, il a raison. | A lagoon so calm the Bureau suspects it of hiding something. For once, it is right. |
| `Island.FRG_Raraka_09.Rumor` | Par temps clair, on verrait des caisses au fond du lagon, tombées d'une barque trop chargée. Le quota, lui, était respecté. | On clear days, crates show on the lagoon floor, lost from an overloaded boat. The quota, at least, was met. |
| `Island.FRG_Raraka_09.Village.Riches` | Kaigara-mura | Kaigara-mura |
| `Island.FRG_Raraka_09.Village.Wood` | Hamabo-mura | Hamabo-mura |
| `Island.FRG_Raraka_09.Village.Food` | Ido-mura | Ido-mura |

### FRG_Raraka_10 · La Poignée de Sable
- **Terrain** : archipel, 19 ha sur 1,8 km, quatre groupes d'îlots épars ; chaque village de zone sur son îlot, la mer entre eux (nage ou barque) ; arrivée côté océan ; 2 criques, aucun mouillage relevé.
- **Image** : la poignée d'îlots vue du galion.
- **Villages** : richesses F10_Z3 · bois F10_Z1 (générateur : Ume-mura, le prunier) · nourriture F10_Z2.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_10.Name` | La Poignée de Sable | The Handful of Sand |
| `Island.FRG_Raraka_10.Epithet` | Îlots épars, la mer entre les villages | Scattered islets, sea between villages |
| `Island.FRG_Raraka_10.Tagline` | Comme une poignée de sable jetée sur la mer : chaque village a son îlot. | Like a handful of sand thrown on the sea: each village has its own islet. |
| `Island.FRG_Raraka_10.CrownName` | Possessions en vrac n° 10 (recomptage en cours) | Bulk Possessions No. 10 (recount in progress) |
| `Island.FRG_Raraka_10.Pitch` | Des îlots semés au large, la mer entre chaque village. Le Bureau conseille de les compter avant de repartir. | Islets sown offshore, with sea between every village. The Bureau advises counting them before leaving. |
| `Island.FRG_Raraka_10.Rumor` | Le Trésor y aurait réparti ses caisses entre les îlots, par prudence. La prudence n'a pas laissé de carte. | The Treasury spread its crates across the islets, out of caution. Caution left no map. |
| `Island.FRG_Raraka_10.Village.Riches` | Yuhi-mura | Yuhi-mura |
| `Island.FRG_Raraka_10.Village.Wood` | Ryuboku-mura | Ryuboku-mura |
| `Island.FRG_Raraka_10.Village.Food` | Shio-mura | Shio-mura |

### FRG_Raraka_11 · La Longue Échine
- **Terrain** : motu scindé, 39 ha sur 1,85 km, long, étroit et presque droit, coupé par un hoa à chaque bout ; fourré 48 % (record) ; arrivée côté lagon, approche 35 m ; 8 criques, 3 mouillages.
- **Image** : la crête de l'île vue de la barque, de profil.
- **Villages** : richesses F11_Z3 · bois F11_Z1 · nourriture F11_Z2 (noms du générateur gardés).

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_11.Name` | La Longue Échine | The Long Spine |
| `Island.FRG_Raraka_11.Epithet` | Motu étroit de près de deux kilomètres | A narrow motu, nearly 2 km long |
| `Island.FRG_Raraka_11.Tagline` | Une échine de fourrés d'un bout à l'autre ; d'un village au suivant, la marche est longue. | A spine of thicket from end to end; from one village to the next, the walk is long. |
| `Island.FRG_Raraka_11.CrownName` | Îlot du Vice-Contrôleur Pembury (nom contesté par Pembury) | Vice-Comptroller Pembury Islet (name disputed by Pembury) |
| `Island.FRG_Raraka_11.Pitch` | Une île tout en longueur. La barque suivra, le Vice-Contrôleur aussi, sur le papier. | An island that is all length. The boat will follow, and so will the Vice-Comptroller, on paper. |
| `Island.FRG_Raraka_11.Rumor` | Un caboteur royal aurait coulé sur le platier. Ses caisses portent encore le sceau de la Couronne, ce qui ne les allège pas. | A royal coaster sank on the reef flat. Its crates still bear the Crown's seal, which does not make them any lighter. |
| `Island.FRG_Raraka_11.Village.Riches` | Hinode-mura | Hinode-mura |
| `Island.FRG_Raraka_11.Village.Wood` | Yashi-mura | Yashi-mura |
| `Island.FRG_Raraka_11.Village.Food` | Hama-mura | Hama-mura |

### FRG_Raraka_12 · Les Rames
- **Terrain** : naturel, 17 ha sur 1,7 km, îlots allongés en lames parallèles, deux groupes : on patauge d'un îlot à l'autre ; arrivée côté océan à 296 m, approche 120 m ; 7 criques, aucun mouillage relevé.
- **Image** : vue aérienne des îlots alignés comme des rames.
- **Villages** : richesses F12_Z3 · bois F12_Z1 (générateur : Ashi-mura, le roseau) · nourriture F12_Z2.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_12.Name` | Les Rames | The Oars |
| `Island.FRG_Raraka_12.Epithet` | Îlots longs et parallèles, côté large | Long parallel islets, ocean side |
| `Island.FRG_Raraka_12.Tagline` | Des îlots longs et minces, rangés comme des rames posées sur l'eau. | Long thin islets, laid out like oars resting on the water. |
| `Island.FRG_Raraka_12.CrownName` | Rame n° 12 (inventoriée avec le papier du Bureau) | Oars, Lot No. 12 (inventoried as Bureau stationery) |
| `Island.FRG_Raraka_12.Pitch` | Des îlots longs et minces, alignés côté large. Entre eux, on patauge ; le Bureau, lui, reste au sec. | Long thin islets lined up on the ocean side. Between them you wade; the Bureau stays dry. |
| `Island.FRG_Raraka_12.Rumor` | Un canot de l'Amirauté y aurait perdu ses rames et sa cargaison. On a retrouvé les rames. | An Admiralty launch lost its oars and its cargo here. The oars have been found. |
| `Island.FRG_Raraka_12.Village.Riches` | Sango-mura | Sango-mura |
| `Island.FRG_Raraka_12.Village.Wood` | Ita-mura | Ita-mura |
| `Island.FRG_Raraka_12.Village.Food` | Iso-mura | Iso-mura |

### FRG_Raraka_13 · Le Toit de Palmes
- **Terrain** : motu scindé, 51 ha sur 1,6 km, large ; cocoteraie 57 % (record) et fourré 18 % (le plus clair) : l'île la plus ouverte, sous les palmes ; arrivée côté lagon ; 4 criques, 3 mouillages. Cabanes réelles côté lagon (exclusion de 50 m) : aucun texte ne les évoque.
- **Image** : sous les cocotiers, la lumière en éclats.
- **Villages** : richesses F13_Z3 (générateur : Kiku-mura, le chrysanthème, emblème impérial) · bois F13_Z1 (générateur : Yoshi-mura, le roseau) · nourriture F13_Z2.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_13.Name` | Le Toit de Palmes | The Palm Roof |
| `Island.FRG_Raraka_13.Epithet` | Cocoteraie dense d'un rivage à l'autre | Dense coconut grove from shore to shore |
| `Island.FRG_Raraka_13.Tagline` | Sous les palmes, la lumière tombe en éclats ; les noix aussi, sans prévenir. | Under the palms, light falls in shards; so do the coconuts, without warning. |
| `Island.FRG_Raraka_13.CrownName` | Cocoteraie de la Couronne n° 13 (noix comptées une à une) | Crown Coconut Grove No. 13 (nuts counted one by one) |
| `Island.FRG_Raraka_13.Pitch` | Une cocoteraie d'un rivage à l'autre. Le Bureau compte les noix ; il vous laisse les recevoir. | A coconut grove from shore to shore. The Bureau counts the nuts; it leaves the catching to you. |
| `Island.FRG_Raraka_13.Rumor` | Un intendant aurait caché la caisse de la Couronne sous les palmes. Sous lesquelles ? Il y en a quelques milliers. | A steward hid the Crown's chest under the palms. Which ones? There are a few thousand. |
| `Island.FRG_Raraka_13.Village.Riches` | Koban-mura | Koban-mura |
| `Island.FRG_Raraka_13.Village.Wood` | Komorebi-mura | Komorebi-mura |
| `Island.FRG_Raraka_13.Village.Food` | Umi-mura | Umi-mura |

### FRG_Raraka_14 · La Grande Allée
- **Terrain** : motu scindé, 45 ha sur 1,6 km, bande droite coupée par un hoa à chaque bout ; les trois villages se suivent du nord au sud ; arrivée côté lagon ; 3 criques, 3 mouillages. Cabanes réelles sur la rive (exclusion de 50 m).
- **Image** : l'allée de cocotiers vue depuis un village.
- **Villages** : richesses F14_Z3 (générateur : Fuji-mura, nom célèbre) · bois F14_Z1 · nourriture F14_Z2.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_14.Name` | La Grande Allée | The Grand Avenue |
| `Island.FRG_Raraka_14.Epithet` | Motu droit, trois villages en file | Straight motu, three villages in a row |
| `Island.FRG_Raraka_14.Tagline` | Un long motu droit : les trois villages se suivent comme les étapes d'une route. | A long, straight motu: the three villages follow one another like stages on a road. |
| `Island.FRG_Raraka_14.CrownName` | Avenue royale n° 14 (pavage prévu, sans date) | Royal Avenue No. 14 (paving planned, no date set) |
| `Island.FRG_Raraka_14.Pitch` | Un motu droit, trois villages en file. Le Bureau promet une route pavée depuis le siècle dernier. | A straight motu, three villages in a row. The Bureau has promised a paved road since the last century. |
| `Island.FRG_Raraka_14.Rumor` | Le Trésor y aurait livré la paie des paveurs de l'avenue royale. Les paveurs ne sont jamais venus ; la paie est restée. | The Treasury delivered the pavers' wages for the royal avenue. The pavers never came; the wages stayed. |
| `Island.FRG_Raraka_14.Village.Riches` | Zeni-mura | Zeni-mura |
| `Island.FRG_Raraka_14.Village.Wood` | Kaya-mura | Kaya-mura |
| `Island.FRG_Raraka_14.Village.Food` | Ura-mura | Ura-mura |

### FRG_Raraka_15 · La Coque Renversée
- **Terrain** : motu scindé, 49 ha sur 1,7 km, large bande à la côte nord festonnée ; arrivée côté lagon ; 3 criques, 3 mouillages. Cabanes réelles côté lagon (exclusion de 50 m) : aucun texte ne les évoque.
- **Image** : la silhouette de l'île depuis le galion, basse et allongée.
- **Villages** : richesses F15_Z3 (générateur : Tsubaki-mura, le camélia) · bois F15_Z2 (générateur : Tochi-mura, le marronnier) · nourriture F15_Z1.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_15.Name` | La Coque Renversée | The Upturned Hull |
| `Island.FRG_Raraka_15.Epithet` | Large motu ouvert sur le lagon | A broad motu facing the lagoon |
| `Island.FRG_Raraka_15.Tagline` | Vue du galion, l'île ressemble à une coque retournée, échouée au bord du lagon. | From the galleon, the island looks like an upturned hull stranded at the lagoon's edge. |
| `Island.FRG_Raraka_15.CrownName` | Coque n° 15, réputée navire (droits de port exigibles) | Hull No. 15, deemed a vessel (harbour dues payable) |
| `Island.FRG_Raraka_15.Pitch` | Une île large, ouverte sur le lagon. Le Bureau la tient pour un navire et réclame les droits de port. | A broad island open to the lagoon. The Bureau deems it a ship and claims harbour dues. |
| `Island.FRG_Raraka_15.Rumor` | Un fret de la Couronne y aurait été débarqué pour échapper aux droits de port. Le Bureau réclame les droits, et le fret. | A Crown cargo was landed here to dodge harbour dues. The Bureau claims the dues, and the cargo. |
| `Island.FRG_Raraka_15.Village.Riches` | Kazari-mura | Kazari-mura |
| `Island.FRG_Raraka_15.Village.Wood` | Funaki-mura | Funaki-mura |
| `Island.FRG_Raraka_15.Village.Food` | Oki-mura | Oki-mura |

### FRG_Raraka_16 · Les Fourrés Qui Écoutent
- **Terrain** : motu scindé, 32 ha sur 1,6 km, deux moitiés reliées par un col étroit et découpé ; cocoteraie 43 % et fourré 40 % : l'une des îles les plus couvertes, avec FRG_20 ; arrivée côté océan ; 4 criques, 3 mouillages.
- **Image** : la lisière du fourré la nuit, une lanterne de pierre au loin.
- **Villages** : richesses F16_Z3 (Hotaru, la luciole : les villages en ont la nuit) · bois F16_Z1 (générateur : Kashi-mura, le chêne) · nourriture F16_Z2 (générateur : Seto-mura, ville connue).

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_16.Name` | Les Fourrés Qui Écoutent | The Listening Thickets |
| `Island.FRG_Raraka_16.Epithet` | Fourrés et cocotiers serrés | Tight thicket and palms |
| `Island.FRG_Raraka_16.Tagline` | Les fourrés couvrent l'île jusqu'au sable ; la nuit, on y marche, on n'y court pas. | Thickets cover the island down to the sand; at night, you walk here, you do not run. |
| `Island.FRG_Raraka_16.CrownName` | Terre provisoirement inhabitable (pétition en cours) | Provisionally Uninhabitable Land (petition pending) |
| `Island.FRG_Raraka_16.Pitch` | Le fourré entend tout. Le Bureau conseille de parler bas et de remplir les formulaires vite. | The thicket hears everything. The Bureau advises speaking softly and filling in forms quickly. |
| `Island.FRG_Raraka_16.Rumor` | Une expédition y a perdu ses lanternes, ses coffres et son courage. Les coffres, au moins, n'ont pas bougé. | An expedition lost its lanterns, its chests and its nerve here. The chests, at least, have not moved. |
| `Island.FRG_Raraka_16.Village.Riches` | Hotaru-mura | Hotaru-mura |
| `Island.FRG_Raraka_16.Village.Wood` | Adan-mura | Adan-mura |
| `Island.FRG_Raraka_16.Village.Food` | Tsubo-mura | Tsubo-mura |

### FRG_Raraka_17 · Le Ressac
- **Terrain** : motu scindé, 35 ha sur 1,4 km, bande courbe qui s'élargit au sud-ouest ; arrivée côté océan avec 120 m de platier ; 2 criques, 3 mouillages ; fourré 39 %.
- **Image** : le platier depuis la barque, la mer qui se brise devant.
- **Villages** : richesses F17_Z3 · bois F17_Z2 (générateur : Keyaki-mura, le zelkova) · nourriture F17_Z1 (générateur : Minato-mura, quartier de Tokyo).

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_17.Name` | Le Ressac | The Backwash |
| `Island.FRG_Raraka_17.Epithet` | Accostage côté large, long platier | Ocean-side landing, long reef flat |
| `Island.FRG_Raraka_17.Tagline` | Côté large, la mer se brise sur un long platier avant de laisser passer la barque. | On the ocean side, the sea breaks on a long reef flat before it lets the boat through. |
| `Island.FRG_Raraka_17.CrownName` | Écueil honoraire Fenwick (décoré à titre posthume) | Honorary Fenwick Reef (decorated posthumously) |
| `Island.FRG_Raraka_17.Pitch` | On y accoste côté large, après un long platier. L'amiral Fenwick y a laissé son nom, et son navire avec. | You land ocean-side, across a long reef flat. Admiral Fenwick left his name here, and his ship with it. |
| `Island.FRG_Raraka_17.Rumor` | L'épave de l'amiral rendrait encore son argenterie sur le platier. L'Amirauté réclame la vaisselle, pas l'amiral. | The admiral's wreck still gives up its silver on the reef flat. The Admiralty wants the tableware back, not the admiral. |
| `Island.FRG_Raraka_17.Village.Riches` | Misaki-mura | Misaki-mura |
| `Island.FRG_Raraka_17.Village.Wood` | Kikori-mura | Kikori-mura |
| `Island.FRG_Raraka_17.Village.Food` | Katsuo-mura | Katsuo-mura |

### FRG_Raraka_18 · Le Poing Fermé
- **Terrain** : motu scindé, 33 ha sur 1,27 km, l'île la plus courte, en lobes ronds serrés comme des jointures ; arrivée côté océan, approche 35 m ; 4 criques, aucun mouillage relevé.
- **Image** : vue aérienne des lobes, du nord au sud.
- **Villages** : richesses F18_Z3 (générateur : Tsuru-mura, la grue) · bois F18_Z2 (générateur : Kiri-mura, le paulownia) · nourriture F18_Z1.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_18.Name` | Le Poing Fermé | The Closed Fist |
| `Island.FRG_Raraka_18.Epithet` | Motu ronds et serrés, île courte | Round, tight motu; a short island |
| `Island.FRG_Raraka_18.Tagline` | Une île courte, ramassée sur elle-même : tout y est proche, même ce qu'on ne voit pas. | A short island, curled in on itself: everything is close, even what you cannot see. |
| `Island.FRG_Raraka_18.CrownName` | Possession de poche de Sa Majesté | The Crown's Pocket Possession |
| `Island.FRG_Raraka_18.Pitch` | Île courte, gros ennuis. Le Trésor la déclare « très rentable au mètre carré ». | Short island, big trouble. The Treasury rates it "highly profitable per square yard". |
| `Island.FRG_Raraka_18.Rumor` | Un percepteur royal s'y serait caché avec la caisse de l'impôt. On cherche toujours la caisse ; le percepteur, beaucoup moins. | A royal tax collector hid here with the tax chest. The chest is still being sought; the collector, far less so. |
| `Island.FRG_Raraka_18.Village.Riches` | Akagane-mura | Akagane-mura |
| `Island.FRG_Raraka_18.Village.Wood` | Sumi-mura | Sumi-mura |
| `Island.FRG_Raraka_18.Village.Food` | Fune-mura | Fune-mura |

### FRG_Raraka_19 · Le Collier
- **Terrain** : archipel, 17 ha étirés sur 2,3 km, onze groupes d'îlots (record), chacun à une nage du suivant ; arrivée côté océan, approche 120 m ; 2 criques, 1 mouillage ; aucune cocoteraie relevée.
- **Image** : la file d'îlots vue de haut sur le platier clair.
- **Villages** : richesses F19_Z2 · bois F19_Z1 (générateur : Shii-mura, le châtaignier du Japon) · nourriture F19_Z3.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_19.Name` | Le Collier | The Necklace |
| `Island.FRG_Raraka_19.Epithet` | Onze îlots sur plus de deux kilomètres | Eleven islets over two kilometres |
| `Island.FRG_Raraka_19.Tagline` | Onze îlots enfilés sur le platier comme les grains d'un collier. | Eleven islets strung along the reef flat like beads on a necklace. |
| `Island.FRG_Raraka_19.CrownName` | Parure n° 19 (en dépôt au Trésor, sans inventaire) | Jewel Set No. 19 (held by the Treasury, uninventoried) |
| `Island.FRG_Raraka_19.Pitch` | Onze îlots en rang, chacun à une nage du suivant. Le Trésor réclame l'inventaire de chaque grain. | Eleven islets in a row, each a swim from the next. The Treasury wants every bead inventoried. |
| `Island.FRG_Raraka_19.Rumor` | Un collier de la Couronne s'y serait rompu pendant une inspection. Le Trésor compte onze perles perdues ; il en manque douze. | A Crown necklace broke here during an inspection. The Treasury counts eleven pearls lost; twelve are missing. |
| `Island.FRG_Raraka_19.Village.Riches` | Kasumi-mura | Kasumi-mura |
| `Island.FRG_Raraka_19.Village.Wood` | Kozue-mura | Kozue-mura |
| `Island.FRG_Raraka_19.Village.Food` | Izumi-mura | Izumi-mura |

### FRG_Raraka_20 · La Lune Couchée
- **Terrain** : motu scindé, 71 ha sur 2 km, le plus grand fragment, long et courbe ; cocoteraie 52 %, fourré 37 % ; villages éloignés les uns des autres ; arrivée côté océan, approche 35 m ; 5 criques, 2 mouillages.
- **Image** : la courbe entière depuis le galion, au lever de la lune.
- **Villages** : richesses F20_Z3 · bois F20_Z2 (générateur : Enoki-mura, le micocoulier) · nourriture F20_Z1.

| Clé | FR | EN |
|---|---|---|
| `Island.FRG_Raraka_20.Name` | La Lune Couchée | The Reclining Moon |
| `Island.FRG_Raraka_20.Epithet` | La plus grande île, villages éloignés | The largest island, villages far apart |
| `Island.FRG_Raraka_20.Tagline` | Un grand motu courbé comme une lune posée sur l'eau ; ses villages sont loin les uns des autres. | A great motu curved like a moon laid on the water; its villages lie far apart. |
| `Island.FRG_Raraka_20.CrownName` | Nouvelle-Pettifer (en attente du sceau depuis onze ans) | New Pettifer (awaiting the seal for eleven years) |
| `Island.FRG_Raraka_20.Pitch` | Lord Pettifer veut y fonder une ville à son nom. Commencez par en revenir. | Lord Pettifer wants a town named after him here. Start by coming back alive. |
| `Island.FRG_Raraka_20.Rumor` | Le coffre-fort de la future mairie aurait été débarqué avant la mairie. Il attend toujours, plein. | The future town hall's strongbox was landed before the town hall. It is still waiting, full. |
| `Island.FRG_Raraka_20.Village.Riches` | Nagisa-mura | Nagisa-mura |
| `Island.FRG_Raraka_20.Village.Wood` | Kokage-mura | Kokage-mura |
| `Island.FRG_Raraka_20.Village.Food` | Mizu-mura | Mizu-mura |

`FRG_Raraka_21` (l'île du fort dans le lagon, « Fort Sainte-Barbe ») est sortie du catalogue (`forts-atoll.md`) : pas d'identité, son DA n'est pas modifié.

## 3. Affichage en jeu (proposition, sans code)
Principe : l'île se présente en trois temps, sans jamais nommer l'atoll réel. Chargement : où l'on va. Arrivée : où l'on est. Bilan : ce qu'on a quitté. Tous les textes viennent de `ST_Islands` ; les mises en capitales sont faites par le widget (`FText::ToUpper`), jamais dans la String Table.

### 3.1 Carte d'arrivée (T-172, ADR-007)
ADR-007 prévoit « JOUR 1 / RARAKA — SECTEUR NORD / QUOTA 0 / 1 800 ». Proposition, sans toponyme ni « secteur » :

```
JOUR 3 / 10
LES TROIS FUMÉES
Trois villages reliés à pied
Quota : 2 300 / 5 500 couronnes
```
- Ligne 1 : le titre « Jour N » existant de la transition (`UDayTransitionSubsystem`).
- Ligne 2, la plus grande : `Island.<FRG>.Name` en capitales. Contrôle UI12 sur le nom le plus long, « LES FOURRÉS QUI ÉCOUTENT » (24 caractères), en FR et en EN.
- Ligne 3 : `Island.<FRG>.Epithet`, en petit.
- Ligne 4 : le texte de quota existant du HUD (monnaie : relevé `ecarts-textes.md`, lignes 1 à 3).
- Pas de désignation de la Couronne ni de phrase d'ambiance sur la carte : elle doit se lire en deux secondes.
- Moment : pendant l'arrivée de la caméra vers le galion, après l'écran de chargement, comme le titre « Jour N » aujourd'hui.

### 3.2 Écran de chargement (§16)
```
Destination : Le Peigne
Registre de la Couronne : Domaine des criques n° 6 (douze criques, un percepteur)
« À l'est, la terre se défait en dents de sable ; entre deux dents, une crique calme. »
Jour 4/10 · Quota 6 100 / 10 500 couronnes
[conseil : une circulaire du Bureau]
```
- `Island.UI.Destination` (titre), `Island.UI.CrownNameFormat` (petit), `Island.<FRG>.Tagline` (italique), puis la ligne de jour existante (`MakeDayLine`), où le nom de l'île remplace le nom de palier (relevé `ecarts-textes.md` n° 11), et un conseil.
- Image : `PreviewImage` du DA (prises de vue suggérées en section 2).
- Condition réseau : la destination doit être connue sur chaque machine avant le voyage (identifiant du fragment tiré, répliqué avant le `ServerTravel`). Sinon, l'écran n'affiche que le jour et le conseil, et le nom n'apparaît qu'à l'arrivée.

### 3.3 Bilan de fin de jour (`STapaoDayEnd`, « JOUR N TERMINÉ »)
- Sous le titre : `Island.UI.DayEndIsland`, « Île quittée à temps : Le Peigne ». Le mot « Île » porte l'accord : aucun problème de genre ni de nombre, quel que soit le nom (« Les Griffes », « Le Miroir »).
- En petit, la ligne du registre : `Island.UI.CrownNameFormat`. Le Percepteur de 3e classe parle au nom de ce registre, jamais au nom de l'île.
- Équipage perdu : la ligne de l'île n'est pas affichée ; les textes de défaite existants suffisent.
- Le compteur de campagne pourra montrer la liste des îles quittées (`Name`) : la recette n° 9 se lit alors d'un coup d'œil.

### 3.4 Tableau de contrats (L4) et carte tactique
- Contrat : `Name` en titre, `Island.UI.CrownNameFormat` en sous-titre, `Pitch`, puis `Island.UI.RumorFormat`. Tester UI12 avec la plus longue rumeur (130 caractères).
- Notifications de zone et carte : nom du village de la zone par thème (`Island.<FRG>.Village.Riches|Wood|Food`), libellé du thème (`Island.Theme.*`), liste `Island.UI.VillagesFormat`.

### 3.5 Ce qui dépend de T-139
| Élément | Dépendance | Sans décision |
|---|---|---|
| Les vingt noms, épithètes et phrases d'ambiance | aucune : ni « tiki », ni « Raraka », ni créature nommée | utilisables tels quels |
| Rumeur de FRG_01 (Perle du Gouverneur sur l'autel d'un marae) | T-139 (a) : marae non pillables | si les marae deviennent non pillables, remplacer la rumeur (proposition : la Perle tombée d'une barque dans le lagon) |
| `DataCredits` des 20 DA : « … l'atoll de Raraka (Tuamotu) … » | T-139 (c) et décision 1 de `ecarts-textes.md` (crédits) | non modifié par le script, signalé dans son rapport |
| Carte d'arrivée de l'ADR-007 (« RARAKA — SECTEUR NORD ») | T-139 (c) | la proposition 3.1 n'utilise que le nom fictif |
| « l'Anneau » (nom de l'archipel dans la bible) | décision 4 de `ecarts-textes.md` | aucun nom d'île ne l'emploie |

### 3.6 Code à prévoir (propositions au lead, aucun C++ modifié ici)
- `UExpeditionIslandDefinition` n'a que `DisplayName` (renseigné par le script). Proposition : des `FText` `Epithet`, `Tagline`, `CrownName`, `Pitch`, `Rumor` (catégorie `Identity`), que le script remplit dès qu'ils existent (il saute aujourd'hui les propriétés absentes et les liste dans son rapport). Variante plus légère : une fonction `GetIdentityText(Champ)` qui lit `Island.<FragmentId>.<Champ>` dans `ST_Islands`. Les propriétés restent préférables : la référence est visible dans l'asset, vérifiée au cook et à la collecte de localisation.
- Villages : afficher `Island.<FragmentId>.Village.<Riches|Wood|Food>` selon `FVillageLayout::Theme` (`richesses`, `bois`, `vivres`), au lieu du nom de travail `FVillageLayout::Name`.
- Destination connue avant le voyage (3.2) ; textes `Island.UI.Destination` et `Island.UI.DayEndIsland` dans les écrans existants.

## 4. String Table `ST_Islands`
- **Asset** : `/Game/MonkeIsland/UI/Text/ST_Islands`, créé par T-095 et déjà référencé par `DA_Island_Raraka_01..08.DisplayName`. La convention de nommage dit `ST_Text_` (`asset-naming.md`, car `ST_` sert aussi aux StateTree) : on garde `ST_Islands` pour ne pas casser ces références. À trancher par `ue-editor-integrator` : renommer avec redirection, ou amender la convention.
- **Namespace** : celui de l'asset par défaut. L'API Python ne permet pas de le changer ; il est relevé dans le rapport du script.
- **CSV** : `docs/narrative/ST_Islands.csv`, UTF-8 avec BOM, fins de ligne CRLF, colonnes `Key`, `SourceString` (FR), `DevNotes` (rôle, longueur maximale, EN proposé). Espaces insécables (U+00A0) avant « : », « ; », « ? », « ! », dans les guillemets « » et après « n° », posées par le générateur.
- **Commandes** (depuis la racine du dépôt, éditeur inutile) :
  ```
  python scripts/editor/island_names_ue.py --write-csv   # régénère le CSV depuis ce document, puis le contrôle
  python scripts/editor/island_names_ue.py --check       # contrôle seulement ; rapport monke_island/Saved/T-092/island-names-check.json
  ```
- **Dans l'éditeur** : action `islandnames` de la session du lead (`scripts/editor/lead_session_t186.py`), verrou de l'éditeur tenu, aucune partie en cours. Le script refait le contrôle et sauvegarde la table actuelle (`Saved/T-092/ST_Islands.before.csv`). Il remplace alors les entrées par celles du CSV, en import complet : les clés `Marae` disparaissent. Ensuite, il pointe `DisplayName` des DA 01 à 20 sur `Island.<FragmentId>.Name`. Rien n'est sauvegardé si rien ne change. Rapport : `monke_island/Saved/T-092/island-names.json`.
- Les clés sont stables : ne jamais renommer une clé publiée, en ajouter une nouvelle. Les textes peuvent changer.

| Clé | FR | EN |
|---|---|---|
| `Island.UI.CrownNameFormat` | Registre de la Couronne : {CrownName} | Crown register: {CrownName} |
| `Island.UI.RumorFormat` | Rumeur : {Rumor} | Rumour: {Rumor} |
| `Island.UI.VillagesFormat` | Villages : {Riches}, {Wood}, {Food} | Villages: {Riches}, {Wood}, {Food} |
| `Island.UI.Destination` | Destination : {Name} | Destination: {Name} |
| `Island.UI.DayEndIsland` | Île quittée à temps : {Name} | Island left in time: {Name} |
| `Island.Theme.Riches` | Richesses et crique d'extraction | Riches and extraction cove |
| `Island.Theme.Wood` | Bois et passage court | Timber and shortcut |
| `Island.Theme.Food` | Nourriture et abri | Food and shelter |

## 5. Suites (hors de cette tâche)
- **Lead** : lancer l'action `islandnames`, lire le rapport. Preuve attendue : `ST_Islands` à 188 clés, sans clé `Marae` ; `DisplayName` des vingt DA résolu (« Le Peigne » pour FRG_06) ; seconde exécution sans aucune sauvegarde.
- **`ue-gameplay-programmer`** : propriétés d'identité du DA et affichage des villages par thème (3.6).
- **`ue-ui-developer`** (T-172, T-138, T-173) : carte d'arrivée, chargement, bilan (section 3) ; UI12 en FR et en EN.
- **`ue-game-designer`** : valider les vingt rumeurs contre les tables de butin ; T-139 (a) pour la rumeur de FRG_01.
- **Générateur des villages** (`scripts/map/fragment_villages.py`, `NAMES`) : reprendre les 60 noms de la section 2, pour que le nom de travail, les journaux et les captures disent le même nom que l'écran. `--check` liste les écarts restants. Les hameaux (`-sato`, non affichés) gardent des mots hors atoll (Yuki, la neige ; Momo, la pêche) : à revoir s'ils deviennent visibles.
- **`ue-editor-integrator`** : nom de l'asset (`ST_Islands` ou `ST_Text_Islands`, section 4).
