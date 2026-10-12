# Feature : Furtivité — accroupi, nuit, bruits

- **Statut** : review. Le code est écrit et compilé fichier par fichier (UBT `-SingleFile`, détail au §8). La spec `monke_island.Stealth.Rules` est écrite et compilée, mais **pas encore exécutée**. **Rien n'a été testé en PIE**, ni en solo ni en hôte + client. `DA_StealthRules` n'existe pas encore : l'action `stealthsetup` le crée une fois l'éditeur recompilé.
- **Owner** : `ue-ai-programmer`
- **Date** : 2026-10-09
- **Cahier des charges** : lot L5, tâche T-262. C'est une extension du PvE (§6.2–6.3) hors cahier, demandée par le propriétaire le 2026-10-09. Recette liée : §12 n°21 (un parcours combat/collecte reste atteignable) et §23 UI08 (textes de touches corrects). La voix (T-263) relève du L7.

## 1. Intention
Demande du propriétaire (2026-10-09) :
> « ajoute aussi le fait de pouvoir parler in game, activation micro etc… cela va permettre de pouvoir signaler présence aux mobs la nuit ; lorsque je suis accroupi, avec contrôle, cela me permet de me déplacer sans me faire voir par mob ou PNJ, ils ne pourront que m'entendre la nuit par mes déplacements en courant ou en parlant ; accroupi je peux déplacer objet sans me faire voir »

- Un joueur accroupi (Ctrl gauche maintenu) est invisible pour tous les mobs et les PNJ, de jour comme de nuit, même s'il porte un objet. Seul un contact très proche le trahit.
- La nuit, personne n'est vu, sauf au contact. Les mobs n'entendent que :
  - la course ;
  - la voix (T-263) ;
  - les coups de feu et les explosions.
- Un mob ou un villageois qui entend un bruit :
  1. va voir sur place, en marchant, et regarde autour de lui ;
  2. s'il voit alors le joueur, le mob attaque et le villageois réagit selon sa logique habituelle ;
  3. sinon, il oublie au bout d'un délai.

## 2. Règles

### Vue (`UStealthSubsystem::CanSee`)
Règles appliquées dans l'ordre (`StealthMath::CanSee`, testé) :
1. **Hors de portée** de l'observateur : la cible n'est pas vue.
2. **La cible n'est pas un joueur** (tiki contre soldat, crabe…) : elle est vue dans la portée, comme avant. Les mobs se voient toujours entre eux.
3. **Joueur accroupi dans un buisson** (à moins de 1,5 m d'un buisson de la grille de `ATikiDirector`, règle T-207 conservée) : vu seulement si l'écart entre les deux corps est d'au plus 50 cm.
4. **Joueur accroupi**, de jour comme de nuit, qu'il porte un objet ou non : vu seulement au contact, c'est-à-dire avec un écart d'au plus 100 cm entre les corps.
   - Entre un tiki et le bean, cela fait environ 1,5 m de centre à centre.
   - Le contact grandit avec le corps de l'observateur : un crabe géant voit au contact à environ 2,3 m.
5. **Nuit** (`EDayPhase::Night` de `ADayNightCycleActor`) : un joueur debout n'est vu qu'au contact.
6. **Sinon** (de jour, debout) : il est vu, comme avant.

`CanSee` n'est qu'un filtre de furtivité, sans lancer de rayon. Chaque observateur garde sa propre ligne de vue : murs, cône de vision des soldats, lancer de rayon des villageois dans leur maison.

### Bruits (`ReportNoise`, `FindHeardNoise`)
| Bruit | Source | Portée (cm) |
|---|---|---|
| Pas de course (`Footstep`) | une foulée tous les 170 cm d'un sprint au sol, calculée par le serveur (`UIslanderMovementComponent::UpdateRunNoise`) | 1 500 |
| Voix (`Voice`) | chat vocal de proximité (T-263), qui appelle l'API | 1 800 (T-263 peut l'ajuster selon le volume) |
| Tir (`Gunshot`) | `AFirearmWeapon::FireProjectiles` (pistolet, tromblon, fusil, mousquet pirate) et `AMusketWeapon::TraceShot` | 5 000 |
| Explosion (`Impact`) | `AGrenadeProjectile::Explode` | 6 000 |

La marche, l'accroupi et la nage ne font aucun bruit.

- **Fenêtre et capacité** : un bruit reste audible 1 s, pour qu'un auditeur qui ne réfléchit que de temps en temps l'entende quand même. Au plus 64 bruits sont gardés ; au-delà, le plus ancien part.
- **Qui alerte qui** : seuls les bruits d'un joueur, de son arme ou de son projectile alertent les mobs. Un mob n'entend jamais ses propres bruits.
- **Portée effective** : la portée du bruit est multipliée par l'ouïe de l'auditeur : 1,0 pour les guerriers tiki, 0,8 pour les villageois, 1,0 pour les soldats du fort.
- **Choix du bruit** : un auditeur prend le bruit le plus récent ; à égalité, le plus proche.
- **Bornes** : le serveur ramène toute portée demandée à 20 000 cm au plus, appels de T-263 compris. Une valeur NaN ou négative est ignorée.

