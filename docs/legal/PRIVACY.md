# Confidentialité — prototype Crownfall Expeditions

**Brouillon à faire relire par un avocat. Non publié. État vérifié le 4 octobre 2026.**

## Aucune donnée collectée aujourd'hui par l'équipe via le jeu

Le prototype examiné ne comporte ni compte utilisateur géré par l'équipe, ni
publicité, ni service de mesure distant, ni serveur de collecte exploité par
l'équipe. Aucun envoi automatique de statistiques de partie vers l'équipe n'a
été identifié dans le code ou la configuration. Cette constatation est une revue
des sources ; elle ne remplace pas une mesure du trafic du package final.

Le jeu crée des fichiers **locaux** : réglages, sauvegardes et journaux Unreal.
Les journaux bruts peuvent contenir le nom du compte système, le nom de machine,
des chemins personnels et des informations techniques. Ils ne sont donc pas
anonymes et ne doivent pas être confondus avec une collecte distante inexistante.
Ils restent sur l'ordinateur, selon les mécanismes de conservation du moteur et
du système. Les sauvegardes ne sont pas effacées automatiquement à la fermeture.

L'outil de diagnostic T-129 produit localement un CSV limité aux événements de
partie reconnus et un résumé. Il écarte les lignes User/Computer, noms d'acteurs
et chemins. Il n'envoie rien. Un joueur peut choisir de transmettre un diagnostic
après lecture ; aucun canal de réception ni délai de conservation par l'équipe
n'est actuellement défini. Ils devront être indiqués avant un tel programme.

Une partie réseau implique des communications avec l'hôte et les autres clients.
« Aucune collecte par l'équipe » ne signifie ni absence de réseau, ni invisibilité
de l'adresse réseau pour l'hôte. Les pratiques d'une plateforme ou d'un service
tiers relèvent aussi de sa propre information de confidentialité.

## Évolutions à annoncer avant activation

La présence d'un Crash Reporter dans un futur package Development ne vaut pas
autorisation d'envoi. Le destinataire, le contenu, les réglages d'envoi et le
consentement devront être vérifiés sur le package. T-127 prévoit son inclusion,
sans installer de service de collecte. Les crash dumps peuvent contenir de la
mémoire et nécessitent une revue distincte de celle du CSV.

Steam, voix, télémétrie distante et assistance avec transmission de fichiers
devront faire l'objet d'une version révisée : responsable, finalités, base légale,
destinataires, transferts, conservation, droits et choix de l'utilisateur. Aucun
consentement à ces fonctionnalités futures n'est demandé par ce brouillon.

Responsable et contact : **à compléter par le propriétaire avant diffusion**.
Pour les droits applicables, consulter la [CNIL](https://www.cnil.fr/fr/mes-demarches/les-droits-pour-maitriser-vos-donnees-personnelles).

Preuves locales : `monke_island.Build.cs`, `Config/`, audit 95/97, contrat
`docs/features/local-events.md` sur dev/T-129. La date de cette page n'atteste pas
la conformité d'un package ultérieur.
