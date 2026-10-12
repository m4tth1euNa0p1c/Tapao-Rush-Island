# 73 — Lieu réel : données, noms, communauté
Domaine : H · Auditeur : juriste (lieux réels, données géographiques)

## Verdict
Le droit n'interdit ni le nom d'un atoll ni sa forme : le risque est contractuel (licences non prouvées, masques dérivés d'une imagerie Maxar « référence uniquement ») et réputationnel (piller et noyer un atoll habité d'une réserve de biosphère UNESCO, sans contact local). Les licences se réparent vite ; crédits, carte livrée et politique « lieu réel » ne sont pas prêts. Risque principal : une réaction publique de la commune ou de la presse polynésienne.

## Constats

- **[Bloquant commercial] Masques dérivés d'une imagerie sous licence restrictive**
  - Constat : hauts-fonds et masque de litière sont calculés pixel à pixel sur la mosaïque Maxar ; « Aucune imagerie Maxar embarquée » ne vaut que pour les pixels bruts. La DAF exclut de son ouverture les satellites sous licence contraignante.
  - Preuve : `build_terrain.py:238-275`, `README.md:64`, `fragments-tuamotu.md:400` ; https://www.service-public.pf/daf/?p=17254 (maj 2026-04-02). Statut de la couche IMAGE_PUBLIQUE : supposé restreint.
  - Recommandation : accord écrit de la DAF pour des dérivés commerciaux, sinon masques et fonds régénérés sans l'image (cocoteraies, bruit).
  - Effort : M

- **[Majeur] Licence citée peut-être fausse, preuve absente**
  - Constat : le dépôt dit partout « Etalab 2.0 » ; la DAF annonce ouvrir toutes ses données sous CC BY 4.0. Aucune capture de licence par couche ; atlas IRD sans licence consignée ; cocoteraies DAG : autre producteur que la DAF. Usage commercial permis sous attribution.
  - Preuve : `README.md:13-19`, `fetch_tefenua.py:4,12`, page DAF ; https://www.data.gouv.fr/pages/legal/licences/etalab-2.0.
  - Recommandation : `docs/reference/map/LICENCES.md` (par couche : métadonnées, licence, date, PDF) ; écrire à l'IRD et à la DAG.
  - Effort : S

- **[Majeur] Crédits en jeu non conformes**
  - Constat : une ligne, sans date de mise à jour (exigée par Etalab 2.0), sans IRD ni DAG, sans mention de transformation ni de non-caution. `DataCredits` des 20 îles n'est lu par aucun code ; vide = warning.
  - Preuve : `import_tapao_ui.py:44`, `setup_fragments.py:49-52`, `ExpeditionIslandDefinition.cpp:66-68`.
  - Recommandation : une ligne par jeu de données (producteur, licence et lien, date, « transformées, non officielles, impropres à la navigation ») ; vide = erreur.
  - Effort : S

- **[Majeur] « Aucun lieu réel » : le jeu se contredit**
  - Constat : `islands.md` interdit d'afficher « Raraka » et « Motutapu », les crédits affichent les deux, et le terrain suit le trait de côte exact d'un atoll identifiable. ADR-004 a tranché E-02 sur le gameplay, sans avis juridique ni local ; l'avertissement « personnages fictifs » protège peu.
  - Preuve : `islands.md:31-33`, `import_tapao_ui.py:44-45`.
  - Recommandation : (A) anonymiser tout texte visible ; (B) fictionnaliser la géométrie (miroir, rotation) ; (C) assumer avec accord écrit. Défaut : A maintenant, B avec #43, C si la commune accepte.
  - Effort : S (A), M (B)

- **[Majeur] Le build livre l'atoll entier, village réel et cadastre compris**
  - Constat : `Lvl_Raraka` est cuite, les 20 îles pointent dessus, le PlayerStart est au sud de Motutapu ; ADR-004 n'exclut le village que des fragments. 620 parcelles « masquées en jeu », probablement cuites : 15 attributs lus, ni propriétaire ni nom de terre, donc aucune donnée personnelle directe, mais inutiles.
  - Preuve : `DefaultGame.ini:29`, `README.md:138-139`, `fragments-tuamotu.md:363`.
  - Recommandation : `Lvl_Raraka` hors cook (comme #43), cadastre « editor only », test de build.
  - Effort : M

- **[Majeur] Aucun contact avec la commune, thème sensible**
  - Constat : Raraka relève de Fakarava (commune associée : Kauehi), n'a qu'un village et appartient à la réserve de biosphère UNESCO de Fakarava. Le jeu y met en scène pillage et submersion (motu « à 2–4 m »). Aucune consultation dans ADR, specs ou backlog.
  - Preuve : `README.md:3`, `backlog.md:48`, https://mab-france.org/en/biosphere-reserve/commune-of-fakarava/ ; contacts hors dépôt : supposés nuls.
  - Recommandation : courrier avant toute page publique (mairie de Fakarava, délégué de Kauehi, DAF) : présentation, relecture ; avis culturel : #72.
  - Effort : S, suivi M

- **[Mineur] Image des biens et noms : risque juridique faible**
  - Constat : aucun bâtiment réel modélisé d'après les docs (les 119 emprises servent à exclure), aucune photo ; un toponyme ne s'approprie pas ; image des biens : trouble anormal requis (Cass. ass. plén., 7 mai 2004, de mémoire). Droit polynésien : supposé, à confirmer.
  - Preuve : `README.md:43,105`.
  - Recommandation : une heure d'avocat à Papeete.
  - Effort : S

## Manques
1. Registre des licences par donnée, notices tierces (crédits, Steam, EULA).
2. Position écrite sur E-02 et clause de fiction.
3. MNT ou lidar de Raraka (ouverts selon la DAF) pour les hauteurs, aujourd'hui estimées.
4. Fiche « lieu » avant tout autre atoll (ADR-004:13).

## Quick wins
1. Réécrire `CREDITS` et `DATA_CREDITS` : producteur, licence, lien, date, transformation.
2. Archiver (PDF, URL, date) la licence de chaque couche.
3. Écrire à la DAF : licence par couche, dérivés Maxar, MNT.
4. Retirer « Raraka » et « Motutapu » des crédits, de Steam et des trailers jusqu'à E-02.

## Questions qui challengent l'équipe
1. Si la mairie de Fakarava exige le retrait la veille du lancement, que livrez-vous en sept jours ?
2. Quelle preuve datée montre que chaque couche cuite est réutilisable commercialement, Maxar compris ?
3. Qu'apporte à l'acheteur Steam le 1:1 d'un atoll qu'il ne reconnaît pas ?