### Réactions
- **Guerriers tiki** (`ATikiWarrior`) :
  - **Repérage** : ils ne prennent pour cible que la créature ennemie la plus proche qu'ils voient.
  - **Cible perdue de vue** : ils la suivent encore 1 s, à la course, vers l'endroit où ils l'ont vue ou entendue en dernier. Ensuite, ils la cherchent là-bas.
  - **Bruit sans cible** : ils se tournent vers le bruit, grognent (alerte, avec le délai `AggroVoiceCooldown` entre deux grognements), vont voir en marchant, puis regardent autour d'eux. Ils oublient 10 s après le dernier indice.
  - **Rayon d'action** : un gardien ou un monstre embusqué ne va voir qu'à `LoseRadius` de son poste au plus.
  - **Défenseur** : un défenseur, appelé contre un joueur, ne suit que ce joueur.
  - **Attaque** : ils ne frappent ou ne tirent au mousquet que sur une cible qu'ils voient.
  - **Coup reçu** : un coup reçu révèle l'attaquant, même caché.
- **Monstres embusqués dans un buisson** (`ATikiDirector::TickAmbush`) :
  - le monstre sort quand il voit le joueur depuis son buisson ;
  - s'il ne fait que l'entendre, il sort sans cible et va voir le bruit ;
  - un joueur accroupi, ou silencieux la nuit, passe sans le déclencher.
- **Villageois** (`ATikiVillager`) :
  - **Joueur caché** : ils ne voient pas un joueur accroupi, ni un joueur la nuit, sauf au contact. Ils ne le saluent pas.
  - **Villageois d'une maison** : il ne surveille pas un joueur accroupi chez lui. Il perd celui qu'il surveillait dès que celui-ci s'accroupit : il va alors voir là où il l'a vu en dernier, puis retourne à sa routine.
  - **Écoute** : pendant leur routine, ils écoutent :
    - un tir ou une explosion les fait fuir loin du tireur (« paniquer ») ; un villageois de maison panique si le tireur est près de lui, et aucun tiki n'est appelé ;
    - des pas de course ou une voix les envoient voir, en marchant. Arrivés sur place, ils regardent autour d'eux (clip `LookAround`). Un villageois de maison ne réagit qu'aux bruits dans sa maison ou à moins de 3 m autour, et y va par les points de passage de la maison ;
    - un joueur vu pendant la recherche est surveillé dans la maison, ou salué sur la place.
  - **Sans écoute** : pendant la cérémonie du drapeau, la fuite, la cachette ou la surveillance d'un joueur, ils n'écoutent pas (« rassembler »).
- **Vol dans une maison** (`ATikiDirector::CheckTheft`) : un objet sorti par un voleur que le villageois ne voit pas (accroupi ou de nuit) passe inaperçu. L'objet est oublié, sans panique.
- **Soldats du fort** (`AFortSoldier`) :
  - **Vue** : ils ne voient un joueur caché qu'au contact. Le filtre s'ajoute à leur ligne de vue et à leur cône de vision.
  - **Bruit** : un bruit d'ennemi les met en alerte. Ils vont voir en marchant (alerte), puis abandonnent `LoseTargetSeconds` (10 s, `DA_FortSoldierRules`) après le dernier indice.
  - **Cible perdue** : une cible vue puis perdue est poursuivie à la course jusqu'au dernier endroit connu, comme avant.
- **Crabe géant** : branché par T-261 sur la même API (`CanSee`, `FindHeardNoise`).

## 3. Valeurs (`DA_StealthRules`, `/Game/MonkeIsland/Stealth/Data`, `UStealthRules`)
Le sous-système charge `DA_StealthRules` par chemin. Si l'asset manque, les valeurs par défaut de la classe s'appliquent ; ce sont les mêmes.

