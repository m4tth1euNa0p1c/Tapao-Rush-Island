// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class monke_island : ModuleRules
{
	public monke_island(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"ProceduralMeshComponent"
		});

		// loading screen between maps (UI/Tapao)
		PrivateDependencyModuleNames.AddRange(new string[] { "MoviePlayer" });

		// editor tools (vegetation reimport keeping the asset identities): never in a game build
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "Json" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"monke_island",
			"monke_island/Variant_Horror",
			"monke_island/Variant_Horror/UI",
			"monke_island/Variant_Shooter",
			"monke_island/Variant_Shooter/AI",
			"monke_island/Variant_Shooter/UI",
			"monke_island/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
