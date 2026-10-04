# HUD : compteur avant inondation (T-097)

Lot **L3** · cahier §6.1 (horloge commune), §19 (HUD) · recette **n°14** (« Zone validée à 75 s : échéance à 240 s ; seconde zone = 360 s. Tous affichent les mêmes valeurs »). UI08 (remappage clavier) ne concerne pas ce widget.

## Ce qui est affiché

Haut-centre de l'écran, dans la zone sûre, sans clic possible (HitTestInvisible).

```
             ┌───────────────────────────────┐
             │  Montée des eaux dans 1:45    │   avant la montée : texte sable, panneau sombre
             └───────────────────────────────┘
                  Zone sécurisée : +2:00           3 s après chaque bonus de zone (vert)

             ┌───────────────────────────────────────┐
             │ ~  Évacuation ! Extraction dans 0:42  │   pendant les 60 s : panneau bleu, icône d'eau, texte bleu clair
             └───────────────────────────────────────┘

             ┌───────────────────┐
             │ ~  Île submergée  │   fin de la montée
             └───────────────────┘
```

| État | Quand | Texte (LOCTEXT `MonkeFloodTimer`) |
|---|---|---|
| Caché | aucune source d'inondation active | — |
| Compte à rebours | avant l'échéance | `Montée des eaux dans {0}` ; couleur d'urgence sous `UrgentSeconds` (30 s) |
| Évacuation | phase serveur ≥ Submerging | `Évacuation ! Extraction dans {0}` + icône d'eau |
| Terminé | phase Resolved / fin de la montée | `Île submergée` |
| Notification | `OnFloodDeadlineChanged` avec une échéance plus tardive (pas au démarrage) | `Zone sécurisée : +{0}` (durée réelle gagnée, +2:00 par zone) |

Temps au format `m:ss`, arrondi au-dessus (0:00 n'apparaît que quand le temps est écoulé), minutes non plafonnées (8:00).

**Alertes** à 60 / 30 / 10 s avant la montée : son bref + pulsation (échelle et éclair du texte). Chaque seuil sonne **une seule fois par expédition**, même si un bonus repousse l'échéance au-dessus (cahier §19). Un saut sur plusieurs seuils (à-coup, commande de debug) ne donne qu'une alerte, la plus urgente. Un joueur qui arrive en cours de partie n'entend pas les seuils déjà passés. L'option `bRearmAlertsAfterExtension` réarme les seuils après un bonus, une fois chacun (désactivée par défaut).

Accessibilité : l'évacuation se distingue par le texte et l'icône, pas seulement par la couleur ; l'alerte est à la fois sonore et visuelle. Police 24 pt (32 px à 1080p, 21 px à 720p, avec la courbe DPI du moteur), contour et ombre noirs sur panneau sombre. Le menu des réglages (T-073) n'a pas encore d'échelle d'interface (§18 : 80–150 %) : le compteur suit la courbe DPI du moteur.

**Jour de la période (ADR-006, T-141)** : quand le GameState porte un `URunStateComponent` (`Run/`, T-109/T-113), la ligne commence par `Jour N/5` (LOCTEXT `RunDay`, « Jour {0}/{1} », 5 = `DaysPerPeriod` de `DA_RunRules`), par exemple `Jour 2/5 · 1:45`. Sans partie en cours (pas de composant), la ligne reste inchangée. Le texte est calculé par `FloodTimerMath::FormatDay` et `ComposeClockLine` (purs, testés dans `monke_island.UI.FloodTimer.Day`) et exposé par `UFloodTimerWidget::GetDayText()`. Le jour est relu à chaque rafraîchissement, depuis l'état répliqué : hôte et clients affichent le même. Dépendance volontairement optionnelle : `Run/` (monk-island-78) n'est pas encore commité. `FloodTimerWidget.cpp` n'inclut `Run/RunStateComponent.h` que si le fichier existe (`__has_include`, macro `MONKE_FLOOD_TIMER_HAS_RUN`). Sans `Run/`, le compteur compile et n'affiche pas de jour. Aucun en-tête réfléchi ne dépend de `Run/`. Une fois `Run/` commité, la condition peut être retirée.

## Source du temps

Le compteur n'a pas de minuterie propre : chaque rafraîchissement relit la source (`FFloodTimerSource`).

1. **Horloge d'expédition** (contrat §6.1), prioritaire dès qu'elle tourne : `AExpeditionGameState` — `GetServerWorldTimeSeconds()`, `GetFloodStartTime()`, `GetSubmersionEndTime()`, phase répliquée. Un client ne passe en évacuation que quand la phase répliquée l'indique (un bonus peut être en route) ; avant, il reste bloqué à 0:00.
2. **Acteur d'inondation autonome** (`AFloodSimulationActor`), si `bShowStandaloneFloodSource` : même horloge que l'acteur, l'horloge jour/nuit répliquée (`ADayNightCycleActor::GetElapsedSeconds`, dérivée du temps serveur), échéance `DA_FloodRules.StartSeconds`, fin `StartSeconds + RiseSeconds`. C'est la source qui fait réellement monter l'eau dans `Lvl_Raraka` aujourd'hui.

Depuis T-098, l'acteur d'inondation suit l'horloge d'expédition dès que la map en a une (`DA_FloodRules.ClockMode = Auto`). Le compteur ne le montre alors qu'à travers l'expédition, et la source 2 ne sert plus qu'aux maps sans expédition. L'avertissement « deux horloges » ne subsiste que si `ClockMode = DayNightOnly` est forcé dans une map où une expédition tourne.

## Architecture

| Fichier | Rôle |
|---|---|
| `UI/HudTimerRules.h` | `UHudTimerRules` (futur `DA_HudTimerRules`) : seuils, durées, tailles, couleurs, sons, SoundClass, icône |
| `UI/FloodTimerMath.h` | logique pure testée : `Evaluate`, `FormatClock`, `IsZoneBonus`, `FFloodTimerPresenter` (modes, alertes uniques) |
| `UI/FloodTimerSource.h` | lecture de l'horloge réelle (expédition, sinon acteur d'inondation) |
| `UI/SFloodTimer.h` | vue Slate, sans asset |
| `UI/FloodTimerWidget.h` | `UFloodTimerWidget` (UUserWidget, `DisableNativeTick`) : rafraîchissement par minuterie (0,2 s) + délégués `OnFloodDeadlineChanged` / `OnPhaseChanged`, sons, événements Blueprint |
| `UI/FloodTimerSubsystem.h` | `UFloodTimerSubsystem` (UWorldSubsystem) : crée le widget pour chaque joueur local quand une source est active ; jamais sur serveur dédié |
| `UI/Tests/FloodTimerSpec.cpp` | `monke_island.UI.FloodTimer.*` |

