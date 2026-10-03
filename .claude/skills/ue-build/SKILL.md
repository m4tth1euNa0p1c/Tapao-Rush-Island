---
name: ue-build
description: Compile le code C++ de monke_island en choisissant la bonne méthode (Live Coding via MCP si l'éditeur est ouvert, sinon UBT via scripts/ue.ps1) et analyse les erreurs. À utiliser après toute modification C++.
---

# Build monke_island

## 1. État
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 status`

## 2. Choisir la méthode
| Situation | Méthode |
|-----------|---------|
| Éditeur fermé | `scripts/ue.ps1 build` |
| Éditeur ouvert, changements **dans le corps des fonctions .cpp** uniquement | Live Coding via MCP : `call_tool` toolset LiveCoding → `CompileLiveCoding` (faire `describe_toolset` d'abord) |
| Éditeur ouvert, **nouvelle UCLASS/USTRUCT/UENUM, UPROPERTY/UFUNCTION ajoutée ou modifiée, Build.cs ou .uproject modifié** | Live Coding ne suffit pas. Demande à l'utilisateur de fermer l'éditeur (ou de le laisser te le faire), puis `scripts/ue.ps1 build`, puis `scripts/ue.ps1 editor` |

Ne force jamais (`-Force`) un build UBT pendant que Live Coding est actif.

## 3. Lire le résultat
- Succès : `Result: Succeeded` / Live Coding `Patch applied`.
- Erreurs : récupère les lignes `error C####`, `error LNK####`, `Error:` (UHT). Corrige à la source.
- LNK / UHT obscurs → agent `ue-build-engineer`.

## 4. Rapport
Indique la méthode utilisée et la ligne de résultat exacte.
