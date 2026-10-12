# 45 — Rendu : Lumen, ombres, eau, lumières, Nanite
Domaine : E · Auditeur : ingénieur rendu

## Verdict
Le rendu est conçu haut de gamme (Lumen matériel, ombres virtuelles, Nanite, eau SLW) et jamais mesuré hors éditeur : 60 FPS en 1080p sur machine milieu de gamme n'est pas réaliste aujourd'hui. La machine de dev (RTX 5060 Ti 16 Go) passe déjà sous 60 FPS en Ultra pendant la submersion (éditeur). Risque : premier lancement en Ultra, sans palier.

## Constats
- **[Bloquant commercial] 60 FPS non démontré, défaut Ultra**
  - Constat : PIE éditeur, Ultra : moyenne 16,7–19,8 ms, maxima 25–108 ms, 17–18 ms en submersion. Démarrage d'un fragment (calculé sur horodatages) : 6–14 FPS à +3–5 s, 12–19 s pour remonter à 55, plus de 25 s sur 2 fragments (cause non isolée). Aucune mesure packagée.
  - Preuve : Saved/Logs …00.39.34.log:4499,5187 ; …00.51.32.log:2516 ; T-091 a-faire ; performance.md:3.
  - Recommandation : trois machines de référence (RTX 3060, GTX 1660 Super, Deck) ; capture packagée (CSV, Insights).
  - Effort : M
- **[Bloquant commercial] Aucun palier de qualité conçu**
  - Constat : ni DefaultScalability.ini ni profil d'appareil, `DefaultGraphicsPerformance=Maximum` ; premier lancement en Epic (cahier §18 : « Moyen ») ; ni benchmark, échelle de rendu ou résolution dynamique. Les paliers moteur coupent les réflexions Lumen sous Élevé et la GI Lumen en Faible, sans éclairage statique de repli (nuit, intérieurs, mer non validés).
  - Preuve : DefaultEngine.ini:27,114 ; moteur Scalability.cpp:1038, BaseScalability.ini:318,519 ; SettingsSubsystem.cpp:137,233.
  - Recommandation : défaut « Moyen » + `RunHardwareBenchmark`. Faible : sans Lumen. Moyen : GI@1, VSM 512 pages, 4 lampes à ombres, TSR 67 %. Élevé : Lumen logiciel, VSM 2 048, 8 lampes, TSR 75 %. Ultra : RT autorisé, historique TSR 100 % (200 % aujourd'hui). À mesurer.
  - Effort : M
- **[Majeur] Lumen matériel par défaut sur 816 000 instances**
  - Constat : `r.RayTracing=True` et `r.Lumen.HardwareRayTracing=1` (défaut moteur) ; coût signalé par le moteur au-delà de 100 000 instances ; scène RT active (24 recréations de SBT).
  - Preuve : DefaultEngine.ini:32 ; moteur LumenHardwareRayTracingCommon.cpp:16-22.
  - Recommandation : `r.Lumen.HardwareRayTracing=0` hors Ultra ; viser L_Frag_* plutôt que le monolithe de 29 km.
  - Effort : S
- **[Majeur] Soleil à 20 Hz : cache VSM jamais stable**
  - Constat : la clé du cache du clipmap contient la direction de la lumière ; chaque mise à jour coûte deux images pleines, soit ≈ 2 images sur 3 à 60 FPS (lu dans le code). S'y ajoute le vent (WPO) sur 43–49 k cocotiers Nanite, sans distance de désactivation.
  - Preuve : DayNightRules.h:59 ; DayNightCycleActor.cpp:91,252 ; moteur VirtualShadowMapCacheManager.cpp:362-392 ; storm-flood.md:51.
  - Recommandation : `UpdateInterval` en pas angulaire (≥ 0,25°) ; distance de désactivation du WPO sur les ISM de palmiers.
  - Effort : S
- **[Majeur] 43–66 lampes à ombres par fragment, sans budget**
  - Constat : 20 JSON de villages (calculé) : 103–166 lumières, 43–66 à ombres (une par maison), pas 36 ; allumées le jour (35 %). La torche (4 joueurs) garde ses ombres et sa position vibre à chaque image : cache VSM annulé (code moteur). 9 avertissements VSM (marquage non Nanite).
  - Preuve : villages/fragments/*.json ; VillageAmbienceActor.cpp:132-154 ; TorchItem.cpp:422-424.
  - Recommandation : budget d'ombres par palier (N plus proches) ; lampes éteintes le jour hors intérieur ; torche sans ombres sous Ultra ; MegaLights (expérimental) à tester en Ultra.
  - Effort : M
- **[Majeur] PNJ de 41–56 k triangles, 8–14 matériaux, sans LOD**
  - Constat : 36–66 villageois par fragment, squelettiques, non Nanite (supposé), LOD0 seul cité ; redessinés dans chaque page d'ombre touchée, cause probable de l'avertissement VSM (supposé).
  - Preuve : characters-tiki.md:20-28 ; villages-japonais.md:146.
  - Recommandation : LOD automatiques (25/8/2 %), 2 matériaux ; essayer le Nanite squelettique du 5.8.
  - Effort : M
- **[Majeur] Eau : 1,18 M triangles fixes, coût jamais mesuré**
  - Constat : grille 769², bornes ×2,5 donc jamais écartée ; elle et les parcelles d'inondation recréent leur géométrie RT à chaque `CreateMeshSection` ; pics de 48–108 ms en submersion, cause non isolée.
  - Preuve : WaveSurfaceActor.cpp:44,87 ; moteur ProceduralMeshComponent.cpp:156-180 ; sea-waves.md:58.
  - Recommandation : résolution 384/512/768 par palier ; `bVisibleInRayTracing=false` (à vérifier).
  - Effort : S
- **[Mineur] Tempête et ciel : réglages, pas rendu**
  - Constat : tempête = soleil ×0,25, brouillard ×4, −0,7 EV ; ni pluie ni nuages pilotés ; nuages volumétriques sans palier ; flou de mouvement actif (cahier §18 : coupé).
  - Preuve : storm-flood.md:38,104 ; moteur SceneView.cpp:221 ; cahier §18 l.350.
  - Recommandation : lier vagues et nuages à la tempête ; `r.DefaultFeature.MotionBlur=0` ; SkyLight : `bRealTimeCapture` ?
  - Effort : S

## Manques
- Machines de référence ; configuration minimale publiée (DX12 SM6 ; DX11 SM5 sans Nanite).
- Résolution dynamique, upscaler gratuit (DLSS, FSR).
- Rendu sous-marin, pluie, éclairs.

## Quick wins
1. `r.DefaultFeature.MotionBlur=0`.
2. `UpdateInterval` 0,05 → 0,25 s dans `DA_DayNightRules`.
3. `bLightCastsShadows=false` dans `DA_TorchRules`.
4. Premier lancement en « Moyen ».
5. DefaultScalability.ini : RT matériel coupé de Faible à Élevé.

## Questions qui challengent l'équipe
1. Quelle carte est votre « milieu de gamme », et acceptez-vous un Moyen sans réflexions Lumen ?
2. Pourquoi optimiser un monolithe de 29 km (816 000 instances) pour jouer 2 km ?
3. Si Ultra ne tient pas 60 FPS en packagé, que coupez-vous : Lumen, VSM ou la végétation ?
