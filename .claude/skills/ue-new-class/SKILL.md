---
name: ue-new-class
description: Crée une nouvelle classe C++ Unreal dans monke_island à partir des templates du projet (Actor, ActorComponent, Character, PlayerController, GameMode, UserWidget, AIController, StateTreeTask, subsystems, DataAsset, DeveloperSettings, BlueprintFunctionLibrary, NativeGameplayTags, SaveGame, Types, Interface, AutomationTest). À utiliser dès qu'il faut ajouter une classe C++.
argument-hint: "<Type> <NomClasse> [Dossier] [description]"
---

# Nouvelle classe C++

Arguments : $ARGUMENTS

## Templates (`templates/` à côté de ce fichier)
| Type | Fichiers | Classe générée | Notes |
|------|----------|----------------|-------|
| Actor | `Actor.h/.cpp.tpl` | `A<Nom>` | tick désactivé |
| ActorComponent | `ActorComponent.h/.cpp.tpl` | `U<Nom>` | |
| Character | `Character.h/.cpp.tpl` | `A<Nom>` | Enhanced Input (move/look) |
| PlayerController | `PlayerController.h/.cpp.tpl` | `A<Nom>` | mapping contexts |
| GameMode | `GameMode.h/.cpp.tpl` | `A<Nom>` | classes assignées en BP |
| UserWidget | `UserWidget.h/.cpp.tpl` | `U<Nom>` | `BindWidgetOptional`, événement BP |
| AIController | `AIController.h/.cpp.tpl` | `A<Nom>` | StateTreeAIComponent + AIPerception |
| StateTreeTask | `StateTreeTask.h/.cpp.tpl` | `F<Nom>` + `F<Nom>InstanceData` | `{{DisplayName}}` ; nom conseillé `StateTree<Action>Task` |
| WorldSubsystem | `WorldSubsystem.h/.cpp.tpl` | `U<Nom>` | mondes de jeu seulement |
| GameInstanceSubsystem | `GameInstanceSubsystem.h/.cpp.tpl` | `U<Nom>` | |
| DataAsset | `DataAsset.h.tpl` | `U<Nom>` | header seul |
| DeveloperSettings | `DeveloperSettings.h.tpl` | `U<Nom>` | header seul, `{{DisplayName}}` ; module `DeveloperSettings` si erreur de link |
| BlueprintFunctionLibrary | `BlueprintFunctionLibrary.h/.cpp.tpl` | `U<Nom>` | |
| NativeGameplayTags | `NativeGameplayTags.h/.cpp.tpl` | tags dans `MonkeTags::` | `{{TagVar}}`, `{{TagName}}` ; module `GameplayTags` requis |
| SaveGame | `SaveGame.h.tpl` | `U<Nom>` | header seul, version de sauvegarde |
| Types | `Types.h.tpl` | `E<EnumName>`, `F<StructName>` | header seul |
| Interface | `Interface.h.tpl` | `U<Nom>` / `I<Nom>` | header seul |
| AutomationTest | `AutomationTest.cpp.tpl` | spec `<Nom>Spec` | `{{TestPath}}` |

Autre type (Pawn, HUD, GameState, AnimInstance…) : partir du template le plus proche et d'une classe équivalente existante dans `Source/monke_island/`.

## Étapes
1. `NomClasse` **sans** préfixe (`HealthComponent`, pas `UHealthComponent`) : le template ajoute le préfixe.
2. Dossier : `monke_island/Source/monke_island/<Dossier>/` (par feature). Vérifier qu'aucune classe du même nom n'existe (Grep).
3. Lire le template et remplacer les placeholders :
   - `{{ClassName}}` → NomClasse
   - `{{IncludePath}}` → `<Dossier>/` (vide si racine du module)
   - `{{Description}}` → une phrase
   - `{{DisplayName}}`, `{{TagVar}}`, `{{TagName}}`, `{{EnumName}}`, `{{StructName}}`, `{{TestPath}}` selon le type
4. Écrire le .h (et le .cpp).
5. Nouveau module requis → `monke_island.Build.cs` (ex. `GameplayTags`, `DeveloperSettings`, `NavigationSystem`).
6. Compiler via la skill `ue-build`. Une nouvelle UCLASS / USTRUCT exige un build complet, éditeur fermé : Live Coding ne suffit pas.
