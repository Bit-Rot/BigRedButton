using UnrealBuildTool;

public class AtrophyToolsEditor : ModuleRules
{
    public AtrophyToolsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bWarningsAsErrors = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "InputCore",
            "UnrealEd",
            "EditorFramework",
            "AtrophyTools",
        });
    }
}
