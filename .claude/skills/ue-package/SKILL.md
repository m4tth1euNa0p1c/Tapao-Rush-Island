---
name: ue-package
description: Package le jeu monke_island pour Windows (BuildCookRun) en Development ou Shipping, vérifie le résultat et prépare une livraison avec la checklist de release.
argument-hint: "[Development|Shipping]"
---

# Packaging

Configuration : $ARGUMENTS (défaut Development)

1. Pré-requis : build éditeur OK (skill `ue-build`), tests OK (skill `ue-test`), pas de bug `crash` ou `bloquant` ouvert (`python scripts/chantier.py`).
2. Lancer (long, timeout 10 min, à relancer en arrière-plan si nécessaire) :
   ```
   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 package -Config <config>
   ```
   Sortie : `build/package/Windows/` (ignoré par git).
3. Échec : chercher la première ligne `Error:` dans la sortie UAT (cook : asset fautif ou référence cassée), puis agent `ue-build-engineer`.
4. Smoke test : lancer l'exe packagé, atteindre la map principale, quitter proprement, et lire le log (`build/package/Windows/monke_island/Saved/Logs/`).
5. Release : checklist `docs/templates/release-checklist.md` → `docs/releases/<version>.md`.
