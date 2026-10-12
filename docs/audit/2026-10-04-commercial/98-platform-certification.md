# 98 — Plateformes et certification
Domaine : J · Auditeur : ingénieur plateformes et certification

## Verdict
Prototype Windows sans intégration de plateforme : ni Steam, ni essai Deck/Proton, ni stratégie écrite (le cahier exclut la publication, §1). Les règles Valve sont franchissables mais lentes : ≈ 4 semaines après paiement de l'AppID. Risque : ouvrir une page Steam pour un coop sans invitation Steam ni manette complète, jamais lancé sous Proton.

## Constats

- **[Majeur] Aucune stratégie de plateforme, publication hors mandat**
  - Constat : « Windows PC, clavier/souris », « aucune publication commerciale incluse » ; lisibilité prévue en 1080p et 720p, pas en 800p (Deck). Aucun ADR, tâche ou fiche ne cite Steam, Deck, Epic ou console.
  - Preuve : cahier §1 l.19 et l.28, §18 l.361 ; grep : 0 résultat dans `docs/production`, `adr`, `conventions`, `features`.
  - Recommandation : ADR « Plateformes » : Steam Windows d'abord, Deck « Playable » en accès anticipé, « Verified » en 1.0, Epic et consoles différés ; §18 en 800p.
  - Effort : S

- **[Bloquant commercial] Steam : rien de branché, calendrier Valve incompressible**
  - Constat : ni plugin Steam au `.uproject`, ni `[OnlineSubsystem]` en ini (le moteur livre le plugin et Steamworks 1.64). Hors réseau (56) : aucun succès, cloud, overlay. Valve : 100 $ par application (récupérés à 1 000 $ de revenu), 21 jours entre paiement et sortie, page « Bientôt » publique ≥ 2 semaines, revue de page 3–5 jours ouvrés (soumettre 7 jours avant), revue de build 1–5 jours : ≈ 4 semaines minimum (calcul).
  - Preuve : `monke_island.uproject:18-122`, `Build.cs:48-51`, `DefaultEngine.ini`, `Steamv164` du moteur, docs Valve, `TapaoUISubsystem.h:67`.
  - Recommandation : payer l'AppID dès l'accord du propriétaire (lance les 21 jours) ; spike : plugin, overlay, un succès, Auto-Cloud sur `Saved/SaveGames/*.sav` (slot `TapaoCampaign`), sans `GameUserSettings.ini` (réglages matériels).
  - Effort : M

- **[Majeur] Deck et Steam Machine : Proton, seul chemin, jamais essayé**
  - Constat : Valve teste lui-même (Deck, Steam Machine, SteamOS tiers) : 30 FPS à 800p, 1080p sur Machine ; « Verified » Deck vaut Machine ; revue ≈ 1 semaine, demande manuelle pas ouverte à tous. Aucune chaîne Linux installée : le jeu passerait par Proton (DX12 vers Vulkan) avec SM6, ray tracing, Substrate (pire cas, supposé). Manette, texte, performances : 26.
  - Preuve : doc Valve ; ni `Engine/Binaries/Linux` ni `LINUX_MULTIARCH_ROOT` ; `DefaultEngine.ini:32,36,45`.
  - Recommandation : au premier package, essai Proton (Deck ou PC Linux) : DX12 contre `-vulkan`, ni lanceur ni avertissement « GPU non supporté ».
  - Effort : M

- **[Majeur] Consoles : porte fermée de cinq façons**
  - Constat : aucune plateforme console dans le moteur installé : Xbox (XR), PlayStation (TRC) et Switch 2 exigent source Epic et SDK sous NDA (supposé). Verrous : (1) manette 4 actions sur 28 (26) ; (2) UI Slate sans CommonUI, pourtant fourni ; (3) textes sous XAG 101 (25) ; (4) mémoire : Series S ≈ 8 Go pour les jeux (source secondaire), Switch 2 12 Go, contre 8,5–11 Go estimés (42), carte monolithique de 110 Mo, Lumen matériel ; (5) listen server, IP directe : sessions, voix, profils plateforme à brancher. Trophées, suspension/reprise : exigences probables (supposé).
  - Preuve : `Engine/Platforms`, `Build.cs:11-24`, cahier §1 l.19, 25, 26, 42.
  - Recommandation : aucun engagement console avant la 1.0 Steam ; CommonUI, glyphes, fragments (ADR-004) servent aussi le Deck. Code portable : aucune branche `PLATFORM_*` (grep).
  - Effort : L

- **[Majeur] Epic Games Store : seconde voie, pas un plan B gratuit**
  - Constat : 88/12, redevance moteur couverte sur ces ventes (annonce 2018, non revalidée) ; ni Linux ni Deck (supposé) ; invitations via EOS, pas Steam (56) : second backend à tester.
  - Preuve : businesswire (Sources), 56.
  - Recommandation : Steam seul jusqu'à la 1.0 ; `ISessionBackend` (56) garde la porte ouverte ; Epic en L8+.
  - Effort : L

- **[Mineur] Divulgation IA Steam : traçabilité absente**
  - Constat : Valve exige de déclarer tout contenu IA livré et consommé par le joueur, pas les assistants de code. L'usine autorise la génération 3D par IA « pour un objet isolé » ; `art/SOURCES.md` est « toutes CC0 » ; aucun usage consigné.
  - Preuve : `.claude/agents/ue-blender-artist.md:13`, `.claude/skills/blender-asset/SKILL.md:19`, `art/SOURCES.md:1`.
  - Recommandation : colonne d'origine (CC0, procédural, IA, main), contrôle avant la page.
  - Effort : S

## Manques
1. ADR « Plateformes », tâches T-NNN Steam/Deck (aucune).
2. Succès indépendants du fournisseur (Steam, puis trophées).
3. Configuration minimale, machines de référence : Deck, GTX 1660, RTX 3060 (45).
4. Checklist de publication : page, survey (80), IA, build, revue Deck.

## Quick wins
1. Cahier §18 : « 1080p, 720p et 1280×800 » (10 min).
2. Configuration Steam Input officielle (manette vers touches actuelles), en attendant la manette native (26) : Deck « Playable » (supposé, ½ j).
3. Premier package lancé sous Linux/Proton (1 j, après 48).

## Questions qui challengent l'équipe
1. Le mandat exclut la publication : qui a décidé de vendre, pour quelle date, vu les 4 semaines Valve ?
2. Si Valve classe le jeu « Unsupported » sous Proton, retirez-vous ray tracing et Substrate, ou assumez-vous le badge ?
3. Pouvez-vous affirmer qu'aucun asset livré n'est généré par IA, ou l'écrirez-vous sur la page Steam ?

Sources (frais, 21 jours, Epic, Series S : extraits de recherche) : https://partner.steamgames.com/doc/steamdeck/compat, https://partner.steamgames.com/doc/store/releasing, https://partner.steamgames.com/doc/gettingstarted/appfee, https://partner.steamgames.com/doc/gettingstarted/contentsurvey, https://www.businesswire.com/news/home/20181204005689/en/Epic-Games-Store-Launch-88-Revenue-Share, https://www.thurrott.com/?p=270746
