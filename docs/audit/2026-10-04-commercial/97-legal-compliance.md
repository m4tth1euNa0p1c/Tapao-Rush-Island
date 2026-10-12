# 97 — Conformité légale : entité, RGPD, licences, accessibilité
Domaine : J · Auditeur : juriste conformité jeu vidéo

## Verdict
Aucun volet légal commercial n'existe : ni entité attestée, ni compte Steamworks, ni EULA, ni politique de confidentialité, et le mandat exclut la publication commerciale (§1:28). Aucune donnée personnelle n'est collectée, pour l'instant. Risque principal : ouvrir la page Steam sans entité, sans licences écrites ni titularité démontrable.

## Constats

- **[Bloquant commercial] Aucune entité vendeuse, aucun compte Steamworks**
  - Constat : aucune entité dans le dépôt, aucune tâche légale au chantier. Steamworks exige un nom légal identique à la banque et aux papiers fiscaux, 100 $ par jeu, 21 jours avant sortie et un contrôle fiscal W-8BEN de 10 à 15 jours ouvrés.
  - Preuve : cahier §1:28, `DefaultGame.ini:3` (aucun `CompanyName`), board.md (aucune ligne légale), https://partner.steamgames.com/doc/gettingstarted/onboarding
  - Recommandation : trancher en octobre pour tenir le playtest de décembre (89), SAS si les investisseurs sont réels, sinon micro-entreprise. Prévoir une adresse de domiciliation (contact public DSA, supposé).
  - Effort : M

- **[Majeur] Aucun document légal, avis tiers du moteur non livrés**
  - Constat : ni EULA/CGU, ni confidentialité, ni mentions. `NOTICES.txt` cite FreeType seul, alors que le build Development embarque onnxruntime, MsQuic, libogg/vorbis, WinPixEventRuntime, Aftermath et couches Vulkan (MIT/BSD/Apache, avis exigé : supposé).
  - Preuve : `build/package/Windows/NOTICES.txt`, `Engine/Binaries/ThirdParty/`, cahier §16:314, textes dans `UE_5.8/Engine/Source/ThirdParty/Licenses`.
  - Recommandation : générer `Licenses/` et Crédits depuis un registre unique (avec Etalab, OFL, IRD, voir 70 et 73), rédiger une EULA/CGU courte (conduite, voix, droit français).
  - Effort : S

- **[Majeur] RGPD : propre aujourd'hui, à cadrer avant L7 et les playtests**
  - Constat : ni module HTTP/Online ni CrashReportClient, donc rien n'est envoyé à l'éditeur. Mais les journaux portent `User: <compte Windows>`, et L7 ajoutera SteamID et voix relayée par l'hôte puis Valve. Ni politique ni registre. Consentement numérique à 15 ans en France, cible PEGI 12 (80).
  - Preuve : `monke_island.Build.cs`, `build/T-124/game-validation.log:663-664`, `security.md:82-87`, cahier §8:187.
  - Recommandation : politique d'une page, voix non enregistrée, PTT, blocage, crash et télémétrie en opt-in avec chemins et pseudos filtrés (SEC-09).
  - Effort : M

- **[Majeur] Titularité : code et planches écrits ou générés par des agents**
  - Constat : le code vient d'agents, les planches `sprite/*.png` semblent générées (70). Le droit d'auteur protège l'apport créatif humain, une sortie purement générée est incertaine (supposé). Steamworks exige un titulaire des droits comme nom légal, et son contrat fait garantir la titularité (supposé).
  - Preuve : CLAUDE.md, cahier §2, §10:214.
  - Recommandation : journal d'apport humain, registre de provenance (70), cession écrite fondateur → société (art. L131-3 CPI), avocat PI (78).
  - Effort : M

- **[Majeur] Builds de recette diffusés avec des données à licence restreinte**
  - Constat : le build T-120 (corrigé en T-122 et T-124) est un « build de recette pour présentation investisseurs » qui cuit `Lvl_Raraka` et les couches contestées par 70 et 73. Le montrer pour lever des fonds peut valoir usage commercial (supposé).
  - Preuve : `docs/releases/v1-rec-investisseurs.md:1`, manifeste `build/package` (46 `DT_Raraka`).
  - Recommandation : suspendre la diffusion, montrer vidéo et captures sans « Raraka » ni « Motutapu », lister les destinataires.
  - Effort : S

- **[Mineur] Redevance Unreal : 5 % au-delà du premier million**
  - Constat : d'après des extraits de recherche (pages en 403), 5 % du revenu brut au-delà de 1 M$ par produit, payable 45 jours après chaque trimestre, rien sous 10 000 $ par trimestre. Assiette (avant ou après commission Steam) et avis Epic non vérifiés. Plugins livrés tous Epic.
  - Preuve : https://www.unrealengine.com/en-US/faq, https://www.unrealengine.com/news/unreal-engines-improved-royalty-reporting-system, `monke_island.uproject`
  - Recommandation : faire lire l'EULA 5.8 par le conseil, suivre le brut cumulé par produit.
  - Effort : S

- **[Mineur] Accessibilité légale (EAA) : le jeu n'est probablement pas visé**
  - Constat : la directive 2019/882 s'applique depuis le 28/06/2025 mais ne cite pas les jeux. Elle vise commerce électronique, communications, matériel, et exempte les micro-entreprises de services (art. 4(5), supposé). Sans boutique payante ni site de vente, pas d'obligation propre, des cosmétiques payants la feraient naître.
  - Preuve : https://www.taylorwessing.com/en/insights-and-events/insights/2025/03/accessibility-in-the-gaming-industry (extrait de recherche), `security.md:17`.
  - Recommandation : ne rien budgéter, traiter l'accessibilité en argument commercial (25), relire avant cosmétiques payants (85) ou consoles (98).
  - Effort : S

## Manques
Checklist priorisée. Avant la page Steam :
1. Entité, banque, domiciliation, Steamworks (100 $, 21 jours), expert-comptable et avocat PI, 1 h chacun.
2. Nom libre (78), lettres DAG/DAF (70, 73).
3. Questionnaires âge et IA (80, 70), aucun « Raraka » public (73).
4. Mentions de page (éditeur, contact, avis Epic).

Avant la vente :
5. `Licenses/`, Crédits, EULA, politique « zéro collecte », opt-in crash, voix encadrée.
6. Suivi : redevance Unreal, TVA (Valve collecte, OSS seulement en vente directe, supposé), plafond micro, CRA (UE 2024/2847, dès 11/09/2026, supposé applicable).

## Quick wins
1. Geler la diffusion du build de recette.
2. Créer `docs/legal/` (`PRIVACY.md` « aucune donnée collectée », `EULA.md`, `LEGAL_NOTICE.md`).
3. Générer `Licenses/` pour les DLL livrées, renseigner `CompanyName` et `CopyrightNotice`.
4. Ajouter les tâches légales au board (91, 92).

## Questions qui challengent l'équipe
1. Qui vend (vous, micro-entreprise, société) et les investisseurs prennent-ils des parts ?
2. « Zéro donnée » (aucune politique à tenir) ou crash reporter opt-in (meilleur support, RGPD à tenir) ?
3. Si le code d'agents n'est pas protégeable, que vaut l'exclusivité, et prouvez-vous votre apport ?
