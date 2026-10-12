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
		PrivateDependencyModuleNames.AddRange(new string[] { "MoviePlayer", "PhysicsCore" });

		// blood of the tiki creatures (Creatures/TikiBlood, T-153): Niagara effects spawned from C++
		PrivateDependencyModuleNames.Add("Niagara");

		// key of a player across the islands (Run/RunSubsystem: FUniqueNetIdRepl::ToString, ADR-007)
		PrivateDependencyModuleNames.Add("CoreOnline");

		// proximity voice chat (Voice/, T-263): the engine's VoIP through the online subsystem (NULL: a local voice
		// session per world, the voice interface, the talking state); plugins listed in monke_island.uproject
		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });

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
