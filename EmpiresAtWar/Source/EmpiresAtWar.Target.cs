using UnrealBuildTool;

public class EmpiresAtWarTarget : TargetRules
{
	public EmpiresAtWarTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("EmpiresAtWar");

		// Compilador mais testado com o UE 4.27.
		WindowsPlatform.CompilerVersion = "14.29.30133";
	}
}
