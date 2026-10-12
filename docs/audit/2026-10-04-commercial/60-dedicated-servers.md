# 60 — Serveurs dédiés : utiles ou non, coûts
Domaine : F · Auditeur : ingénieur infrastructure

## Verdict
Le listen server reste le bon choix pour la sortie : le cahier n'exige aucun matchmaking public (§16), un dédié ne corrige ni les RPC clients ni les griefs, et l'hébergé coûterait 0,18 à 0,32 $/h par équipage plus un backend. Le code tolère le dédié sans l'avoir exécuté, et rien ne peut en être construit ici (moteur Launcher, 26 Go libres). Risque : dépendre d'un hébergeur (Hathora et Multiplay ont fermé en 2026) ou promettre des serveurs sans mesurer l'hôte.

## Constats

- **[Majeur] Trois modèles : un seul tient pour la sortie**
  - Constat : listen (§1) : 0 $, mais l'hôte porte rendu, IA, physique (51) et son départ clôt la session (§24.5). Communautaire : 0 $ récurrent, mais Target serveur et administration. Hébergé (Edgegap : 0,069 $/h/vCPU, 0,10 $/Go sortant) : 2 à 4 vCPU (supposé) et 4 clients à 30 Ko/s (46) donnent 0,18 à 0,32 $/h, soit 33 à 57 $ par joueur simultané et par mois (calculé). Un dédié retire le rendu, pas l'IA ni la crue (51).
  - Preuve : https://edgegap.com/pricing (résumé de recherche) ; cahier §16 l.321.
  - Recommandation : sortie : listen et relais Steam (56). Ensuite, déclencheurs proposés : dédié communautaire si l'hôte de référence rate le seuil fixé en 51 à 4 joueurs ou si 20 % des avis le citent ; hébergé seulement avec matchmaking public ou classements.
  - Effort : S

- **[Majeur] Hébergeurs instables en 2026**
  - Constat : Hathora a fermé le 5 mai, Unity Multiplay le 31 mars. PlayFab : 1 000 joueurs à vie et 750 h gratuites hors Xbox, puis forfait payant. Edgegap tourne.
  - Preuve : https://www.techspot.com/news/111969-stormgate-servers-go-dark-following-ai-focused-hosting.html ; https://crux.supercraft.host/blog/game-backend-shutdowns-2026-who-died-where-studios-went/ ; https://developer.microsoft.com/en-us/games/products/playfab/pricing/ (résumés, source tierce).
  - Recommandation : aucun contrat ; si hébergé : conteneur Linux portable derrière `ISessionBackend` (56), dépense plafonnée.
  - Effort : S

- **[Mineur] Le code tolère le dédié, sans preuve**
  - Constat : 15 `NM_DedicatedServer` dans 12 fichiers, tous cosmétiques ; l'autorité se teste en `!= NM_Client`, valable sur dédié. Mais 0 test `-server`, pas de `Target.cs` serveur, `ue.ps1 package` ne produit que le client.
  - Preuve : ExpeditionIslandSubsystem.cpp:393 ; BoatSubsystem.cpp:33 ; ue.ps1:140.
  - Recommandation : `UnrealEditor.exe monke_island.uproject <map> -server -game -log` et 2 clients `-game` ; une spec `-server` (58).
  - Effort : S

- **[Majeur] Construire un serveur : source build, disque plein**
  - Constat : moteur Launcher (`IsPromotedBuild: 1`) ; Epic exige un moteur compilé depuis les sources. Un exe Game packagé ne peut pas être dédié (`UE_GAME`) ; seul l'éditeur lit `-server`, non distribuable. C: n'a que 26 Go libres sur 1,9 To, un moteur source bien plus (supposé).
  - Preuve : https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine (« You must use a source build ») ; CoreMisc.h:146-169.
  - Recommandation : source build au tag 5.8.3, autre disque ou 2e PC, seulement sur déclencheur.
  - Effort : L

- **[Majeur] Persistance : la sauvegarde active disparaît avec l'UI**
  - Constat : `UTapaoUISubsystem` n'existe pas sur dédié et porte `GetSave`/`Autosave` : rien ne serait sauvé. `URunSubsystem` convient. Le cahier attache la campagne à l'hôte (§9, §16) : sur dédié, à qui ? Hébergé : base de données en plus.
  - Preuve : TapaoUISubsystem.cpp:74-77,672-699 ; ADR-005:43.
  - Recommandation : `SaveSubsystem` sans UI (T-057) ; `SG_Run` copiée chez chaque joueur à chaque signature (54) : « l'hôte part » sans serveur.
  - Effort : M

- **[Mineur] Triche : le dédié protège de l'hôte, pas des clients**
  - Constat : les trous sont côté clients (21 RPC sans `_Validate`, 40) et subsistent sur dédié. Sans classements ni graines partagées (19), il n'apporte rien. Un serveur public facture la sortie, attaque comprise.
  - Preuve : 40 ; grep `_Validate` : 0.
  - Recommandation : « coop non classé » en 1.0 ; durcir les RPC avant tout serveur public.
  - Effort : M

- **[Majeur] Anti-griefing : le dédié déplace la modération**
  - Constat : les griefs de 57 sont des règles, pas de l'infrastructure. En listen l'hôte modère ; en dédié communautaire il faut mot de passe, expulsion, bannissement (0 `KickPlayer`, 0 `PreLogin`) ; en public hébergé, personne. Les règles disent « hôte » (quota manqué, « Recommencer »), absent d'un serveur dédié.
  - Preuve : cahier §24.5 ; 57 ; grep.
  - Recommandation : rôle « Capitaine » (serveur, transférable) dans `ARunGameMode`, sans test `NM_ListenServer` (2 seulement aujourd'hui), avant T-112 à T-116.
  - Effort : S, puis M

## Manques
1. ADR « modèle réseau » : absent du cahier, du backlog, des ADR.
2. Rôle Capitaine, transfert de commandement.

## Quick wins
1. ADR (½ j) : listen en 1.0, déclencheurs du dédié.
2. Banc `-server` du constat 3 (≤ 1 j).
3. `bClampListenServerTickRate=True`, `NetServerMaxTickRate=60` (IpNetDriver, SteamSocketsNetDriver) : un dédié est plafonné à 30 Hz (GameEngine.cpp:1742), un listen jamais ; à arbitrer.
4. Libérer ≈ 100 Go (estimé) sur C: ou désigner le 2e PC.

## Questions qui challengent l'équipe
1. Quel seuil (joueurs simultanés, avis, mesure de l'hôte) vous ferait payer un serveur, et qui répond de l'astreinte à 3 h ?
2. « L'hôte part, tout s'arrête » (§24.5) : acceptable à la manche 4 ?
3. Public ou amis ? Sans matchmaking (§16), l'hébergé n'a pas de client ; avec, modération (57) et RPC (40) d'abord.
