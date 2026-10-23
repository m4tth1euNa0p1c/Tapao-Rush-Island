// Tidemark plugin

using UnrealBuildTool;

public class TidemarkEditor : ModuleRules
{
	public TidemarkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Editor side: terrain import (Landscape, heightmaps, collision), first-wet bake, lint panel, commandlet.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"TidemarkRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"Landscape",
			"MeshDescription",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"EditorFramework",
			"Projects",
			// Import/: asset creation and registration (TidemarkImportLibrary).
			"AssetRegistry",
			// Import/TextureHeightSource: 16-bit PNG and EXR heightmaps through IImageWrapperModule::DecompressImage.
			"ImageWrapper",
			"ImageCore"
		});
	}
}
