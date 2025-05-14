
namespace UnrealBuildTool.Rules
{
	public class AnimPhysGraphNode : ModuleRules
	{
		public AnimPhysGraphNode(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(new string[]{	"AnimationCore", "AnimGraphRuntime", "Core", "CoreUObject", "Engine", "InputCore", "KismetCompiler", "AnimPhys" });

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(new string[]{	"UnrealEd",	"Kismet", "AnimGraph", "BlueprintGraph", "Slate", "SlateCore", "ToolMenus" });
			}
		}
	}
}
