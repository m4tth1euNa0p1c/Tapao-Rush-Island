# Feature : fin de jour, avis de contrat et jour perdu

- **Statut** : review. Code écrit et **compilé fichier par fichier** (UBT `-SingleFile`, éditeur ouvert, 0 erreur). Specs écrites, **non exécutées**. **Rien testé en éditeur, en PIE ni en réseau.** Le script éditeur `dayendux` n'a pas encore été lancé.
- **Owner** : `ue-ui-developer`
- **Date** : 2026-10-10
- **Tâches** : T-173 (écrans de fin de jour et de contrat, jour perdu) et T-283 (minuteur sans le jour).
- **Contrat** :
  - ADR-007 : §7 (« JOUR N TERMINÉ »), §9 et §10 (contrats), §14 (« équipage entier mort : le jour est consommé »), §16 (cupidité) ;
  - cahier : §16 (menus), §19 (HUD, bilan) ;
  - lots : L4 (partie), L5 (HUD) ;
  - recette visée : RUN07 (équipage entier mort), RUN08 à RUN10 (contrat, défaite), §12 n°4.

## 1. Intention
Une campagne dure 10 jours, avec trois contrats (5 500 au jour 3, 10 500 au jour 6, 20 500 au jour 10).
- **Mourir tous ensemble fait perdre le jour, pas la partie.** Le butin reste sur l'île, le jour est consommé, on passe au jour suivant. Avant T-173, le joueur voyait « NAUFRAGÉ » et « RECOMMENCER », et le journal disait « game over, restart in 30 s ».
- **La fin d'un contrat est un moment** : le Bureau rend son avis.
  - « CONTRAT TERMINÉ » quand le palier est atteint, « CAMPAGNE HONORÉE » au dernier.
  - « CONTRAT ÉCHOUÉ » quand le quota manque au jour jalon.
- **Un seul compte des jours, celui de la campagne** (T-283). Le HUD, l'arrivée et les avis disent « Jour 5 ». Le minuteur en haut à droite ne garde que l'horloge.

## 2. Ce qui suit la fin d'un jour (serveur)
Le serveur décide (`AExpeditionGameState`). La règle pure est `ExpeditionDayEnd::Plan` (`Expedition/Core/ExpeditionDayEnd.h`). Elle est testée par `monke_island.Expedition.Clock`, rubrique « Lost day ».

