# 61 — Cohérence de la direction artistique
Domaine : G · Auditeur : directeur artistique

## Verdict
Bean et tiki forment une famille de figurines vinyle vendable, et le lagon au soleil couchant fait une vraie image (vu : T-095/01). Le reste est un collage sans règle écrite : échelles, matériaux, palette et UI divergent, sans étalonnage. Risque commercial : captures lues comme un assemblage d'assets, satire de la Couronne invisible.

## Constats
- **[Bloquant commercial] Aucune DA écrite, contrats contradictoires**
  - Constat : le cahier veut « stylisée, inquiétante », l'environment artist « crédible et réaliste », le tech artist un turquoise « pas peint ». Aucun directeur artistique parmi les 29 agents, aucune bible, aucune tâche DA.
  - Preuve : CAHIER:23 ; ue-environment-artist.md:6 ; ue-tech-artist.md:10 ; islands.md:13.
  - Recommandation : `docs/art/STYLE.md`, style cible « figurines de la Couronne sur île vivante » : acteurs (bean, tiki, butin, armes, barque, galion) = jouets lisses saturés ; construit = intermédiaire, ≤ 8 matériaux teintés ; nature = PBR actuel étalonné. Les tiki, matière d'île, font le pont. Check DA au gate REC.
  - Effort : S

- **[Majeur] Aucun étalonnage : rendu Unreal par défaut**
  - Constat : aucun PostProcessVolume dans les 3 cartes (recherche de chaînes ; Blueprints non inspectés), seule l'exposition de nuit en code, ni LUT ni preset ; les 4 captures du dépôt sont de jour.
  - Preuve : DayNightCycleActor.cpp:41,230 ; docs/production/captures/.
  - Recommandation : volume global + LUT (ombres froides, lumières chaudes, saturation +10 %) ; presets jour vif, crépuscule doré (T-095/01), nuit et tempête désaturées, lanternes chaudes.
  - Effort : M

- **[Majeur] Quatre langages de matériaux, acteurs sans maître commun**
  - Constat : photo-PBR (nature, villages), atlas peint (barque), aplats mouchetés (galion), aplats de palette (bean, tiki). Les 4 tiki cumulent 55 matériaux autonomes, 0 instance ; bake encore à faire.
  - Preuve : Content/MonkeIsland/Characters (24+11+9+11 `M_`, 0 `MI_`) ; characters-tiki.md:11,81 ; import_galleon.py:36-49.
  - Recommandation : `M_Toy_Master` (rampe de palette, reflet de bord, diffusion « argile ») pour bean, tiki, butin, armes, avant le bake.
  - Effort : M

- **[Majeur] Villages et échelles : le construit est hors style**
  - Constat : 1 181 maisons sur 20 fragments, minka photo-PBR de 36 matériaux, brief « not a cartoon toy » face à des beans jouets. Échelle : barque ×0,75, galion ×0,58, mais minka (7 m, portes de 2,1 m) et cocotiers restent humains.
  - Preuve : villages-japonais.md:158 ; minka-rurale.md:136 ; barque.md:50 ; galleon-report.json ; village_kit.json.
  - Recommandation : trancher avant de regénérer 60 villages : Japon réduit à ses signatures (chaume, torii, lanternes) sur un kit jouet ≤ 8 matériaux, ou fare polynésien ; échelle écrite : construit ×0,58, nature 1:1.
  - Effort : L

- **[Majeur] Palette non partitionnée**
  - Constat : mesuré sur les planches, le rouge du bean (#A31E1A) est voisin de celui du Guerrier Coco (#AE3727) et du Crabe (#B54426) : ami et ennemi partagent la couleur de marque. Le galion, bleu moyen (≈ #3F6FAD), sort de la marine #1E2848.
  - Preuve : sprite/player-1.png, mob-1 à 3 ; import_galleon.py:39.
  - Recommandation : trois familles : Couronne (rouge, marine, crème, laiton), Île (ocre, vert, turquoise, noir de lave), Valeur (or seul) ; galion en marine ; recolorer les tiki.
  - Effort : S

- **[Majeur] UI d'un autre jeu, satire invisible**
  - Constat : UI = kit « TAPAO: Rush Island » (bois, corde, parchemin), logo compris ; le cahier impose « Crownfall Expeditions », la config « First Person Template ». Ni sceau, ni formulaire, ni registre : la satire du §1 reste invisible.
  - Preuve : TapaoUISubsystem.cpp:638 ; ui_manifest.json ; DefaultGame.ini:3.
  - Recommandation : changer logo et chaînes ; registre, quota et bilan en paperasse royale (papier, encre, cire, tampon), idéalement un registre 3D chez le Percepteur ; voir 68.
  - Effort : M

- **[Majeur] Hiérarchie de détail et de finition inversée**
  - Constat : les vitrines marketing sont les moins finies : requin lisse, galion à l'intérieur noir et au pavillon blanc uni, relique grise (aperçu Blender). La recette signale `MI_Bean_SleeveRed` sans usage SkeletalMesh : manches possiblement par défaut.
  - Preuve : art/_previews/shark.png, tidal_relic.png ; galleon_pie.png ; galleon.md:45 ; v1-rec-investisseurs.md.
  - Recommandation : finition par rang (héros : bean, galion, requin, relique, butin) avant le trailer ; vérifier le bean packagé.
  - Effort : M

## Manques
1. Bible de style et garant DA.
2. Presets d'étalonnage jour, crépuscule, nuit, tempête.
3. Motif signature : sceau de la Couronne sur caisses, voiles, pavillon, UI.
4. Vitrine du bean : en 1re personne, seuls les coéquipiers le voient (supposé).

## Quick wins
1. `STYLE.md` : hex des trois familles, rangs, échelle, interdits.
2. `MI_Galleon_Blue` en marine ; emblème sur pavillon et voiles.
3. Volume post-process global d'essai, comparé à T-095/01.
4. Remplacer « TAPAO : Rush Island » (sortie, logo, `ProjectName`).
5. Vérifier `MI_Bean_SleeveRed` en packagé.

## Questions qui challengent l'équipe
1. Quelle est l'image unique de la capsule Steam, et quelle règle y fait entrer galion, villages et UI ?
2. Qui tranche la DA : vous seul (§10) ou un garant pouvant refuser un asset ? Les planches `sprite/` sont générées (minka-rurale.md:131) : leur usage sera-t-il déclaré sur Steam (70, 97) ?
3. Quelle phrase du pitch justifie des maisons japonaises dans un archipel de tiki polynésiens gardé par des soldats britanniques ?
