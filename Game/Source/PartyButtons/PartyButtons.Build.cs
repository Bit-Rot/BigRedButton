using UnrealBuildTool;

public class PartyButtons : ModuleRules
{
    public PartyButtons(ReadOnlyTargetRules Target) : base(Target)
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
            "EnhancedInput",
            "PartyInput",
            "PhysicsCore", // UPhysicalMaterial — see AOctoPawn::ApplySurfaceMaterial
        });
    }
}