| Paramètre | Valeur | Rôle |
|---|---|---|
| `Sight.bCrouchHides` | vrai | accroupi = caché partout ; si faux, caché seulement dans un buisson (T-207 seul) |
| `Sight.bNightHidesStanding` | vrai | la nuit, un joueur debout est caché |
| `Sight.ContactGapCm` | 100 | écart entre les corps à partir duquel un joueur caché est vu |
| `Sight.BushRadiusCm` | 150 | distance à un buisson pour être « dans » le buisson |
| `Sight.BushContactGapCm` | 50 | contact accroupi dans un buisson |
| `Hearing.RunStepRadiusCm` / `RunStrideCm` | 1 500 / 170 | portée d'une foulée, distance entre deux foulées |
| `Hearing.VoiceRadiusCm` | 1 800 | portée de la voix |
| `Hearing.GunshotRadiusCm` / `ImpactRadiusCm` | 5 000 / 6 000 | tir, explosion |
| `Hearing.MaxNoiseRadiusCm` | 20 000 | borne serveur |
| `Hearing.NoiseWindowSeconds` / `MaxNoises` | 1 s / 64 | durée d'audibilité, capacité |
| `NoiseMemorySeconds` | 10 s | délai d'oubli d'une recherche |
| `LoseSightGraceSeconds` | 1 s | une cible perdue de vue est encore suivie pendant ce délai |
| `ListenIntervalSeconds` | 0,2 s | écoute des mobs et villageois |
| `ArriveRadiusCm` / `SearchTurnSeconds` | 120 / 1,6 s | arrivée sur place, rythme du regard |
| `WarriorHearingScale` / `VillagerHearingScale` / `SoldierHearingScale` | 1,0 / 0,8 / 1,0 | ouïe |
| `VillagerSightCm` | 2 000 | un villageois qui cherche voit un joueur debout jusque-là (de jour) |
| `HouseNoiseMarginCm` | 300 | un villageois de maison réagit aux bruits jusqu'à cette distance autour de sa maison |

## 4. Commandes et retour joueur
- **Accroupi** : Ctrl gauche **maintenu**, comme dans Counter-Strike (`IA_Crouch`, `IMC_Islander_Actions`). C reste lié à la même action.
  - L'appui accroupit, le relâchement relève.
  - Si la touche est tenue au moment d'atterrir, de sortir de l'eau ou de se relever d'une chute, le personnage s'accroupit dès que possible.
  - Dans l'eau, la même touche fait plonger.
- **Course** : Maj ne relève plus un joueur qui tient Ctrl. Accroupi, on ne court pas.
- **Rituel** : pendant le rituel du drapeau, la posture ne change pas.
- **HUD TAPAO** (en bas à gauche, au-dessus des jauges) :
  - « Caché » quand le joueur est accroupi, ou debout la nuit ;
  - « Bruyant » (doré) pendant un sprint au sol ;
  - rien de jour quand il est debout.

  L'indicateur est calculé en local (accroupi et horloge répliqués), sans nouvelle réplication.
- **Aide des touches** : `docs/features/player-movement-lc.md`, mise à jour.

## 5. Réseau
- **Serveur autoritaire** :
  - les bruits ne vivent que sur le serveur, et `ReportNoise` ignore un appel client ;
  - les foulées sont mesurées par le serveur, à partir des mouvements reçus ;
  - les tirs et les explosions sont signalés là où le serveur résout le coup ;
  - la vue est décidée par le serveur.
- **Données répliquées utilisées** : la posture accroupie (indicateur du moteur, `bIsCrouched`) et l'horloge jour/nuit. Aucune nouvelle propriété répliquée, aucun nouveau RPC.
- **Voix (T-263)** : le serveur appelle `ReportNoise` quand un joueur parle et borne la portée.
- **Tests réseau** : non faits (hôte + client, réseau dégradé). À faire en INT.

## 6. API (contrat partagé avec T-261 et T-263)
Dans `Source/monke_island/Stealth/StealthSubsystem.h` (les types sont dans `StealthTypes.h`, inclus par ce fichier) :
```cpp
UENUM(BlueprintType) enum class EStealthNoise : uint8 { Footstep, Voice, Gunshot, Impact };
USTRUCT(BlueprintType) struct FStealthNoise { TWeakObjectPtr<AActor> Instigator; FVector Location; float RadiusCm; EStealthNoise Kind; double Time; };
static UStealthSubsystem* Get(const UObject* WorldContext);
bool CanSee(const AActor* Observer, const AActor* Target, float SightRangeCm) const;              // portée <= 0 : sans limite
void ReportNoise(AActor* Instigator, const FVector& Location, float RadiusCm, EStealthNoise Kind);  // serveur
bool FindHeardNoise(const AActor* Listener, float HearingScale, FStealthNoise& OutNoise) const;    // serveur
bool IsNight() const;
```
Autres fonctions disponibles :
- `RulesOf` et `GetRules` : règles en vigueur ;
- `GetNoiseRadius(Kind)` : portée d'un type de bruit (par exemple `ReportNoise(Pawn, Loc, Stealth->GetNoiseRadius(EStealthNoise::Voice) * Volume, EStealthNoise::Voice)` pour T-263) ;
- `ReportNoiseOfKind` et `Report(WorldContext, …)` : une ligne pour une arme ;
- `CanSeeFrom` : observateur sans acteur, comme le monstre d'un buisson ;
- `FindHeardNoiseAt` : écoute depuis un point ;
- `GetExposure` : état pour le HUD ;
- `SetCoverQuery` et `ClearCoverQuery` : requête de buisson, branchée par `ATikiDirector`.

