# 84 — Stratégie Steam : page, wishlists, Next Fest, démo
Domaine : I · Auditeur : spécialiste Steam

## Verdict
Aucune stratégie Steam dans le dépôt : ni AppID, ni page. Valve impose un seul Next Fest par jeu et une démo soumise 3 à 5 semaines avant : le bon créneau est juin 2027, page ouverte le 18 janvier. Risque : griller ce Next Fest sous 3 000 wishlists, démo sans coop joignable.

## Constats

- **[Majeur] Aucun plan Steam, calendrier à rebours de Valve**
  - Constat : hors audits, `docs/` ne mentionne Steam qu'une fois (security.md:65), `Config/` jamais.
  - Preuve : grep, cahier §1:28 (publication exclue), backlog.md:35 (L7 en P2).
  - Recommandation :
    - **2026-10** accord de publier (cahier v1.5), entité et compte Steamworks (97), AppID, nom clearé (78).
    - **2026-11→2027-01** tranche RUN01–10, sessions Steam (56).
    - **01-18** page publique, **mars** Playtest public.
    - **04-26** go/no-go et inscription, **05-10** démo soumise, **06-03** démo publique, **06-14→21** Next Fest.
    - **Août–sept. 2027** sortie (89) si 7 000 wishlists ; 92 (sortie le 29 juin) tombe en Summer Sale.
  - Effort : M

- **[Bloquant commercial] Page « Coming soon » : six portes à fermer le 4 janvier**
  - Constat : elle conditionne Next Fest et Playtest public. Nom pris (78), aucun visuel (69).
  - Preuve : Valve (Next Fest) : « must be published and public ».
  - Recommandation : nom clearé, capsule et 5 captures 4K, bande-annonce de 75 s (86), textes EN, « Raraka » et « tiki » retirés (72, 73), questionnaire contenu et IA (80, 98). Ni date ni prix.
  - Effort : M

- **[Bloquant commercial] Next Fest : une seule participation, démo publique obligatoire**
  - Constat : page et démo publiques à l'ouverture. Démo soumise 5 semaines avant (presse) ou 3. Jeu sorti inéligible (accès anticipé : supposé). Éditions : 19 oct. 2026 manquée, 22 fév. 2027, 14 juin, octobre 2027 non publiée. Inscription ≈ 6 semaines avant (89).
  - Preuve : https://partner.steamgames.com/doc/marketing/upcoming_events/nextfest ; https://partner.steamgames.com/doc/marketing/upcoming_events (lus le 2026-10-04).
  - Recommandation : juin 2027 (≈ 3 000 wishlists d'entrée), pas février (inscription close le 10 janvier, démo due le 1er février, ≈ 700–1 600 d'entrée).
  - Effort : S

- **[Bloquant commercial] Démo « contrat d'essai » : 2 îles, 2 manches, coop**
  - Constat : impossible aujourd'hui : manche inachevée, coop par console, anglais absent.
  - Preuve : audits 11, 56, 79.
  - Recommandation : 30–40 min, manche 1 sur FRG_01 (essai, 13), manche 2 tirée (quota 400), solo ou 2–4 en invitation Steam, écran final « liste de souhaits », EN et FR ; repli, 1 île (89). Porte « démo » du 26 avril : RUN01–10 verts, 2 réseaux distincts, 3 PC neufs, 80 % de 30 testeurs finissent la manche 1 en 20 min (89).
  - Effort : L

- **[Majeur] Wishlists : cibles chiffrées**
  - Constat : 7 000 ≈ seuil « Popular Upcoming » (Zukowski). Semaine 1, conversion de 15 % sous 5 000, 20 % jusqu'à 40 000 (Carless). Next Fest, médiane +322 sous 1 000 d'entrée, tous les gagnants de 10 000+ entraient avec ≥ 3 000 (résumés non relus).
  - Preuve : https://presskit.gg/field-guides/how-many-wishlists-to-launch ; https://howtomarketagame.com/SNF ; https://newsletter.gamediscover.co/p/steam-demo-showcases-officially-good
  - Recommandation : 3 000 avant le Next Fest (143/semaine dès le 18 janvier ; 2 000 au 26 avril, sinon octobre 2027), +2 000 pendant (supposé), 7 000 à la sortie (≈ 1 050–1 400 ventes semaine 1).
  - Effort : M

- **[Majeur] Tags et description courte**
  - Constat : aucune liste.
  - Preuve : supposé (jugement).
  - Recommandation : Online Co-Op, Co-op, Multiplayer, Physics, Funny, Comedy, First/Third Person (E-01), Action, Adventure, Exploration, Cartoony, Stylized, Dark Humor, Satire, Atmospheric, Indie, 3D, Singleplayer, Sailing, Replay Value. Exclus : Extraction Shooter (public PvP), Horror, Pirates, Survival (81). Courte (EN) : « Row to a doomed island, haul fragile loot to your galleon before the sea rises, and sell it to a Crown that bills everything. 1–4 player online co-op. »
  - Effort : S

- **[Majeur] Playtest Steam avant la démo**
  - Constat : accès tiré au sort, sans NDA ni avis ; avant la page, clés confidentielles seulement.
  - Preuve : https://partner.steamgames.com/doc/features/playtest
  - Recommandation : nov.–janv. clés confidentielles, 8 puis 24 testeurs (94) ; mars, 3 week-ends de 150–300 testeurs (supposé).
  - Effort : M

- **[Mineur] Festivals, prix, langues**
  - Constat : aucun festival listé jusqu'en juin 2027 ne vise le co-op en ligne, l'extraction ou les pirates (Couch Co-Op Fest : local) ; « Coming soon » avec démo éligible.
  - Preuve : https://partner.steamgames.com/doc/marketing/upcoming_events (2026-10-04).
  - Recommandation : surveiller la page. Prix masqué (décision : 85). Page EN puis FR, cocher seulement l'interface livrée, ni « manette » ni « voix de proximité » avant 26, 56.
  - Effort : S

## Manques
1. Décision de publier (cahier v1.5), entité, AppID.
2. Capsule, trailer, build démo, interface EN.

## Quick wins
1. Lancer l'inscription Steamworks (W-8BEN : 10–15 jours, 97), sans publier.
2. Tableau de 8 pages concurrentes.
3. `docs/marketing/steam-page.md` : tags, textes, interdits.

## Questions qui challengent l'équipe
1. Signez-vous un cahier v1.5 autorisant la publication, et tranchez-vous nom, Raraka, tiki avant la page ?
2. Démo moyenne en février ou désirable en juin : laquelle brûle l'unique Next Fest ?
3. Sous 2 000 wishlists le 26 avril : reportez-vous le Next Fest et la sortie ?
