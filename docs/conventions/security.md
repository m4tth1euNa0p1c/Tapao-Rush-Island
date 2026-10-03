# Normes de sécurité

Couvre le jeu coopératif (1 à 4 joueurs en listen server), ses fichiers locaux, la chaîne d'outils et la vie privée.

**Références** :
- Microsoft SDL, avec la modélisation des menaces STRIDE ;
- CWE Top 25 et SEI CERT C++ Coding Standard ;
- OWASP ASVS, si un service web est ajouté ;
- documentation Epic *Networking Overview* (serveur autoritaire, validation des RPC) ;
- RGPD (pseudos, voix au L7) ;
- cahier §4, §9, §14, §17, §18, §20, §21.

## 1. Modèle de menace
**Contexte** :
- l'hôte est un joueur et fait autorité ;
- sauvegardes locales de l'hôte ;
- aucun backend, aucun achat réel ;
- sessions privées au L7.

**Ce qu'on protège** :
- l'intégrité d'une partie pour l'équipage ;
- la progression de l'hôte ;
- la machine du joueur (fichiers, réseau) ;
- les données personnelles ;
- les clés de signature des builds.

| STRIDE | Exemple | Contrôle |
|--------|---------|----------|
| Usurpation | un client agit au nom d'un autre joueur ou d'un acteur qu'il ne possède pas | requêtes uniquement via un acteur possédé ; le serveur revérifie le propriétaire et les références |
| Falsification | dégâts, valeur de loot ou positions trafiqués par un client ; sauvegarde éditée à la main | le serveur décide de tout (§4, §9) ; valeurs bornées et validées ; sauvegarde versionnée avec somme de contrôle, qui détecte la corruption sans prétendre bloquer la triche |
| Répudiation | contestation d'un bilan | journal serveur des résolutions par `ExpeditionId` |
| Divulgation | logs ou « copier le résumé technique » contenant une IP, un chemin utilisateur ou un pseudo ; clé dans le dépôt | aucune donnée personnelle ni secret dans les logs ou les copies (§18) ; secrets hors dépôt |
| Déni de service | rafale de requêtes, tableau géant envoyé au serveur, projet d'île ou sauvegarde énorme | limitation de débit par joueur, tailles bornées (§20), délais maximaux (60 s au §16) |
| Élévation de privilèges | console ou commandes de triche en Shipping ; exécution de contenu téléchargé | en Shipping, ni console ni CheatManager ; aucune exécution de code ou script venu de l'extérieur |

**Risque accepté** : en listen server, l'hôte peut tricher. C'est acceptable en coopération privée ; il n'y a pas d'anti-triche côté client.

## 2. Code réseau : ne jamais faire confiance au client
- **Contrôles de chaque RPC `Server`** :
  - l'appelant possède l'acteur d'entrée ;
  - la cible existe (`IsValid`) et est pertinente ;
  - distance, ligne de vue, phase de la partie et état du joueur sont cohérents ;
  - les nombres sont finis (`FMath::IsFinite`, `ContainsNaN()`) et bornés (`FMath::Clamp`) ;
  - les tableaux ont une taille maximale.
- **`WithValidation`** (`_Validate`) rejette les entrées impossibles en temps normal, ce qui déconnecte le client. Les refus métier (trop loin, mauvaise phase) se traitent dans `_Implementation` : refus et log `Warning`, sans déconnexion.
- **Limitation de débit** : un délai minimal ou un compteur par joueur, côté serveur, pour toute requête répétable (interaction, vote, achat, sermon, placement).
- **Fiabilité** : un RPC fiable seulement quand c'est nécessaire, jamais à chaque image (§9).
- **Réplication minimale** : `COND_OwnerOnly` pour ce qui n'appartient qu'à un joueur. Jamais d'état serveur interne répliqué sans raison.