| Fin du jour | Verdict du contrat (`FRunContractCalculator::EndDay`) | Suite | Attente avant le voyage |
|---|---|---|---|
| survivants à bord | jour suivant, contrat terminé ou échoué (règle d'exécution inactive), victoire | île suivante | `DA_ExpeditionRules.DayEndTravelDelay` (10 s) |
| survivants à bord | question de cupidité | île suivante | 10 s + `DA_RunRules.GreedChoiceSeconds` (15 s) |
| survivants à bord | quota manqué au jour jalon, règle d'exécution active | exécution (`AExecutionScene`), pas de voyage | — |
| **équipage entier mort** | tout verdict sauf l'exécution | **jour perdu**, île suivante | `DA_ExpeditionRules.GameOverRestartDelay`, nommé « Lost Day Travel Delay » dans l'éditeur (le DA vaut 30 s aujourd'hui) |
| équipage entier mort | question de cupidité (quota déjà atteint, jours restants) | jour perdu, île suivante | délai du jour perdu + 15 s |
| équipage entier mort | quota manqué au jour jalon, règle d'exécution active | **campagne perdue**, sans scène (personne à exécuter) et sans voyage | — |

- L'hôte peut **avancer le voyage** :
  - après sa réponse à la question de cupidité ;
  - après un jour perdu (bouton JOUR SUIVANT).
  - Le voyage part alors dans `UTapaoDayUISubsystem::TravelAfterChoiceSeconds` (3 s), par `AExpeditionGameState::HurryTravel`.
- Le journal serveur dit :
  - « every player is dead at … s: the day is lost, next island in … s » ;
  - « … on the last day of the contract, quota missed: the campaign is lost (nobody left to execute, no travel) » ;
  - « the travel to the next island is hurried, it leaves in 3.0 s ».
- `IsExecution()` veut dire « campagne perdue, pas de voyage ». La scène n'est jouée que s'il reste des vivants. Après une campagne perdue, `GetNextTravelTime()` vaut −1 et le minuteur se cache.

## 3. Écrans (Slate TAPAO)
L'écran est `STapaoDayEnd`, géré par `UTapaoDayUISubsystem` à l'ordre Z 870. Ses textes viennent de `TapaoDayEndText` : fonctions pures, testées, LOCTEXT de l'espace `TapaoDayEnd`.

Le numéro du jour est celui de la **campagne** : jours joués, à la fin du jour.

| Avis | Quand | Place | Contenu |
|---|---|---|---|
| **JOUR N TERMINÉ** | jour survécu, contrat en cours | à gauche ; la scène de départ du galion est au centre ; 9 s (`SummarySeconds`) | zone ; « QUOTA : x / y couronnes » en grand ; valeur ramenée ; morts et frais ; trésorerie ; « Jour suivant : zone. Échéance du contrat : jour 6. » |
| **JOUR N PERDU** | équipage entier mort | **au centre, sur un voile**, jusqu'au voyage | « Tout l'équipage est mort : le butin reste sur l'île. » ; « Tu es mort : cause » (propre à chaque joueur ; NAUFRAGÉ la portait avant, T-135) ; morts et frais ; quota ; jour suivant ; « Jour suivant dans 25 s » (temps serveur) ; hôte : bouton **JOUR SUIVANT** ; autres joueurs : « Le capitaine (l'hôte) peut lancer le jour suivant. » |
| **CONTRAT TERMINÉ** | palier atteint au dernier jour, ou « quitter la région » | comme le jour (à gauche, ou au centre si le jour est perdu) | en-tête « BUREAU DES POSSESSIONS LOINTAINES » ; « zone · jour 3 terminé » ; quota ; « Excédent versé au Trésor : +x couronnes » ; « Le Bureau constate que le contrat est rempli. Nouvelle destination : … » |
| **CAMPAGNE HONORÉE** | dernier palier atteint | idem | « Le Bureau constate que la campagne est honorée. La Couronne est satisfaite. Provisoirement. » |
| **CONTRAT ÉCHOUÉ**, la campagne continue | quota manqué au jour jalon, règle d'exécution inactive | idem | « Il manque x couronnes au contrat. » ; « Retenue sur la trésorerie : y couronnes (50 %) » ; « Le Bureau constate le défaut. Retour vers : … Les améliorations du galion restent acquises. » |
| **CONTRAT ÉCHOUÉ**, la campagne est perdue | après la scène d'exécution, ou 8 s après un équipage mort au jour jalon (`LostCampaignScreenSeconds`) | écran de fin (`STapaoEndScreen`, Z 920, ruban vierge) | ruban « CONTRAT ÉCHOUÉ » ; « Avis de déchéance : zone » ; « x / y couronnes : il manque z couronnes. » ; « L'équipage est déclaré traître et remis au peloton français. » (ou « à titre posthume ») ; « La campagne s'achève au jour 3. » ; statistiques ; RECOMMENCER (hôte, nouvelle campagne) et MENU |
| **Question de cupidité** | quota atteint avec des jours restants | à gauche (au centre si le jour est perdu) | « QUOTA ATTEINT : quitter la région, ou tenter un jour de plus ? » ; hôte : **QUITTER LA RÉGION** / **UN JOUR DE PLUS** ; autres joueurs : « Le capitaine (l'hôte) décide : partir ou rester… » ; pour tous : « Un jour de plus : butin +50 %, danger +40 %. Sans réponse, l'équipage quitte la région. » (lu dans `DA_RunRules`) et « Réponse attendue dans 12 s » |

- **Pas d'avis pendant l'exécution.** La scène a ses titres (« Quota non atteint », « La campagne s'achève ici »). L'écran de fin vient après.
- **NAUFRAGÉ** ne reste que sur une carte sans campagne : aucun état de partie rempli (`TapaoDayEndText::IsCampaignUnderWay`).
- **Accessibilité** :
  - textes de 20 px et plus à 1080p (15 à 46 pt) ;
  - l'état est toujours écrit : la couleur double l'information (rouge pour perdu ou échoué, or pour atteint), elle ne la porte jamais seule ;
  - les boutons se prennent au clavier et à la manette (focus Slate) ;
  - le texte se coupe dans 640 px.
- **Registre** (`docs/narrative/BIBLE.md`) : les avis de contrat sont au registre Bureau (impersonnel, chiffres exacts, ni « ! » ni calembour), les décomptes au registre Bord.

### HUD (T-283)
- **En haut à gauche**, en petit (`STapaoHUD`) :
  - « Jour 2 / 10 » ;
  - « Quota 9 648 / 5 500 c » ;
  - **« Contrat : jour 3 »**, avec « · jour de cupidité » le cas échéant (`TapaoDayEndText::ContractLine`). L'échéance est comptée en jours de campagne.
- **En haut à droite** : l'horloge seule. Le jour et le quota devant l'horloge ne reviennent qu'avec `DA_HudTimerRules.bShowRunDayInClock` (désactivé par défaut).

## 4. Réseau
- Le serveur décide de tout :
  - fin du jour, jour perdu, verdict ;
  - réponse à la cupidité : l'hôte joue sur le serveur et appelle `URunSubsystem::Choose` ;
  - avance du voyage : `HurryTravel`, autorité seulement.
- Les clients lisent l'état répliqué (`URunStateComponent` : rapport du jour, question ouverte, jour de cupidité ; `AExpeditionGameState` : jour perdu, campagne perdue, heure du voyage) et l'affichent. Les boutons n'existent que chez l'hôte.
- Aucun nouveau RPC ni nouvelle propriété répliquée. `DayEndTravelWait` (répliqué) porte maintenant aussi l'attente du jour perdu.

## 5. Valeurs
| Valeur | Où | Défaut |
|---|---|---|
| Attente du jour perdu | `DA_ExpeditionRules.GameOverRestartDelay` (« Lost Day Travel Delay ») | 8 s (classe) ; 30 s dans le DA |
| Attente d'un jour survécu | `DA_ExpeditionRules.DayEndTravelDelay` | 10 s |
| Temps de la cupidité, butin, danger, réponse par défaut | `DA_RunRules.GreedChoiceSeconds`, `GreedLootScale`, `GreedDangerScale`, `bGreedDefaultStay` | 15 s, ×1,5, ×1,4, partir |
| Part de la trésorerie perdue (contrat échoué) | `DA_RunRules.FailedTreasuryLoss` | 50 % |
| Jour et quota devant l'horloge | `DA_HudTimerRules.bShowRunDayInClock` | non |
| Durée du bilan d'un jour survécu, voyage après un choix de l'hôte | `UTapaoDayUISubsystem` (`DefaultGame.ini`) : `SummarySeconds`, `TravelAfterChoiceSeconds` | 9 s, 3 s |
| Écran de fin après un équipage mort au jour jalon | `UTapaoUISubsystem.LostCampaignScreenSeconds` (`DefaultGame.ini`) | 8 s |

## 6. Tests
- `monke_island.UI.DayEnd` (nouveau, `UI/Tests/TapaoDayEndSpec.cpp`) :
  - titres au jour de campagne ;
  - jour perdu : la campagne continue, JOUR SUIVANT, butin perdu, cause de la mort du joueur local ;
  - question de cupidité ouverte après un jour perdu ;
  - décomptes ;
  - pas de NAUFRAGÉ en campagne ;
  - avis de contrat terminé, honoré, échoué, exécution (avec ou sans survivants) ;
  - lignes de l'écran de la campagne perdue ;
  - verdict après la réponse de cupidité, texte de cupidité lu dans le DA ;
  - échéance du contrat (jours 3, 6 et 10).
- `monke_island.Expedition.Clock`, rubrique « Lost day (T-104, T-173) » (remplace « Game over ») :
  - définition de l'équipage entier mort ;
  - jour perdu : île suivante quel que soit le verdict, jamais de fin de partie ;
  - attentes, avec la cupidité ;
  - campagne perdue seulement au jour jalon : exécution, ou rien sans survivants ;
  - jamais d'attente négative ;
  - défauts du DA.
- `monke_island.UI.FloodTimer` :
  - horloge seule par défaut ;
  - « jour perdu » à la place de « game over » ;
  - minuteur caché quand aucun voyage ne suit.

## 7. Vérification en PIE (après le build complet et l'action `dayendux`)
Sur une île de campagne, en solo puis en hôte + client. Les commandes serveur se tapent dans la fenêtre de l'hôte.

| Capture | Étapes |
|---|---|
| HUD sans doublon (T-283) | Arrivée sur l'île. En haut à gauche : « Jour N / 10 », « Quota … c », « Contrat : jour 3 » (« Contrat : jour 6 » après `monke.Run.Day 4` et l'île suivante). En haut à droite : l'horloge seule (« 8:22 »). « · jour de cupidité » apparaît sur l'île qui suit UN JOUR DE PLUS (quota atteint avant le dernier jour du contrat). |
| JOUR PERDU (vrai flux) | `monke.Run.Day 2` (jour 2, quota non atteint), puis `monke.Death.Kill 0` en solo, ou `monke.Death.Kill 0` puis `monke.Death.Kill 1` à deux. « JOUR 2 PERDU » au centre, « Jour suivant dans 30 s », bouton JOUR SUIVANT chez l'hôte, voyage 3 s après le clic. Journal : « the day is lost, next island in 30.0 s ». |
| CONTRAT ÉCHOUÉ, campagne perdue (vrai flux) | `monke.Run.Day 3`, rester sur le pont du galion, `monke.EndDayAtHelm` : scène d'exécution, puis ruban « CONTRAT ÉCHOUÉ » et avis de déchéance. Variante sans survivants : `monke.Run.Day 3` puis `monke.Death.Kill 0` : « JOUR 3 PERDU » au centre, puis l'écran de fin 8 s après (« à titre posthume »), sans voyage. |
| CONTRAT TERMINÉ | Vrai flux : 5 500 c de butin à bord avant la fin du jour 3 (`monke.Run.Day 3`), ou au jour 1 puis QUITTER LA RÉGION. Aperçu local : `monke.UI.DayEnd Complete`. |
| Aperçus (cette machine seulement, rien ne change sur le serveur, hors Shipping) | `monke.UI.DayEnd Next`, `Choose`, `Complete`, `Victory`, `Failed`, `Executed`, avec `Lost` en second mot pour un jour perdu (`monke.UI.DayEnd Next Lost`) ; `monke.UI.DayEnd Hide` ferme l'aperçu. `Executed` sans `Lost` montre l'écran de fin de la campagne perdue. |

## 8. Preuves et statut honnête
- **Compilé** (2026-10-10, éditeur ouvert, UBT `-SingleFile`, un fichier à la fois, 0 erreur), avec le code UHT régénéré :
  - `TapaoDayEndText.cpp`, `STapaoDayEnd.cpp`, `TapaoDayUISubsystem.cpp`, `TapaoUISubsystem.cpp`, `STapaoHUD.cpp`, `STapaoScreens.cpp` ;
  - `FloodTimerWidget.cpp`, `FloodTimerMath.cpp`, `HudTimerRules.cpp`, `FloodTimerSubsystem.cpp`, `FloodTimerSource.cpp`, `SFloodTimer.cpp` ;
  - `ExpeditionGameState.cpp`, `DayTransitionSubsystem.cpp`, `TapaoDeathUISubsystem.cpp`, `DeathSpectatorComponent.cpp`, `IslanderHUD.cpp`, `SettingsSubsystem.cpp` ;
  - specs `TapaoDayEndSpec.cpp`, `ExpeditionClockSpec.cpp`, `FloodTimerSpec.cpp`, `TapaoInputSpec.cpp`.
- **Pas fait** : édition de liens, build complet, exécution des specs, PIE solo, PIE hôte + client, build packagé.

## 9. Écarts et questions
- **Victoire** : après « CAMPAGNE HONORÉE », le galion part comme un jour ordinaire, et `URunSubsystem::BeginDay` lance une nouvelle campagne sans écran. Proposition (hors périmètre de T-173) : à la victoire, pas de voyage et un écran de fin « CAMPAGNE HONORÉE » avec NOUVELLE CAMPAGNE et MENU, comme pour la campagne perdue.
- **Attente du jour perdu** : 30 s dans `DA_ExpeditionRules`. Le script ne la change pas, et l'hôte peut avancer le voyage. À baisser (10 à 12 s, comme un jour survécu) si le propriétaire trouve l'attente longue.
- **Taille du HUD** : les lignes de quota et de contrat sont à 11 pt (environ 15 px à 1080p), sous les 18 px de la règle d'accessibilité, à la demande du propriétaire (« tout en plus petit », 2026-10-08). Proposition : 14 pt (environ 19 px).
- **Vote de l'équipage** pour la cupidité (ADR-007, écarts) : l'hôte décide encore seul.
- **« Frais médicaux » ou « frais de recrutement »** (BIBLE, décision 5) : non tranché, le texte actuel est gardé.
