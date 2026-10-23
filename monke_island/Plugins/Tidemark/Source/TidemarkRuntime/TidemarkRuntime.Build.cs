// Tidemark plugin

using UnrealBuildTool;

public class TidemarkRuntime : ModuleRules
{
	public TidemarkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Layer A (Grid), B (Solver, Net, Rules) and C (Query, Render): engine modules only, never the game module.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"NetCore",
			"RenderCore",
			"RHI",
			"Niagara"
		});
	}
}
