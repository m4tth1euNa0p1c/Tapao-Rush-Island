---
name: ue-ui-widget
description: Crée un widget UI monke_island — classe C++ de base (UserWidget) + Widget Blueprint WBP_ construit et câblé via le MCP UMGToolSet — puis l'affiche et le vérifie en PIE.
argument-hint: "<NomDuWidget> <rôle>"
---

# Widget UI

Widget : $ARGUMENTS

1. **C++** : skill `ue-new-class` avec le type `UserWidget` (logique, données, événements `BlueprintImplementableEvent` vers le BP). Compiler (skill `ue-build`, éditeur fermé si nouvelle classe).
2. **MCP** : `describe_toolset` sur `UMGToolSet.UMGToolSet`, puis :
   - créer `WBP_<Nom>` dans `/Game/MonkeIsland/UI/` avec la classe C++ comme parent ;
   - construire l'arbre de widgets ; les noms doivent correspondre aux `BindWidget` du C++ ;
   - **pour chaque widget renvoyé** : `ObjectTools.list_properties` → `get_properties` → `set_properties` avec les noms exacts (sinon échec silencieux).
3. **Affichage** : création et ajout au viewport par le PlayerController ou le HUD (pas par le widget lui-même).
4. **Vérification** : `StartPIE` + `CaptureViewport`, aux résolutions 1080p et 1440p si possible.
5. Textes : String Table (`StringTableTools`), pas de texte en dur.

## Règles
- Accessibilité : taille de texte ≥ 18 px à 1080p, contraste, pas d'information portée par la seule couleur.
- Pas de binding de propriété par tick pour des valeurs rares : événements.