Création : un sous-système de monde plutôt qu'un ajout dans `Amonke_islandPlayerController` ou `AIslanderHUD`. Il fonctionne quel que soit le contrôleur (`Lvl_Raraka` tourne avec `BP_FirstPersonPlayerController`) et ne touche pas `AIslanderHUD`, réservé par T-069. Le widget appartient au PlayerController local (`CreateWidget(Controller)` + `AddToPlayerScreen`). La recherche de source (0,5 s) s'arrête dès que chaque joueur local a son compteur.

Sons : `AlertSound`, `EvacuationSound`, `ZoneSecuredSound` et `SoundClass` (à régler sur la classe Effets de `DA_SettingsRules`, T-073) dans le DataAsset, chargés en asynchrone, joués avec `SoundClassOverride`. Sans asset, pas de son, et pas d'erreur.

Configuration (`DefaultGame.ini`) :

```ini
[/Script/monke_island.FloodTimerSubsystem]
RulesAsset=/Game/MonkeIsland/UI/DA_HudTimerRules.DA_HudTimerRules
WidgetClass=/Game/MonkeIsland/UI/WBP_FloodTimer.WBP_FloodTimer_C
```

Vides, ces deux entrées donnent les valeurs par défaut de la classe et la vue Slate native.

Base pour un WBP : un Blueprint enfant de `UFloodTimerWidget` qui a un arbre de widgets remplace la vue Slate. Il s'habille avec `OnFloodTimerUpdated`, `OnFloodTimerModeChanged`, `OnZoneSecured`, `OnFloodAlert` et lit `GetMode`, `GetSecondsLeft`, `GetClockText`. Les sons et les alertes restent en C++.

## Critères d'acceptation

- [x] Formatage `m:ss`, arrondi au-dessus, jamais négatif (spec).
- [x] Recette 14 : zone à 75 s → échéance 240 s, `2:45` affiché ; seconde zone → 360 s ; même texte hôte et client pour le même état et le même temps serveur (spec).
- [x] Alertes 60/30/10 uniques, non rejouées après une extension ; arrivée tardive silencieuse ; un saut = une alerte (spec).
- [x] Bascule vers le décompte d'évacuation, puis « Île submergée » ; pas d'évacuation côté client avant la phase serveur (spec).
- [ ] Specs exécutées (cible éditeur : à la gate INT, éditeur fermé).
- [ ] PIE solo sur `Lvl_Raraka` : compteur visible (source acteur d'inondation), `monke.FloodNow` → alerte 10 s puis évacuation.
- [ ] PIE hôte + client sur une map avec `AExpeditionGameState` : mêmes valeurs, `+2:00` des deux côtés.
- [ ] Lisibilité contrôlée par capture en 1080p et 720p.

## Reste à faire

- `DA_HudTimerRules` (sons courts, SoundClass Effets, icône `T_UI_Water`) et les entrées `DefaultGame.ini` : tâche éditeur et audio.
- `WBP_HUD` (§21) : intégrer ce compteur au HUD complet au lot **L5** (santé/endurance de `AIslanderHUD`, objet porté, quotas, zones restantes, direction de la barque), en héritant de `UFloodTimerWidget` ou en l'imbriquant.
- « Zones restantes » et quotas arrimés (§6.1 UI) : à ajouter au HUD complet (`GetRemainingZoneCount` existe déjà).
- Échelle d'interface du menu des réglages (§18) : quand elle existera, l'appliquer au compteur.
- Unifier les deux horloges d'inondation (voir l'incohérence relevée dans le rapport de T-097) : ce n'est pas du ressort de l'UI.
