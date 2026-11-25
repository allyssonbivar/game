using UnrealBuildTool;

public class EmpiresAtWar : ModuleRules
{
	public EmpiresAtWar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Permite #include "Combat/..." a partir da raiz do modulo.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"AIModule", "PhysicsCore"
		});

		// Ferramentas chamadas pelos scripts do editor (refazer PhysicsAsset etc.).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}
	}
}
