using UnrealBuildTool;

public class EmpiresAtWarEditorTarget : TargetRules
{
	public EmpiresAtWarEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("EmpiresAtWar");

		// Compilador mais testado com o UE 4.27.
		WindowsPlatform.CompilerVersion = "14.29.30133";
	}
}
