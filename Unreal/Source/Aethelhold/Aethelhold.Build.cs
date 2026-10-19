using UnrealBuildTool;

public class Aethelhold : ModuleRules
{
	public Aethelhold(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DynamicWind" });
	}
}
