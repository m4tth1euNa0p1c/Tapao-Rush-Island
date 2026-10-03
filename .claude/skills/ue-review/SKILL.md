---
name: ue-review
description: Lance une code review C++ Unreal (agent ue-code-reviewer) sur les changements en cours ou des fichiers donnés, puis propose/applique les corrections.
argument-hint: [fichiers ou dossier]
---

# Review

Cible : `$ARGUMENTS` (défaut : fichiers modifiés — `git status` si le repo est sous git, sinon demande quels fichiers).

1. Lance l'agent `ue-code-reviewer` avec la liste de fichiers.
2. Présente les points triés par sévérité.
3. Corrige les CRITIQUE et MAJEUR (ou demande à l'utilisateur pour ceux qui changent le comportement), puis skill `ue-build`.
4. MINEUR : liste-les, corrige seulement si l'utilisateur le demande.
