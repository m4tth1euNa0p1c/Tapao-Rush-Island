---
name: ue-security-review
description: Revue de sécurité de monke_island selon docs/conventions/security.md — `code [dossier]` (RPC serveur, limitation de débit, fichiers et désérialisation, réplication) ; `secrets` (clés, jetons, mots de passe, clés de pak) ; `deps` (plugins, bibliothèques, scripts : provenance, licence, avis de sécurité) ; `shipping` (console, CheatManager, signature des paks, symboles) ; `threat-model` (STRIDE, docs/security/threat-model.md). À utiliser en REC sur du code réseau, de sauvegarde ou de fichiers, avant une publication ou un build distribué, ou à l'ajout d'un plugin.
argument-hint: "code [dossier] | secrets | deps | shipping | threat-model"
---

# Revue de sécurité

Mode : `$ARGUMENTS`. Par défaut, `code` sur les fichiers de la tâche, sinon sur `monke_island/Source/monke_island/`.
Norme : `docs/conventions/security.md`. Agent : `ue-security-engineer`.

## `code [dossier]`
1. **Surface d'attaque** :
   - `UFUNCTION(Server` et `UFUNCTION(NetMulticast` ;
   - `GetLifetimeReplicatedProps` ;
   - lectures de fichiers (`LoadGameFromSlot`, `FFileHelper::Load`, `IFileManager`, `FArchive <<`) ;
   - saisies joueur (noms de campagne, codes d'invitation, champs de l'éditeur d'îles).
2. **Pour chaque RPC `Server`** :

   | Contrôle | Ce qu'on vérifie |
   |----------|------------------|
   | SEC-02 | propriétaire de l'acteur appelant, `IsValid` sur chaque référence reçue, distance, ligne de vue, phase et état revérifiés côté serveur |
   | SEC-01 | aucune valeur décidée par le client (dégâts, loot, temps, validation) |
   | SEC-03 | délai minimal ou compteur par joueur |
   | — | nombres finis et bornés, tableaux de taille maximale |

3. **Fichiers** :
   - SEC-04 : aucun chemin tiré d'une saisie, `FPaths::ValidatePath`, écriture sous le dossier de sauvegarde uniquement ;
   - SEC-05 : version de schéma vérifiée, bornes vérifiées avant d'allouer, aucun `check` sur le contenu du fichier.
4. **Réplication** : rien de privé ni d'interne répliqué à tous (`COND_OwnerOnly`, par exemple).
5. **Abus** (sur le projet uniquement) : test automatisé ou PIE avec des valeurs hors bornes, des requêtes répétées, un fichier tronqué ou aléatoire. Le jeu refuse, consigne et ne plante pas.
6. **Rapport** dans `docs/audits/<date>-securite-<sujet>.md` (modèle `docs/templates/audit-report.md`). Pour une tâche en REC, ajouter dans sa note `revue-securite=OK`, ou la liste des écarts.

## `secrets`
- Chercher dans le dépôt (hors `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`), `Config/*.ini`, `scripts/`, `.mcp.json` et `docs/` :
  - motifs `(?i)(api[_-]?key|secret|token|password|passwd|authorization|bearer)\s*[:=]` ;
  - clés de pak (`EncryptionKey`, `SigningKey` dans `DefaultCrypto.ini`) ;
  - secrets EOS ou Steam ;
  - clés privées (`-----BEGIN`).
- Ne jamais recopier une valeur trouvée : citer le fichier, la ligne et le type de secret, puis recommander de le révoquer et de le changer (SEC-06).
- Si git existe, vérifier `.gitignore` (`Saved/`, `*.pem`, `DefaultCrypto.ini` si le dépôt devient public) et l'historique (`git log -p -S`).

## `deps`
- Inventorier :
  - plugins du `.uproject` et de `Plugins/` ;
  - modules du `Build.cs` ;
  - bibliothèques tierces (`ThirdParty/`) ;
  - outils Python et uv (`.mcp.json`, `scripts/`).
- Pour chacun : version, provenance (Epic, Fab, dépôt officiel), licence, accès au réseau ou aux fichiers, avis de sécurité connus (WebSearch). Payant ou sans licence claire = SEC-07, et arbitrage par l'utilisateur (§14).
- Tenir l'inventaire dans `docs/security/dependencies.md`.

## `shipping`
- Vérifier dans `Config/` et le code :
  - aucune réactivation de la console (`ALLOW_CONSOLE_IN_SHIPPING`) ;
  - aucun CheatManager ni commande de debug exposés ;
  - signature des paks configurée avant une distribution publique ;
  - symboles hors du package ;
  - aucun log verbeux forcé.
- Sur un build packagé : la console ne s'ouvre pas, les commandes de triche sont absentes (SEC-08).

## `threat-model`
- Mettre à jour `docs/security/threat-model.md` :
  - contexte et actifs ;
  - points d'entrée (RPC, fichiers, saisies, dépendances, sessions L7) ;
  - tableau STRIDE avec, pour chaque menace, son contrôle et la tâche correspondante ;
  - risques acceptés, chacun avec son ADR.
- Le modèle de départ est la section 1 de la norme.

## Règles
- Uniquement sur ce projet et cette machine : pas de scan réseau, pas d'outil offensif.
- Chaque constat est prouvé par le code ou par un test, avec fichier et ligne, ID SEC et, s'il y a lieu, CWE.
