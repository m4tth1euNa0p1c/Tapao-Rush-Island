# Release <version> — YYYY-MM-DD

## Avant le build
- [ ] Chantier : aucun bug `crash` ou `bloquant` ouvert (`python scripts/chantier.py`)
- [ ] Build éditeur OK (`scripts/ue.ps1 build`)
- [ ] Tests OK (`scripts/ue.ps1 test`) — nombre passés / échoués :
- [ ] Assets sauvegardés, aucun asset de test laissé dans `/Game/MonkeIsland`
- [ ] Crédits à jour (`art/SOURCES.md`) ; imagerie satellite **non** embarquée

## Build
- [ ] `scripts/ue.ps1 package -Config <Development|Shipping>` → `build/package/Windows/`
- [ ] Taille du package :

## Smoke test du package
- [ ] Lancement, menu, map principale atteinte, sortie propre
- [ ] Log sans erreur bloquante
- [ ] Perf de référence (skill `ue-profile` sur le package)

## Notes de version
Nouveautés, corrections, problèmes connus.
