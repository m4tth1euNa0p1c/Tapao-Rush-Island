# 63 — Variété visuelle des 20 fragments
Domaine : G · Auditeur : environment artist

## Verdict
Sur la carte, les 20 fragments se distinguent (13 rubans « motu scindé », 4 naturels, 3 archipels) ; à hauteur d'yeux, peu : même palette (flore, sol, villages), même cadrage d'arrivée, et c'est surtout l'heure de départ aléatoire (6,5–18 h) qui renouvelle l'image. Sur 10 manches : 8,3 îles vues, une rejouée dans 90 % des cas (simulation) : monotonie probable dès la 3e–4e manche (supposé). Risque commercial : l'avis « même île × 20 », car « îles variées » attend L8 (cahier §11).

## Constats
- **[Majeur] Flore et sol : une seule palette**
  - Constat : seule la cocoteraie varie (0–57 %) ; fourré et sable : écart-type 7 points. FRG_01 : naupaka 35 %, fougères 19 %, cocotiers adultes 5 %. UE n'a que 4 couches de sol sur 7. Un atoll n'a pas de biomes : variez par faciès.
  - Preuve : fragments.json `shares`, FRG_Raraka_01/vegetation.json (calculés) ; Environment/Ground/Textures.
  - Recommandation : faciès (côté océan, côté lagon, plantation, forêt) réglés à la génération (`fragment_vegetation.py`) : naupaka de 15 à 45 %, le reste vers Miki-miki, Pandanus, Tahinu (TikiRules.h:347 ; vérifier la traversabilité) ; couches Wrack, Soil, Rubble déjà produites ; teinte de sable par île.
  - Effort : M

- **[Majeur] Villages, marae, camps : un gabarit chacun**
  - Constat : 46 villages sur 60 ont exactement 16 maisons, rayon 90 m, mesh `SM_Minka` unique ; 18 zooms vus : route, deux rangées, place ronde. Marae : une disposition ×2 par fragment ; camps : un décor.
  - Preuve : villages/fragments/index.json (calculé), preview_*.png (vu ; aperçus de fragments 02–20 = pointeurs LFS) ; MaraeRules.h:63,83 ; CampRules.h:104.
  - Recommandation : 3 gabarits de village (rue, grappe, pilotis sur le platier), 2 palettes de toit, un état ruiné ; 2–3 variantes de marae et de camp (`VariantSeeds`).
  - Effort : M

- **[Majeur] Premier regard standardisé ; côté océan = côté lagon**
  - Constat : 17 arrivées sur 20 sont à 274–276 m de la plage (milieu de la règle), un seul point par fragment. Deux arrivées « océan » (FRG_19, 20) ressemblent à un lagon : eau calme, ligne de cocotiers ; houle unique.
  - Preuve : fragments.json `arrival` (calculé) ; rules.json:79 ; captures T-095/01 et 02 (vu).
  - Recommandation : distance par seed (150–600 m), 2–3 arrivées par fragment ; côté océan : houle ×2, écume, blocs de corail ; côté lagon : eau calme, sable.
  - Effort : S (arrivées), M (littoral)

- **[Majeur] Aucun repère, épave ni ruine ; l'horizon est vide**
  - Constat : motu à 2–4 m : rien ne dépasse la canopée (tour de guet 9 m, cocotiers 19–23 m) ; à 2 m d'œil, l'horizon de FRG_01 n'est que des filets. Aucun asset d'épave, ruine ou balise. Pourtant 7 rumeurs sur 8 promettent caisses ou épave de la Couronne.
  - Preuve : horizon_pano.png (vu) ; recherche Content, art ; islands.md:105-159.
  - Recommandation : une « relique de la Couronne » par île, tirée du `CrownName` (barque échouée, poste du géomètre, mairie inachevée, épave de l'amiral), visible depuis l'arrivée ; kitbash de SM_Barque et du galion.
  - Effort : M

- **[Majeur] Ciel, brume, mer : seule l'heure varie, et globalement**
  - Constat : l'heure aléatoire existe (la moitié des expéditions croisent la nuit, voir 15) mais sans lien avec l'île ni le contrat. Un VolumetricCloud fixe dans la carte, sans code ; ni pluie ni brume variable ; aucun champ d'environnement dans `UExpeditionIslandDefinition`.
  - Preuve : DayNightRules.h:48-55 ; scan de Lvl_Raraka.umap, grep Source ; ExpeditionIslandDefinition.h:41-79.
  - Recommandation : `UEnvironmentProfile` (DA) par île et contrat, applicable dès maintenant : heures, nuages, brume, houle, vent ; 6 préréglages (clair, voilé, couvert, brume, grain, houle).
  - Effort : M

- **[Majeur] Autres atolls : levier fort, mais bridé**
  - Constat : le pipeline accepte une autre emprise (`fetch_tefenua.py <bbox>`) ; mais la submersion (jusqu'à +21,5 m) suppose des terres basses (pas d'île haute), et `build_terrain.py` dérive de l'imagerie Maxar « jamais embarquée ».
  - Preuve : fetch_tefenua.py:1-5 ; ExpeditionRules.h:47-53 ; rapport 38.
  - Recommandation : un 2e atoll de morphologie différente (passe ouverte, motu larges) après avis juridique ; ne générer les 19 autres cartes qu'avec faciès, gabarits et relique : chaque carte gèle la recette.
  - Effort : L

## Manques
1. Profil d'environnement par île et contrat.
2. Relique unique par île ; 3–5 îles héros à la main.
3. Variantes de village, marae, camp ; état ruiné.
4. Faune d'ambiance, fumée, embruns, pluie : aucun asset, aucun Niagara (grep).

## Quick wins
1. Mémoire des 8 dernières îles dans `ExpeditionIslands::PickIndex` : 8,3 → 9,9 îles distinctes sur 10 manches (voir 10).
2. `StartHours` par île dans `UExpeditionIslandDefinition`, lu par `ADayNightCycleActor`.
3. Arrivée : distance par seed (`rules.json`), puis `--regen` et `setup_fragments.py --part arrivals`.
4. Épave prototype : SM_Barque couchée et trois caisses devant l'arrivée de 5 fragments.

## Questions qui challengent l'équipe
1. Combien d'îles un joueur sait-il décrire après 10 manches ? Cinq îles héros faites à la main (cahier §1) ne valent-elles pas vingt découpes ?
2. Un atoll réel est plat : qui, sur Steam, saura le décor fidèle, et le paie-t-on en variété ?
3. Avec une semaine : 19 cartes identiques, ou le profil d'environnement et les reliques qui les différencient ?
