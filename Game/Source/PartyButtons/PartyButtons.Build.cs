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
            "PartyAudio",  // Sound events, impact/loop emitters — see AI/design/audio.md
            "PhysicsCore", // UPhysicalMaterial — see AOctoPawn::ApplySurfaceMaterial
            "Chaos",       // Solver-side body access in AOctoPawn::AsyncPhysicsTickActor
            "Water",       // AOctoBackgroundBoat samples the ocean surface
        });
    }
}
