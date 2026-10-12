# 89 — Plan de sortie et MVP vendable
Domaine : I · Auditeur : producteur release

## Verdict
Aucun plan de sortie : le cahier exclut la publication (§1) et le board (125 tâches, dont 22 `fait`, 81 `review`, 18 `a-faire`, 4 `en-cours`) n'a aucune tâche Steam, démo publique, playtest, tutoriel ou internet. Trajectoire proposée : tranche verticale fin nov. 2026, playtest Steam dès déc., démo au Next Fest de juin 2027, accès anticipé (EA) août–sept. 2027, 1.0 mi-2028. Risque : griller l'unique Next Fest sur une boucle jamais testée à deux machines.

## Constats

- **[Bloquant commercial] MVP vendable : 7 conditions, aucune validée**
  - Constat : (1) Boucle RUN01–14 : T-112 à T-116 `a-faire`, pacification, arrimage et sauvegarde (T-054/055/057) hors `main`. (2) Internet : aucune tâche L7, 0 test à deux machines. (3) Sauvegarde de partie : T-115. (4) Îles : 1 carte (`L_Frag_Raraka_01`) sur 3–5, ADR-004 renvoie les autres en L8. (5) Tutoriel : 0 tâche. (6) Build : 3 packages Development vérifiés par logs, perf jamais mesurée. (7) Identité : « First Person Template », nom pris, FR seul.
  - Preuve : board.md, ADR-005, backlog.md:35, `git merge-base`, Build.cs:49, DefaultGame.ini:3.
  - Recommandation : figer ces 7 conditions, anglais inclus. MVP = 4 îles héros, 16 fragments en réserve. Refuser factions, cyclone et E1 avant l'EA. L7a (sessions Steam, 2–3 sem. selon 56) passe avant le packaging L6.
  - Effort : L

- **[Majeur] Aucun jalon ni critère go/no-go**
  - Constat : aucune date au cahier (§11), aucun lot « Sortie », la porte PROD juge un lot, pas une sortie.
  - Preuve : cahier §1, §11, §15, FACTORY.md:19, v1-rec-investisseurs.md:53.
  - Recommandation : cahier v1.4 (lot L9), accord écrit du propriétaire avant publication (§14). Seuils proposés, un no-go reporte la date, jamais le critère :
    - **J1 Tranche verticale** (L3-L4), 30 nov. 2026, `testé build` : RUN01, 04, 06–11 en solo et RUN02, 03, 05 en LAN, puis 10 manches sans crash ni duplication. Sinon gel du contenu.
    - **J2 Playtest Steam** (L5, L7a), déc.–mars, `testé multi-machines` : 2 réseaux (NET01), RUN12–13, 8 testeurs externes, ≥ 90 % de sessions sans crash.
    - **J3 Démo** (L6), 14–21 juin 2027 : 4 novices sur 5 finissent la manche 1 en ≤ 20 min, chargement < 30 s sur HDD, médiane 16,6 ms sur la machine de référence.
    - **J4 EA** (L7-L8 partiel), août–sept. 2027 : MVP complet, 4 joueurs 60 min sans désynchronisation, crash < 2 %, souhaits au seuil fixé avec 84.
    - **J5 Mises à jour** : voir cadence. **J6 1.0**, EA + 9–12 mois : manche finale (19), 8–10 îles, avis ≥ 80 % positifs.
  - Effort : S

- **[Majeur] Le calendrier Steam impose Playtest, Next Fest, puis EA**
  - Constat : Next Fest 22 fév.–1er mars 2027 (inscription avant le 10 janv., démo et page publiques), puis 14–21 juin. Une seule participation, sortie après la fête (supposé : l'EA compte). Page : revue 3–5 jours ouvrés, « coming soon » ≥ 2 semaines. Playtest : gratuit, public sans NDA, clés privées possibles. EA : jeu jouable, aucune promesse précise, mise à jour annuelle minimum.
  - Preuve : partner.steamgames.com/doc/marketing/upcoming_events/nextfest/feb_2027, /doc/store/releasing, /doc/features/playtest, /doc/store/earlyaccess, notebookcheck.net (juin).
  - Recommandation : privé puis public. Décision le 10 janv. : février seulement si J1 passé et J2 privé tenu, sinon juin. EA 4–10 semaines après (supposé).
  - Effort : S

- **[Majeur] Démo et portes hors code : rien d'écrit**
  - Constat : la démo, app Steam distincte (supposé), n'a aucune tâche. Quatre portes avant la 1re page publique : nom (« Crownfall » existe depuis 2017, 78), licences et masques Maxar (73), revue culturelle (72), signature (48).
  - Preuve : board.md, audits 48, 72, 73, 78.
  - Recommandation : démo = contrat d'essai sur FRG_01 (départ 9 h, 2 manches non éliminatoires), même build sous indicateur. Rien de public avant les 4 portes.
  - Effort : L

- **[Majeur] Cadence post-lancement : un seul humain valide**
  - Constat : 81 tâches `review` attendent le propriétaire, sans CI (36). Un rythme hebdomadaire est intenable (supposé).
  - Preuve : board.md, audit 36.
  - Recommandation : hotfix ≤ 72 h (crash, duplication, perte de sauvegarde). Correctifs à 15 jours (3 mois), puis mensuels. Contenu toutes les 8 semaines, gel 1 semaine avant, bêta Steam 24 h. Porte : Automation, `nettest`, smoke packagé. Plan interne (recette et coût par lot, §11) : U1 (EA + 1 mois) voix de proximité (au MVP si le playtest l'exige), accessibilité, 1–2 îles, cosmétiques. U2 (+ 3) cyclone, Rôdeur des Ronces, contrats à modificateurs. U3 (+ 5) élite, 2 îles, faction non hostile. U4 (+ 7) atoll n° 2 après avis juridique, classements. E1 décidé sur la rétention.
  - Effort : M

## Manques
1. Lot L9 « Sortie » : AppID, page, EULA, signature, crash reporting, support.
2. L7a sessions Steam.
3. Tutoriel et démo.
4. Quatre îles héros.
5. Machine de référence (§12), `recette.md`.

## Quick wins
1. Six tâches de sortie au board, avec niveau de preuve.
2. Machine de référence, frametime et essai 2 PC sur `build/rec-T124`.
3. Fusionner T-054/055/057, lancer leurs specs.

## Questions qui challengent l'équipe
1. Si J1 manque le 30 nov., renoncez-vous au Next Fest de février ?
2. Un MVP à 4 îles, sans E1 ni factions, vous suffit-il ?
3. Seul testeur, qui assure hotfix 72 h et correctifs à 15 jours ?
