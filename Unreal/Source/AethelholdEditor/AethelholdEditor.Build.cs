using UnrealBuildTool;

public class AethelholdEditor : ModuleRules
{
	public AethelholdEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Aethelhold" });
		PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "Landscape", "LandscapeEditor", "AssetRegistry" });
	}
}
