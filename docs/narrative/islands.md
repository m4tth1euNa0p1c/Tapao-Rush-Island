# Îles d'expédition : noms et identités des fragments

| Champ | Valeur |
|---|---|
| Tâche | T-092 (feature `fragments`) |
| Lots | L3 : `FRG_Raraka_01` (première île, trois zones) · L4 : tableau de contrats du hub (§7, `BP_ContractBoard`) et écran de chargement (§16) · L8 : fragments 02 à 08 (« îles variées ») |
| Recette | UI12 (texte maximal sans troncature) ; écran de chargement « destination » (§16) ; n° 9 (deuxième expédition : aucune trace de l'île précédente, donc des noms distincts d'une île à l'autre) |
| Décisions | ADR-001 (E-03 : aucun lieu, village ou habitant réel dans la fiction), ADR-004 (fragments de Raraka séparés par la mer, Motutapu exclu) |
| Specs liées | `docs/features/fragments-tuamotu.md`, `docs/features/villages-japonais.md`, `docs/features/marae-des-perles.md`, `docs/features/characters-tiki.md` |
| String Table | `ST_Islands` (proposition ci-dessous, CSV d'import `docs/narrative/ST_Islands.csv`) |
| Statut | écrit, non intégré : aucun asset créé (pas d'éditeur dans cette tâche) |

La bible de l'univers (`docs/narrative/bible.md`) n'existe pas encore. Ce document fixe en attendant le registre des noms de lieux ; la bible devra le reprendre.

## 1. Règles de nommage

### 1.1 Principe : deux noms par île, aucun mot inventé « à la polynésienne »
Chaque fragment porte deux noms, qui racontent la satire à eux seuls.

1. **Nom affiché** (`Name`) : le nom que lui donnent ses habitants, **traduit en français** sous forme d'image simple (« La Longue Échine », « Les Pas-sur-l'Eau »). C'est le nom principal, en gros sur le contrat et l'écran de chargement.
2. **Désignation de la Couronne** (`CrownName`) : le nom administratif que lui ont collé les cartographes du Bureau royal, sans jamais demander leur avis aux insulaires. Numéros, parenthèses, officiels vaniteux, procédures en cours. Il s'affiche en petit, sous le nom, comme une étiquette de registre.

Pourquoi ce registre plutôt qu'un « nom insulaire inventé » :
- **Pas de faux pa'umotu.** Un mot fabriqué pour « sonner polynésien » est soit un cliché exotique, soit, par hasard, un vrai mot (nom de lieu, de lignée, mot sacré, grossièreté) qu'aucun de nous ne sait vérifier. Le traduire en français supprime ce risque : rien ne peut coïncider avec un toponyme réel des Tuamotu.
- **La satire vise la Couronne.** Le nom des habitants est sobre et juste ; le nom de la Couronne est ridicule. Le joueur lit d'abord le premier : on respecte l'île avant de rire du Bureau.
- **Cohérent avec la fiction.** Les villages des factions insulaires sont japonais (`villages-japonais.md`) et les créatures sont des tiki stylisés (`characters-tiki.md`) : la fiction est déjà un archipel imaginaire, pas une reconstitution des Tuamotu.
- **Lisible et traduisible.** Une image traduite se localise (EN : « The Long Spine ») ; un mot inventé ne se traduit pas et se prononce mal.

### 1.2 Nom affiché (`Name`)
- Une image concrète de l'île vue par ceux qui y vivent : relief, forme, eau, végétation, lumière, bruit. Article défini, 2 à 4 mots, **28 caractères au plus** (UI12).
- Interdit : tout toponyme réel (Raraka, Motutapu, passe Manureva, noms d'atolls ou de motu des Tuamotu, noms de marae réels), tout mot pa'umotu, tahitien ou hawaïen employé comme nom propre, les noms de divinités, les mots liés au sacré (tapu, atua, mana…), les clichés de carte postale (« paradis », « perle du Pacifique », « sauvage »).
- Deux îles ne partagent ni mot principal ni image (recette n° 9 : les joueurs doivent savoir qu'ils ont changé d'île).
- `Raraka` ne figure que dans l'identifiant technique `FRG_Raraka_NN` (origine des données carto) : il n'est **jamais** affiché.

### 1.3 Désignation de la Couronne (`CrownName`)
- Registre administratif français, ton de registre : « Possession n° 1 », « Îlot du Vice-Contrôleur Pembury », « Lagon réglementaire n° 5 ». 60 caractères au plus.
- Une parenthèse comique par nom au plus, qui moque la bureaucratie : procédure en cours, sceau attendu, nom contesté, décoration posthume.
- Officiels **fictifs** aux noms anglais génériques (Pembury, Pettifer, Fenwick) : jamais une personne réelle, historique ou vivante, ni un explorateur réel du Pacifique.
- Les institutions sont inventées et reviennent d'une île à l'autre : le **Bureau des Possessions lointaines**, le **Lord Cartographe**, le **Trésor**, l'**Amirauté**.
- Jamais d'humour aux dépens des habitants, de leur langue ou de leur foi. Le ridicule naît de la Couronne qui nomme, mesure et taxe ce qu'elle ne comprend pas.

### 1.4 Villages (trois par fragment, un par zone de pacification)
- Forme : `<mot japonais>-mura`, en romaji Hepburn sans macron, un seul mot avant `-mura`. 16 caractères au plus.
- Le mot dit le thème de la zone (cahier §6.2, « chaque zone ouvre un intérêt concret ») :

| Thème | Zone | Racines possibles (sens) |
|---|---|---|
| Richesses | richesses et crique d'extraction | Takara (trésor), Kogane (or), Kaigara (coquillages), Zeni (monnaie), Tama (joyau), Gin (argent), Shinju (perle), Kinko (coffre-fort) |
| Bois | bois et passage court | Mori (forêt), Yashi (cocotier), Take (bambou), Kodachi (bosquet), Hayashi (bois), Funaki (bois de navire), Kikori (bûcheron), Ita (planche) |
| Vivres | nourriture et abri | Kura (grenier), Ami (filet), Shio (sel), Uo (poisson), Imo (tubercule), Himono (poisson séché), Tsubo (jarre), Ido (puits) |

- Le mot reste plausible sur un atoll : pas de rizière (`kome`), de pin (`matsu`) ni de montagne (`yama`) ; la spec des villages exclut rizières et haies.
- Chaque nom de village est **unique dans tout le catalogue** : un village nommé dans une notification (« Zone sécurisée : Kogane-mura ») désigne une seule île.
- Éviter les noms de villes japonaises connues (Kurashiki, Takarazuka…). Un nom composé de cette façon peut coïncider avec un petit lieu-dit ou un nom de famille au Japon ; c'est accepté (ce n'est pas un lieu des Tuamotu), mais à vérifier en relecture de localisation japonaise si elle arrive un jour.
- Les noms restent en romaji dans toutes les langues ; la traduction anglaise ne touche que les textes qui les entourent.

### 1.5 Marae (un par fragment)
- Forme : « Marae de/des/du + nom commun français », image tirée de la nature de l'île ou de ses offrandes : « Marae des Perles », « Marae des Conques ». 28 caractères au plus.
- « Marae » reste le nom commun polynésien du lieu cérémoniel (déjà employé par `marae-des-perles.md`) ; on n'invente jamais de nom propre polynésien pour un marae et on ne reprend aucun marae réel (ni ses noms, ni ses légendes).
- Pas de nom de divinité, pas de « maudit », « sanglant », « des sacrifices » ni d'imagerie d'horreur : le marae est un lieu calme (zone sans tiki, `TikiQuietZone`), respecté par le jeu même quand la Couronne ne le respecte pas.
- Ton : le marae n'est jamais l'objet de la satire. Seule l'est l'obsession du Bureau à l'inventorier.

### 1.6 Vocabulaire géographique admis
On garde dans les textes les noms communs de la géographie des atolls, en usage courant dans le français de Polynésie et déjà définis dans les specs du projet : **motu** (îlot de l'anneau corallien), **hoa** (chenal peu profond entre deux motu), **lagon**, **platier**, **crique**. Toujours en minuscules, jamais comme nom propre, jamais au pluriel avec un « s » (« deux motu », « les hoa »).

### 1.7 Le butin des rumeurs
- Une rumeur (`Rumor`) promet ce que la Couronne a **elle-même** perdu ou laissé : caisses d'un caboteur royal, caisse d'un percepteur, argenterie d'un amiral, coffres d'une expédition précédente. Elle ne promet jamais de piller les habitants ou leurs lieux sacrés.
- La rumeur donne une indication honnête mais vague : elle oriente vers le village des richesses, une crique, le platier ou l'eau peu profonde. Elle ne promet pas un objet garanti : le loot reste seedé et configurable (§6.2). Le game designer valide chaque rumeur contre la table de loot du fragment.
- **Point de vigilance, marae des Perles (FRG 01).** La perle noire en or posée sur l'autel (1 500 $, `marae-des-perles.md`) fait du marae un lieu de pillage. Proposition narrative à valider par `ue-game-designer` : c'est **la Perle du Gouverneur**, un bijou de la Couronne abandonné là par une expédition précédente, que les insulaires ont laissé sur l'autel comme on laisse un avertissement. La reprendre, c'est récupérer le bien du Bureau, pas voler un objet sacré. Aucun changement de gameplay ; seul le nom de l'objet changerait (hors `ST_Islands`).

### 1.8 Longueurs (UI12, 1080p et texte maximal)
| Champ | Maximum |
|---|---|
| `Name` | 28 caractères |
| `CrownName` | 60 caractères |
| `Pitch` (une ligne du tableau de contrats) | 110 caractères |
| `Rumor` | 130 caractères |
| Village | 16 caractères |
| `Marae` | 28 caractères |

Les textes ci-dessous respectent ces maxima en français et en anglais.

## 2. Les huit identités

La géographie des fragments sera fixée par T-082 (`fragments.json`). Chaque identité porte donc un **profil attendu** plutôt qu'un emplacement : au gel du catalogue, on attribue à chaque `FRG_Raraka_NN` l'identité dont le profil correspond le mieux, puis on adapte l'accroche si besoin (règles de la section 1). Les candidats actuels (aperçu du 2026-10-03) : 6 fragments naturels de 8 à 50 ha, 2 grands motu à scinder par des hoa synthétiques, des chapelets d'îlots à regrouper en fragments archipel.

Pour chaque île, les trois villages sont donnés dans l'ordre **richesses / bois / vivres**, c'est-à-dire les rôles `Takara` / `Mori` / `Kura` de `place_villages.py`, quel que soit l'identifiant `V0x` que le générateur leur attribue.

### FRG_Raraka_01 : Les Trois Fumées (L3, `L_Frag_Raraka_01`)
- **Profil attendu** : fragment naturel moyen, 40 à 50 ha (F1 à F3 de l'aperçu) ; c'est l'ancien `L_Island_Prototype`. Île d'apprentissage.
- **Couronne** : Possession n° 1, dite « l'Échantillon ».
- **Accroche** : Contrat d'essai. Trois villages, un marae, et un Bureau qui veut des chiffres avant le thé.
- **Ambiance** : cocoteraie claire et plages franches ; au crépuscule, la fumée des trois villages monte droit dans l'air calme et sert de repère. Mer calme côté lagon. La nuit fait peur sans écraser : c'est la première.
- **Villages** : Takara-mura (richesses) · Mori-mura (bois) · Kura-mura (vivres). Noms déjà posés par `place_villages.py` : on les garde.
- **Marae** : Marae des Perles (existant, `marae-des-perles.md`).
- **Rumeur** : Une expédition aurait laissé la Perle du Gouverneur sur l'autel du marae. Le Bureau la croit perdue ; les gens d'ici savent où.

### FRG_Raraka_02 : La Longue Échine
- **Profil attendu** : moitié d'un grand motu coupée par un hoa synthétique ; long (2 à 2,5 km le long de l'anneau) et étroit. Les zones sont loin les unes des autres : la barque doit suivre.
- **Couronne** : Îlot du Vice-Contrôleur Pembury (nom contesté par Pembury).
- **Accroche** : Une île tout en longueur. La barque suivra, le Vice-Contrôleur aussi, sur le papier.
- **Ambiance** : une crête de cocotiers d'un bout à l'autre, vent régulier côté océan, longs trajets à découvert. La nuit, les deux bouts de l'île se perdent dans le noir.
- **Villages** : Kogane-mura · Yashi-mura · Ami-mura.
- **Marae** : Marae des Conques.
- **Rumeur** : Un caboteur royal aurait coulé sur le platier. Ses caisses portent encore le sceau de la Couronne, ce qui ne les allège pas.

### FRG_Raraka_03 : Les Pas-sur-l'Eau
- **Profil attendu** : fragment archipel, chapelet d'îlots de moins de 8 ha reliés par des hoa où l'on patauge.
- **Couronne** : Banc n° 3, non cadastré (le géomètre refuse de se mouiller).
- **Accroche** : Des îlots reliés par des gués. Pieds mouillés garantis, indemnité refusée d'avance.
- **Ambiance** : de l'eau partout, reflets, sable mouillé, bruit du courant dans les hoa. La submersion se lit tout de suite : chaque gué disparaît l'un après l'autre.
- **Villages** : Kaigara-mura · Take-mura · Shio-mura.
- **Marae** : Marae des Gués.
- **Rumeur** : Une expédition aurait semé ses coffres d'îlot en îlot en fuyant la marée. Son rapport parle d'un « repli ordonné ».

### FRG_Raraka_04 : Le Poing Fermé
- **Profil attendu** : petit fragment compact, 15 à 25 ha (F4, F5) ; trajets courts, fourré dense.
- **Couronne** : Possession de poche de Sa Majesté.
- **Accroche** : Petite île, gros ennuis. Le Trésor la déclare « très rentable au mètre carré ».
- **Ambiance** : fourré épais, villages serrés, vue courte. Tout est proche, y compris ce qui rôde ; la nuit y est la plus étouffante du catalogue.
- **Villages** : Zeni-mura · Kodachi-mura · Uo-mura.
- **Marae** : Marae du Vent Couché.
- **Rumeur** : Un percepteur royal s'y serait caché avec la caisse de l'impôt. On cherche toujours la caisse ; le percepteur, beaucoup moins.

### FRG_Raraka_05 : Le Miroir
- **Profil attendu** : fragment tourné vers le lagon, eau calme et peu profonde, crique d'extraction abritée ; convient à un fragment naturel ou à un morceau de grand motu côté lagon.
- **Couronne** : Lagon réglementaire n° 5 (eau jugée non conforme).
- **Accroche** : Un lagon si calme que le Bureau le soupçonne de cacher quelque chose. Pour une fois, il a raison.
- **Ambiance** : eau plate, ciel reflété, beau temps trompeur. La nuit, les lucioles se doublent dans l'eau ; la tempête finale tranche d'autant plus.
- **Villages** : Tama-mura · Hayashi-mura · Imo-mura.
- **Marae** : Marae des Eaux Lentes.
- **Rumeur** : Par temps clair, on verrait des caisses au fond du lagon, tombées d'une barque trop chargée. Le quota, lui, était respecté.

### FRG_Raraka_06 : Le Dos de la Houle
- **Profil attendu** : fragment exposé côté océan, platier large semé de blocs de corail ; accostage difficile, mouillages côté lagon.
- **Couronne** : Écueil honoraire Fenwick (décoré à titre posthume).
- **Accroche** : Côté large, la houle cogne. L'amiral Fenwick y a laissé son nom, et son navire avec.
- **Ambiance** : vent fort, embruns, ressac permanent, gros blocs de corail sur le platier. Le bruit de la mer couvre les pas : on se fait surprendre.
- **Villages** : Gin-mura · Funaki-mura · Himono-mura.
- **Marae** : Marae de la Houle.
- **Rumeur** : L'épave de l'amiral rendrait encore son argenterie à marée basse. L'Amirauté réclame la vaisselle, pas l'amiral.

### FRG_Raraka_07 : Les Fourrés Qui Écoutent
- **Profil attendu** : fragment boisé, peu de plage, beaucoup de fourré et de cocoteraie ; difficile, pensé pour la peur nocturne.
- **Couronne** : Terre provisoirement inhabitable (pétition en cours).
- **Accroche** : Le fourré entend tout. Le Bureau conseille de parler bas et de remplir les formulaires vite.
- **Ambiance** : fourré dense, lune faible, lucioles, craquements de branches. Les lanternes y valent leur prix (boutique §7).
- **Villages** : Shinju-mura · Kikori-mura · Tsubo-mura.
- **Marae** : Marae des Lucioles.
- **Rumeur** : Une expédition y a perdu ses lanternes, ses coffres et son courage. Les coffres, au moins, n'ont pas bougé.

### FRG_Raraka_08 : L'Anneau Brisé
- **Profil attendu** : le plus grand fragment retenu (proche de 80 ha), plusieurs hoa et criques, zones éloignées ; fin de catalogue, trajets en barque obligatoires.
- **Couronne** : Nouvelle-Pettifer (en attente du sceau depuis onze ans).
- **Accroche** : Lord Pettifer veut y fonder une ville à son nom. Commencez par en revenir.
- **Ambiance** : grande île ouverte, horizon de tous côtés, hoa à traverser, criques multiples. La plus longue à pacifier, la plus belle à quitter de justesse.
- **Villages** : Kinko-mura · Ita-mura · Ido-mura.
- **Marae** : Marae des Frégates.
- **Rumeur** : Le coffre-fort de la future mairie aurait été débarqué avant la mairie. Il attend toujours, plein.

## 3. String Table `ST_Islands` (proposition)

- Asset cible : `/Game/MonkeIsland/Localization/ST_Islands` (à créer par `ue-editor-integrator` via `StringTableTools`, import du CSV). Namespace : `Islands`.
- CSV d'import : `docs/narrative/ST_Islands.csv` (colonnes `Key,SourceString`, texte source français, UTF-8 avec BOM). Les espaces avant `:`, `;`, `!`, `?` et à l'intérieur des guillemets « » y sont insécables (U+00A0) ; la colonne EN sert à la future culture `en`.
- Utilisation : `UExpeditionIslandDefinition` référence `Island.<FRG>.Name` (nom affiché, spec fragments) ; `BP_ContractBoard` (L4) montre `Name`, `CrownName` via `Island.UI.CrownNameFormat`, `Pitch` et `Rumor` ; l'écran de chargement (§16) montre `Name` ; les notifications de zone et la carte montrent les villages et le marae.
- Les clés sont stables : ne jamais renommer une clé publiée, en ajouter une nouvelle.

| Clé | Texte FR | Texte EN |
|---|---|---|
| `Island.UI.CrownNameFormat` | Registre de la Couronne : {CrownName} | Crown register: {CrownName} |
| `Island.UI.RumorFormat` | Rumeur : {Rumor} | Rumour: {Rumor} |
| `Island.UI.VillagesFormat` | Villages : {Riches}, {Wood}, {Food} | Villages: {Riches}, {Wood}, {Food} |
| `Island.Theme.Riches` | Richesses et crique d'extraction | Riches and extraction cove |
| `Island.Theme.Wood` | Bois et passage court | Timber and shortcut |
| `Island.Theme.Food` | Vivres et abri | Food and shelter |
| `Island.FRG_Raraka_01.Name` | Les Trois Fumées | The Three Smokes |
| `Island.FRG_Raraka_01.CrownName` | Possession n° 1, dite « l'Échantillon » | Possession No. 1, known as "the Sample" |
| `Island.FRG_Raraka_01.Pitch` | Contrat d'essai. Trois villages, un marae, et un Bureau qui veut des chiffres avant le thé. | Trial contract. Three villages, one marae, and a Bureau that wants figures before tea. |
| `Island.FRG_Raraka_01.Rumor` | Une expédition aurait laissé la Perle du Gouverneur sur l'autel du marae. Le Bureau la croit perdue ; les gens d'ici savent où. | An expedition left the Governor's Pearl on the marae altar. The Bureau thinks it lost; the locals know where. |
| `Island.FRG_Raraka_01.Village.Riches` | Takara-mura | Takara-mura |
| `Island.FRG_Raraka_01.Village.Wood` | Mori-mura | Mori-mura |
| `Island.FRG_Raraka_01.Village.Food` | Kura-mura | Kura-mura |
| `Island.FRG_Raraka_01.Marae` | Marae des Perles | Marae of Pearls |
| `Island.FRG_Raraka_02.Name` | La Longue Échine | The Long Spine |
| `Island.FRG_Raraka_02.CrownName` | Îlot du Vice-Contrôleur Pembury (nom contesté par Pembury) | Vice-Comptroller Pembury Islet (name disputed by Pembury) |
| `Island.FRG_Raraka_02.Pitch` | Une île tout en longueur. La barque suivra, le Vice-Contrôleur aussi, sur le papier. | An island that is all length. The boat will follow, and so will the Vice-Comptroller, on paper. |
| `Island.FRG_Raraka_02.Rumor` | Un caboteur royal aurait coulé sur le platier. Ses caisses portent encore le sceau de la Couronne, ce qui ne les allège pas. | A royal coaster sank on the reef flat. Its crates still bear the Crown's seal, which does not make them any lighter. |
| `Island.FRG_Raraka_02.Village.Riches` | Kogane-mura | Kogane-mura |
| `Island.FRG_Raraka_02.Village.Wood` | Yashi-mura | Yashi-mura |
| `Island.FRG_Raraka_02.Village.Food` | Ami-mura | Ami-mura |
| `Island.FRG_Raraka_02.Marae` | Marae des Conques | Marae of Conches |
| `Island.FRG_Raraka_03.Name` | Les Pas-sur-l'Eau | The Water Steps |
| `Island.FRG_Raraka_03.CrownName` | Banc n° 3, non cadastré (le géomètre refuse de se mouiller) | Bank No. 3, unsurveyable (the surveyor refused to get wet) |
| `Island.FRG_Raraka_03.Pitch` | Des îlots reliés par des gués. Pieds mouillés garantis, indemnité refusée d'avance. | Islets linked by fords. Wet feet guaranteed, compensation refused in advance. |
| `Island.FRG_Raraka_03.Rumor` | Une expédition aurait semé ses coffres d'îlot en îlot en fuyant la marée. Son rapport parle d'un « repli ordonné ». | An expedition dropped its chests islet by islet while fleeing the tide. Its report calls it an "orderly withdrawal". |
| `Island.FRG_Raraka_03.Village.Riches` | Kaigara-mura | Kaigara-mura |
| `Island.FRG_Raraka_03.Village.Wood` | Take-mura | Take-mura |
| `Island.FRG_Raraka_03.Village.Food` | Shio-mura | Shio-mura |
| `Island.FRG_Raraka_03.Marae` | Marae des Gués | Marae of Fords |
| `Island.FRG_Raraka_04.Name` | Le Poing Fermé | The Closed Fist |
| `Island.FRG_Raraka_04.CrownName` | Possession de poche de Sa Majesté | The Crown's Pocket Possession |
| `Island.FRG_Raraka_04.Pitch` | Petite île, gros ennuis. Le Trésor la déclare « très rentable au mètre carré ». | Small island, big trouble. The Treasury rates it "highly profitable per square yard". |
| `Island.FRG_Raraka_04.Rumor` | Un percepteur royal s'y serait caché avec la caisse de l'impôt. On cherche toujours la caisse ; le percepteur, beaucoup moins. | A royal tax collector hid here with the tax chest. The chest is still being sought; the collector, far less so. |
| `Island.FRG_Raraka_04.Village.Riches` | Zeni-mura | Zeni-mura |
| `Island.FRG_Raraka_04.Village.Wood` | Kodachi-mura | Kodachi-mura |
| `Island.FRG_Raraka_04.Village.Food` | Uo-mura | Uo-mura |
| `Island.FRG_Raraka_04.Marae` | Marae du Vent Couché | Marae of the Resting Wind |
| `Island.FRG_Raraka_05.Name` | Le Miroir | The Mirror |
| `Island.FRG_Raraka_05.CrownName` | Lagon réglementaire n° 5 (eau jugée non conforme) | Regulation Lagoon No. 5 (water found non-compliant) |
| `Island.FRG_Raraka_05.Pitch` | Un lagon si calme que le Bureau le soupçonne de cacher quelque chose. Pour une fois, il a raison. | A lagoon so calm the Bureau suspects it of hiding something. For once, it is right. |
| `Island.FRG_Raraka_05.Rumor` | Par temps clair, on verrait des caisses au fond du lagon, tombées d'une barque trop chargée. Le quota, lui, était respecté. | On clear days, crates show on the lagoon floor, lost from an overloaded boat. The quota, at least, was met. |
| `Island.FRG_Raraka_05.Village.Riches` | Tama-mura | Tama-mura |
| `Island.FRG_Raraka_05.Village.Wood` | Hayashi-mura | Hayashi-mura |
| `Island.FRG_Raraka_05.Village.Food` | Imo-mura | Imo-mura |
| `Island.FRG_Raraka_05.Marae` | Marae des Eaux Lentes | Marae of Slow Waters |
| `Island.FRG_Raraka_06.Name` | Le Dos de la Houle | The Swell's Back |
| `Island.FRG_Raraka_06.CrownName` | Écueil honoraire Fenwick (décoré à titre posthume) | Honorary Fenwick Reef (decorated posthumously) |
| `Island.FRG_Raraka_06.Pitch` | Côté large, la houle cogne. L'amiral Fenwick y a laissé son nom, et son navire avec. | On the ocean side, the swell pounds. Admiral Fenwick left his name here, and his ship with it. |
| `Island.FRG_Raraka_06.Rumor` | L'épave de l'amiral rendrait encore son argenterie à marée basse. L'Amirauté réclame la vaisselle, pas l'amiral. | The admiral's wreck still gives up its silver at low tide. The Admiralty wants the tableware back, not the admiral. |
| `Island.FRG_Raraka_06.Village.Riches` | Gin-mura | Gin-mura |
| `Island.FRG_Raraka_06.Village.Wood` | Funaki-mura | Funaki-mura |
| `Island.FRG_Raraka_06.Village.Food` | Himono-mura | Himono-mura |
| `Island.FRG_Raraka_06.Marae` | Marae de la Houle | Marae of the Swell |
| `Island.FRG_Raraka_07.Name` | Les Fourrés Qui Écoutent | The Listening Thickets |
| `Island.FRG_Raraka_07.CrownName` | Terre provisoirement inhabitable (pétition en cours) | Provisionally Uninhabitable Land (petition pending) |
| `Island.FRG_Raraka_07.Pitch` | Le fourré entend tout. Le Bureau conseille de parler bas et de remplir les formulaires vite. | The thicket hears everything. The Bureau advises speaking softly and filling in forms quickly. |
| `Island.FRG_Raraka_07.Rumor` | Une expédition y a perdu ses lanternes, ses coffres et son courage. Les coffres, au moins, n'ont pas bougé. | An expedition lost its lanterns, its chests and its nerve here. The chests, at least, have not moved. |
| `Island.FRG_Raraka_07.Village.Riches` | Shinju-mura | Shinju-mura |
| `Island.FRG_Raraka_07.Village.Wood` | Kikori-mura | Kikori-mura |
| `Island.FRG_Raraka_07.Village.Food` | Tsubo-mura | Tsubo-mura |
| `Island.FRG_Raraka_07.Marae` | Marae des Lucioles | Marae of Fireflies |
| `Island.FRG_Raraka_08.Name` | L'Anneau Brisé | The Broken Ring |
| `Island.FRG_Raraka_08.CrownName` | Nouvelle-Pettifer (en attente du sceau depuis onze ans) | New Pettifer (awaiting the seal for eleven years) |
| `Island.FRG_Raraka_08.Pitch` | Lord Pettifer veut y fonder une ville à son nom. Commencez par en revenir. | Lord Pettifer wants a town named after him here. Start by coming back alive. |
| `Island.FRG_Raraka_08.Rumor` | Le coffre-fort de la future mairie aurait été débarqué avant la mairie. Il attend toujours, plein. | The future town hall's strongbox was landed before the town hall. It is still waiting, full. |
| `Island.FRG_Raraka_08.Village.Riches` | Kinko-mura | Kinko-mura |
| `Island.FRG_Raraka_08.Village.Wood` | Ita-mura | Ita-mura |
| `Island.FRG_Raraka_08.Village.Food` | Ido-mura | Ido-mura |
| `Island.FRG_Raraka_08.Marae` | Marae des Frégates | Marae of Frigatebirds |

Le CSV est généré depuis ce tableau (colonne FR) : en cas de modification, changer d'abord ce tableau, puis le CSV, avec les mêmes clés.

## 4. Intégration (hors de cette tâche)
- `ue-editor-integrator` : créer `ST_Islands` et importer le CSV (`StringTableTools`) ; vérifier l'affichage des « » et des espaces insécables.
- `ue-gameplay-programmer` (T-087) : le nom affiché de `UExpeditionIslandDefinition` pointe sur `ST_Islands` / `Island.<FRG>.Name` (`FText` issu de la String Table, jamais une chaîne en dur). Les noms des villages remplacent, à l'import du fragment, les noms techniques de `villages.json`, selon le rôle richesses / bois / vivres.
- `ue-ui-developer` (L4) : contrat = `Name` en titre, `CrownNameFormat` en sous-titre, `Pitch`, puis `RumorFormat` ; tester UI12 en FR et EN à taille de texte maximale.
- `ue-game-designer` : valider les rumeurs contre les tables de loot de chaque fragment, et la proposition « Perle du Gouverneur » (section 1.7).
- Au gel de T-082 : attribuer chaque identité à un `FRG_Raraka_NN` selon son profil et noter ici la correspondance.
