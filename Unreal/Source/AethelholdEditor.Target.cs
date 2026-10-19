using UnrealBuildTool;

public class AethelholdEditorTarget : TargetRules
{
	public AethelholdEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new[] { "Aethelhold", "AethelholdEditor" });
	}
}
