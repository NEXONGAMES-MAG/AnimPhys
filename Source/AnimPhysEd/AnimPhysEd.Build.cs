
using UnrealBuildTool;

public class AnimPhysEd : ModuleRules
{
	public AnimPhysEd(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AnimPhys", "AnimPhysGraphNode" });
        PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraph", "BlueprintGraph", "Persona", "UnrealEd", "AnimGraphRuntime", "SlateCore", "EditorFramework" });
		PrivateDependencyModuleNames.AddRange(new string[] { "AnimationEditMode" });
	}
}
