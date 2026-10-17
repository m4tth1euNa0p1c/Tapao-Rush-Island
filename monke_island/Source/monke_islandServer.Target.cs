// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// Build a dedicated headless game server from an Unreal Engine 5.8 source installation.
// This target is not part of the client Shipping build.
public class monke_islandServerTarget : TargetRules
{
	public monke_islandServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("monke_island");
	}
}
