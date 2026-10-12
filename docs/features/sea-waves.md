# Vagues de la mer et ambiance du rivage

**Lot :** L5 (ambiance), sur demande de l'utilisateur du 2026-10-03. Le niveau d'eau reste un paramètre global (`UIslandWaterSubsystem::SetSeaLevel`).

## Vagues (Gerstner)
- **Modèle :** somme de vagues de Gerstner. Références :
  - GPU Gems 1, chapitre 1, « Effective Water Simulation from Physical Models » (Finch, 2004) ;
  - dispersion en eau profonde ω² = g k (Tessendorf, « Simulating Ocean Water », 2001).
- **Valeurs (`DA_WaveRules`) :**
  - 6 trains de 24 m à 2,1 m de longueur d'onde, crête la plus haute à 37 cm ;
  - houle venant de l'est-sud-est ;
  - `Intensity` multiplie toutes les hauteurs ;
  - les rides plus courtes que 2 m restent dans les cartes de normales du matériau.
- **Même formule partout :**
  - `Water/WaveMath.h` en C++ ;
  - le code HLSL de `M_Water_Ocean` (`scripts/props/build_ocean_material.py`).
- **Hauteur de l'eau :**
  - `UIslandWaterSubsystem::GetWaterSurfaceAt` ajoute la hauteur des vagues ;
  - la barque (points de flottaison), le nageur, le requin et les corps suivent donc la mer dessinée ;
  - la crue locale reste prise en compte par le max.
- **Horloge :** temps serveur répliqué (`GetServerWorldTimeSeconds`). Toutes les machines voient la même mer, et le serveur fait foi pour la flottaison.
- **Rendu (`AWaveSurfaceActor`, jamais sur un serveur dédié) :**
  - une grille de 769 × 769 sommets suit la caméra : 25 cm entre sommets près d'elle, puis une croissance géométrique jusqu'à 40 km ;
  - chaque vague s'efface là où les cellules dépassent le huitième de sa longueur, ce qui évite le scintillement ;
  - les plans de mer statiques (tag `IslandWaterSurface`) sont masqués pendant que la grille est affichée ;
  - la grille reste à l'origine du monde, et c'est le matériau qui la décale sous la caméra (`WaveFollow` dans `MPC_Waves`). Auparavant, l'acteur se déplaçait à chaque image : le moteur voyait toute la mer glisser, et TSR et le flou de mouvement la faisaient baver et trembler pendant les déplacements (bug signalé le 2026-10-03) ;
  - le matériau est rendu des deux côtés, pour que la caméra d'un nageur qui passe sous une crête voie la surface par en dessous ;
  - `MPC_Waves` est alimenté à chaque image.
- **Matériau `M_Water_Ocean` :**
  - Single Layer Water avec les couleurs de `M_Water_Lagoon` ;
  - normales de Gerstner, plus deux cartes de normales de détail (`T_Water_N`) qui défilent en espace monde ;
  - écume sur les crêtes les plus hautes (bruit), plus rugueuse.
- **Console :** `monke.Waves 0` donne une mer plate (lu au démarrage du monde).

## Ambiance du rivage
- **Sons :**
  - `SW_Shore_Surf` : déferlement sur le sable, boucle de 48 s ;
  - `SW_Shore_Sea` : mer au large et récif, boucle stéréo de 32 s ;
  - tous deux synthétisés par `scripts/props/shore_sounds.py`, donc sans licence.
- **Placement :** `AShoreAmbienceActor` sonde le sol en anneaux jusqu'à 70 m et trouve la ligne de rivage la plus proche :
  - le déferlement joue en 3D sur ce rivage ;
  - la mer joue tout autour, selon la part d'eau alentour.
- **Volumes de `DA_ShoreAmbienceRules` :** baissés à la demande de l'utilisateur (0,25 / 0,12).
- **Console :** `monke.ShoreAmbience 0` coupe cette ambiance.

## Recette
- `monke_island.Water.Waves` : 5 tests :
  - hauteur sous le sommet dessiné ;
  - bornes ;
  - mer plate à intensité nulle ;
  - période de 3,9 s pour 24 m ;
  - grille et échantillonnage.
- `monke_island.Ambience.Shore` : 4 tests.
- **Compilé :** build éditeur, suite complète 189/189.
- **Testé éditeur :**
  - aperçu du matériau compilé (eau, reflets, écume) ;
  - journal PIE : « Waves: surface of 769 x 769 vertices, 6 trains, highest crest 37 cm ».
- **Non vérifié à l'œil en jeu :** flottaison de la barque dans les vagues, rendu à l'horizon, coût GPU de la grille (1,2 M triangles).
