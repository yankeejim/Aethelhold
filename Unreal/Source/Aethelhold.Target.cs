using UnrealBuildTool;

public class AethelholdTarget : TargetRules
{
	public AethelholdTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Aethelhold");
	}
}
