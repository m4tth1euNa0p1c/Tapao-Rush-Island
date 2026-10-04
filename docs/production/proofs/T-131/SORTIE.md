T-131 est en **review** sur `dev/T-131`.

T-054, T-055 et T-057 sont fusionnées sans conflit. La pacification appelle
`AddZoneBonus`, avec refus des validations tardives ; défaut d'horloge à 540 s.
Huit specs de raccord et deux specs du Standard ajoutées. Prérequis de compilation
et montage des tests Save corrigés.

Preuve : cible jeu compilée sur `eb1d46c`, résultat `Succeeded` ; journaux et
empreintes dans ce dossier. Build éditeur et tests headless **non exécutés** :
l'éditeur d'une autre session reste ouvert, sa fermeture étant interdite par le mandat.
La ressource est ensuite réservée par `monk-island-aa`.

Report main arrêté : quatre fichiers joueur ont des modifications non commitées.
Faire versionner ces changements par leurs propriétaires, reprendre main dans
T-131, valider les 77 cas ciblés puis la suite complète avant report. Détails : README.
Risques restants : intégration en carte, entrées joueur, sauvegarde de manches, réseau non éprouvés.
