# Mémoire des îles jouées — T-128

Complément de `fragments-tuamotu.md`, rapports 63 et 10 vérifiés dans le code.

`DA_FragmentRules.RecentIslandCount` vaut 8 par défaut (0 désactive la mémoire,
borne technique 1024). Le serveur conserve les identifiants des N dernières
expéditions démarrées dans `UExpeditionIslandSubsystem`, à travers les voyages.
Une sélection annulée ou un voyage échoué ne consomme pas d'entrée. Le raccord
à `StartExpedition` ajoute quatre lignes au GameState partagé.

Le tirage garde l'ordre du catalogue et utilise `FRandomStream` : même catalogue,
même historique et même graine donnent le même résultat. Si toutes les îles sont
récentes, seule la moins récemment jouée redevient éligible. Les identifiants
sortis du catalogue sont ignorés. Un catalogue vide reste une erreur ; un catalogue
d'une seule île permet sa répétition. Les anciennes signatures restent disponibles.

Contrôles : ARC-03 (valeur en DA), ARC-04/MEM-08 (mémoire d'instance bornée),
SEC-01 (enregistrement et tirage serveur), ERR-09 (six specs de calcul).
La mémoire n'est pas sauvegardée sur disque : le raccord à SG_Run relève de T-115.
La compilation cible jeu ne valide ni les specs EditorContext, ni les voyages,
ni la valeur sérialisée de l'asset ; gate éditeur et recette réseau restent requis.

Validation du 2026-10-04 : `worktree.py build T-128`, base `383d923`, échec.
UHT passe ; UBT échoue sur des dépendances de la base non commitées dans main :
`AIslanderCharacter::SetForceThirdPerson` (BoatRiderComponent),
`UIslanderMovementComponent::bExhaustionKillsInWater` (BoatPhysicsSpec),
`EAnchorageRole::Galleon` (GalleonSpawner). Aucun diagnostic sur les fichiers
ajoutés par T-128. Cela ne constitue pas une compilation complète réussie.
Six specs écrites, non exécutées ; aucun test éditeur ou réseau.