Debug (hors Shipping) :
- `monke.Stealth.Debug 1` journalise chaque bruit et dessine sa portée sur le serveur ;
- `monke.Stealth.Night -1|0|1` force le jour ou la nuit pour la furtivité.

Journal : `LogMonkeStealth`.

## 7. Critères d'acceptation (recette proposée)
| ID | Vérification | Attendu |
|---|---|---|
| FURT-01 | Jour, debout, à 10 m d'un guerrier | repéré comme avant (alerte, attaque) |
| FURT-02 | Jour, Ctrl maintenu, passer à 3 m d'un guerrier, d'un soldat et d'un villageois, objet porté ou non | personne ne réagit ; à environ 1,5 m, repéré |
| FURT-03 | Relâcher Ctrl | le personnage se relève ; Maj ne le relève pas tant que Ctrl est tenu |
| FURT-04 | Nuit (`monke.SetHour 23` ou `monke.Stealth.Night 1`), marcher à 5 m d'un guerrier | non repéré |
| FURT-05 | Nuit, sprinter à 12 m d'un guerrier | il se tourne, grogne, va voir en marchant, regarde autour ; s'il atteint le contact, il attaque ; sinon il oublie au bout d'environ 10 s (`LogMonkeStealth`) |
| FURT-06 | Tirer au mousquet près d'un village | les villageois à moins de 40 m fuient ; les soldats et guerriers à moins de 50 m vont voir |
| FURT-07 | Grenade | entendue à 60 m |
| FURT-08 | Chassé de jour, s'accroupir et s'éloigner | le guerrier court au dernier endroit vu, cherche, puis abandonne |
| FURT-09 | Voler un objet d'une maison accroupi | pas de panique (`unseen` dans le journal) ; debout, panique comme avant |
| FURT-10 | Hôte + client : FURT-02, FURT-05 et FURT-06 joués par le client | mêmes réactions que pour l'hôte |
| FURT-11 | HUD | « Caché » accroupi ou la nuit, « Bruyant » en sprint |

## 8. Preuves et statut honnête
- **Compilé** : chaque `.cpp` créé ou modifié, avec UBT `-SingleFile` (éditeur, Development), jusqu'à « Result: Succeeded ». La liste et l'état sont dans le rapport de la tâche T-262 au chantier.
- **Tests** : la spec `monke_island.Stealth.Rules` (vue, bush, contact, nuit, portée des bruits, fenêtre, oubli, exposition) est écrite et compilée, mais pas exécutée : les tests d'automatisation sont réservés au lead dans cette vague.
- **Éditeur** : l'action `stealthsetup` a tourné. Constats :
  - `IA_Crouch` est booléen, sans déclencheur ;
  - Ctrl gauche et C y sont déjà liés dans `IMC_Islander_Actions` ;
  - aucune autre action n'utilise Ctrl gauche ;
  - `BP_IslanderBean` utilise bien `IA_Crouch`.

  `DA_StealthRules` n'est pas créé : la classe sera disponible après le build complet. Relancer `stealthsetup` à ce moment-là.
- **Non fait** : PIE solo, PIE hôte + client, build packagé.
- **Cuisson** : `DA_StealthRules` n'est référencé par aucune carte. Il faut ajouter `+DirectoriesToAlwaysCook=(Path="/Game/MonkeIsland/Stealth")` dans `Config/DefaultGame.ini`. Sans cette ligne, un build packagé tourne avec les valeurs par défaut de la classe, qui sont identiques.

## 9. Limites connues et suite
- **Bruits** : ils traversent les murs. Il n'y a pas d'atténuation par les obstacles.
- **Recherche** : la recherche est en ligne droite pour les guerriers et les villageois de la place, comme leurs déplacements actuels (pas de NavMesh sur l'atoll). Les soldats utilisent leur navigation.
- **Éclairage** : la nuit est binaire (phase `Night`). Il n'y a pas de visibilité graduée selon la lumière ou la torche. Une torche allumée pourrait trahir le joueur la nuit ; proposition à arbitrer.
  - **T-275 (2026-10-10)** : la proposition est appliquée aux guerriers tiki et aux monstres cachés dans les buissons (`DA_TikiRules.Pack.bTorchRevealsAtNight`, `TorchNightSightCm` 25 m) : la nuit, un joueur debout qui tient une torche allumée est vu ; accroupi, il reste caché ; de jour, rien ne change. Les soldats et les villageois ne sont pas concernés. Règle à arbitrer par le propriétaire ; le drapeau la coupe. Détail : `characters-tiki.md`, « Refonte des mécaniques ».
- **Retours** : il n'y a ni son ni indicateur au-dessus d'un mob qui a entendu ; seuls le grognement (guerriers) et le regard (villageois) le montrent.