## 3. Fichiers et données
- **Chemins** : jamais un nom saisi par le joueur comme chemin de fichier (§17). Les fichiers sont nommés par identifiant interne (`FGuid`). Tout chemin construit est vérifié avec `FPaths::ValidatePath` et `FPaths::IsUnderDirectory(Chemin, FPaths::ProjectSavedDir())`.
- **Désérialisation défensive** (sauvegardes, projets d'île, paramètres) :
  - version de schéma vérifiée ; une version future est refusée sans crash ;
  - tailles et valeurs bornées avant toute allocation ;
  - `AssetId` présents dans une liste autorisée (§20).

  Un fichier tronqué ou aléatoire ne doit jamais faire planter le jeu : c'est testé.
- **Écriture** : uniquement sous le dossier de sauvegarde de l'utilisateur. Aucun accès en dehors.
- **Exécution** : aucune exécution de code, script ou commande console venant d'un fichier, du réseau ou d'une saisie.

## 4. Secrets et chaîne d'approvisionnement
- **Secrets hors dépôt** :
  - clés d'API, mots de passe, jetons ;
  - clés de signature et de chiffrement des paks (`Config/DefaultCrypto.ini`) ;
  - secrets de plateforme (EOS, Steam).

  Le skill `/ue-security-review secrets` scanne le dépôt avant un commit ou une publication.
- **Plugins et bibliothèques** :
  - provenance connue (Epic, Fab, dépôt officiel) ;
  - version figée, licence notée dans `art/SOURCES.md` ou un ADR ;
  - aucun plugin payant sans accord (§14) ;
  - accès au réseau et aux fichiers relu.
- **Scripts outils** (Python, PowerShell) : HTTPS uniquement, aucun `eval` ni `exec` de données distantes, sources tracées.
- **Outils de développement** : le serveur MCP Unreal écoute sur `127.0.0.1` seulement, jamais sur le réseau. Le MCP Blender reste local.

## 5. Build Shipping
- La console est désactivée (`ALLOW_CONSOLE_IN_SHIPPING` vaut 0 par défaut) et le CheatManager absent (`UE_WITH_CHEAT_MANAGER` vaut `!UE_BUILD_SHIPPING`). Aucune commande de debug n'est réactivée par un `.ini` modifiable.
- Paks signés (*Project Settings > Crypto*) avant toute distribution publique, ce qui détecte un contenu modifié.
- Symboles (`.pdb`) archivés hors du package livré.

## 6. Vie privée
- Données personnelles minimales : pseudo de plateforme ; voix au L7.
  - Le push-to-talk est actif par défaut.
  - On peut couper chaque joueur.
  - Un indicateur montre quand le micro est ouvert.
- Pas de télémétrie ni d'envoi automatique de rapport de crash sans consentement explicite.
- Les rapports et le « résumé technique » (§18) ne contiennent ni IP, ni chemins utilisateur, ni pseudos.

## 7. Contrôles de la revue de sécurité (`/ue-security-review`)
| ID | Contrôle | Sévérité par défaut |
|----|----------|---------------------|
| SEC-01 | Décision de jeu prise ou crue côté client (dégâts, loot, temps, validation de zone) | CRITIQUE |
| SEC-02 | RPC `Server` sans vérification du propriétaire, de la cible ou des bornes | CRITIQUE |
| SEC-03 | Requête répétable sans limitation de débit | MAJEUR |
| SEC-04 | Chemin de fichier dérivé d'une saisie, ou écriture hors du dossier de sauvegarde | CRITIQUE |
| SEC-05 | Désérialisation sans version, sans borne, ou qui plante sur une entrée malformée | MAJEUR |
| SEC-06 | Secret ou clé dans le dépôt, un `.ini` ou un log | CRITIQUE |
| SEC-07 | Plugin ou dépendance sans provenance ou licence documentée | MAJEUR |
| SEC-08 | Console, CheatManager ou commande de debug accessible en Shipping | MAJEUR |
| SEC-09 | Donnée personnelle dans un log, un rapport ou une copie de diagnostic | MAJEUR |
| SEC-10 | Exécution de contenu externe (script, commande, code) | CRITIQUE |
