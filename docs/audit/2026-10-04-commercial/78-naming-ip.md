# 78 — Nom et propriété intellectuelle (« Crownfall Expeditions »)
Domaine : H · Auditeur : conseil en marque et propriété intellectuelle

## Verdict
« Crownfall » n'est ni libre ni distinctif : un jeu Steam homonyme existe depuis 2017, Valve en a fait un événement Dota 2 en 2024, et le joueur ne voit même pas ce nom. Il n'est dans aucun .cpp, .h ni asset (grep : 0) : le changer coûte presque rien aujourd'hui. Risque principal : bâtir logo, capsule et page Steam sur un nom à abandonner ou à défendre.

## Constats

- **[Bloquant commercial] « Crownfall » existe déjà dans le même secteur**
  - Constat : Steam renvoie « CrownFall » (app 683730, action gratuit, sorti le 17/08/2017) et un module Pathfinder « Crownfall » (Fantasy Grounds). Dota 2 a eu un événement gratuit « Crownfall » (printemps 2024, 4 actes, prolongé jusqu'en octobre, selon la presse). Domaines crownfall.com, .net, .io, .gg déjà délégués (DNS local) ; réseaux sociaux non vérifiés. Recherche web de marques : rien trouvé, sans valeur (registres non indexés). Nom long (21 caractères), cumulant « Expeditions » et « -fall », motifs saturés (connaissance générale).
  - Preuve : https://store.steampowered.com/api/storesearch/?term=crownfall ; https://store.steampowered.com/api/appdetails?appids=683730 ; https://www.pcgamesn.com/dota-2/crownfall ; https://esports.gg/news/dota-2/dota-2-crownfall-extended-till-october/
  - Recommandation : abandonner ce nom : signe quasi identique, jeux vidéo (classes 9 et 41), même vitrine, donc confusion et opposition probables (supposé, hors avis juridique). Viser 14 caractères au plus, sous-titre descriptif sur Steam.
  - Effort : S

- **[Majeur] Quatre noms coexistent, aucun clearé**
  - Constat : « First Person Template » (réglages), « TAPAO : Rush Island » (logo, menus), « Crownfall Expeditions » (docs seulement), `monke_island` (module, exe). Ni CompanyName ni CopyrightNotice. L'audit 11 en cite trois ; TAPAO et Rush Island entrent dans la clearance s'ils restent.
  - Preuve : DefaultGame.ini:3 ; TapaoUISubsystem.cpp:638 ; art/ui/tapao/ui_manifest.json:3 ; art/ui/tapao/sprites/raw/logo.png (lu : « TAPAO », ruban « RUSH ISLAND »).
  - Recommandation : un seul nom, puis réglages projet, menu et logo alignés.
  - Effort : S (config) ; M (logo, capsule)

- **[Majeur] Aucune clearance ni plan de dépôt, alors que l'AppID approche**
  - Constat : grep INPI, EUIPO, trademark, nom de domaine dans docs/ hors audits : 0. L'audit 56 veut un AppID réel avant les playtests externes.
  - Preuve : 56-online-services.md:42.
  - Recommandation : méthode en 5 temps.
    1. Pré-tri (2 h) : Steam, SteamDB, IGDB, itch.io, Google exact, RDAP, namechk (X, TikTok, YouTube, Discord).
    2. Registres (½ j) : INPI (data.inpi.fr, identique + phonétique), EUIPO eSearch plus, TMview (UE, UK, USPTO), WIPO Global Brand Database, USPTO Trademark Search. Classes de Nice 9 et 41 (+ 28, 25 si dérivés). Écarter tout signe proche.
    3. Rapport d'antériorités d'un conseil en propriété industrielle sur 2 finalistes (supposé : quelques centaines d'euros).
    4. Dépôt INPI (date de priorité), extension UE, UK, US sous 6 mois (Convention de Paris) ; titulaire à décider.
    5. Ensuite seulement : domaines, comptes, AppID, logo, trailer.
  - Effort : M

- **[Majeur] Cinq alternatives courtes, deux déjà testées**
  - Constat : par ordre de préférence.
    1. **Drowned Assets** : « actifs » = butin, l'île se noie. Steam : 0 résultat ; .com et .gg sans délégation.
    2. **Sinking Fund** : fonds d'amortissement, île qui coule. Steam : 0 résultat ; .com pris (namefind, probablement parqué), .gg sans délégation ; web saturé de finance.
    3. **Tidewrit** : marée + « writ » (mandat royal), mot inventé. .com pris, .io et .gg sans délégation ; satire faible ; Steam non testé.
    4. **HMS Quota** : navire de Sa Majesté + quota, nom du galion. .com sans délégation ; Steam non testé.
    5. **Bean Counters** : « rond-de-cuir » + bean, peu maritime. .com pris ; expression courante, probablement utilisée ; non testé.
  - Preuve : https://store.steampowered.com/api/storesearch/?term=sinking%20fund et ?term=drowned%20assets ; DNS local du 2026-10-04 (sans délégation = probablement libre, non confirmé).
  - Recommandation : clearer 1 et 2 d'abord. Aucun lieu réel ni « tiki » dans le nom : les audits 72 et 73 peuvent les retirer.
  - Effort : S

- **[Mineur] `monke_island` rappelle « Monkey Island » et sort déjà du dépôt**
  - Constat : le build de recette investisseurs se lance par `monke_island.exe`. « Monkey Island » est une franchise Lucasfilm/Disney (connaissance générale) ; le jeu n'a aucun singe.
  - Preuve : docs/releases/v1-rec-investisseurs.md:7,17.
  - Recommandation : garder `monke_island` en nom de code (déjà renommé une fois : DefaultEngine.ini:88-89) ; exécutable, dossier d'installation et comptes publics au nom final.
  - Effort : S

## Manques
1. Nom final clearé, titulaire de la marque, tâche au backlog.
2. Logo et capsule au nom final (l'actuel dit TAPAO / RUSH ISLAND).
3. Mentions légales (copyright, marque) absentes des réglages.

## Quick wins
1. Geler toute publication de « Crownfall » (Steam, deck investisseurs, trailer) avant clearance.
2. Pré-tri de 2 h sur Drowned Assets et Sinking Fund (Google exact, namechk, TMview).
3. Renseigner ProjectName, CompanyName, CopyrightNotice ; nommer le dossier de livraison d'après le nom final.

## Questions qui challengent l'équipe
1. Si Valve ou l'éditeur du CrownFall de 2017 vous écrivait après la sortie, renommeriez-vous ?
2. Quel est le vrai nom : TAPAO, Rush Island, Crownfall ou autre ? Qui dépose : vous ou une société ?
3. La Couronne doit-elle figurer dans le nom, ou la satire comptable suffit-elle ?
