
using UnrealBuildTool;

public class AnimPhys : ModuleRules
{
	public AnimPhys(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "AnimGraphRuntime" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject",	"Engine", "Slate", "SlateCore" });
	}
}
