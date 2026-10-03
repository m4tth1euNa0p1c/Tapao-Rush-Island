---
name: ue-import
description: Importe des fichiers dans le projet Unreal avec les bonnes conventions — textures (PBR, masques, HDRI), meshes (FBX/OBJ/glTF), audio et autres — via le MCP quand c'est possible, sinon via le commandlet ImportAssets ou l'import manuel.
argument-hint: "<fichiers ou dossier> [dossier de destination]"
---

# Import d'assets

Sources : $ARGUMENTS

## Ce que le MCP sait importer
| Type | Outil | Ensuite |
|------|-------|---------|
| Image → Texture2D | `editor_toolset.toolsets.texture.TextureTools.import_file` | réglages via `ObjectTools.set_properties` (compression, sRGB) après `list_properties` |
| Mesh (fbx, obj, glb) → StaticMesh | `editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file` | `set_nanite_enabled`, `generate_convex_collisions`, `set_material` |

Le reste (sons, skeletal meshes, HDRI en `.hdr`/`.exr`, etc.) passe soit par **glisser-déposer** dans le Content Browser (le demander à l'utilisateur), soit par le **commandlet**, **éditeur fermé** :
```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<uproject>" -run=ImportAssets -source="<fichier ou dossier>" -dest="/Game/MonkeIsland/<...>" -replaceexisting -nosourcecontrol
```

## Conventions (voir `docs/conventions/asset-naming.md`)
- Destination : `/Game/MonkeIsland/<Feature>/<Type>/`. Pour l'environnement : `/Game/MonkeIsland/Environment/<Catégorie>/`.
- Textures : `T_<Nom>_D` (base color, sRGB), `T_<Nom>_N` (normal, compression Normalmap, format OpenGL → cocher Flip Green si la source est DirectX… et inversement : les sources Poly Haven `nor_gl` sont OpenGL, UE attend DirectX), `T_<Nom>_ORM` ou `_R` / `_AO` (linéaires, sRGB décoché, compression Masks).
- Meshes : `SM_<Nom>` ; Nanite activé pour les meshes détaillés et le terrain.
- Toujours vérifier le résultat : `CaptureAssetImage`, dimensions (`get_bounds`), puis `save_assets`.

## Sources du projet
- Textures PBR CC0 : `python scripts/fetch_atoll_textures.py` → `art/textures/<catégorie>/<asset>/`, HDRI → `art/hdri/`.
- Exports Blender : `art/export/<Feature>/`.
- Terrain : `docs/reference/map/<ile>/terrain/`.
