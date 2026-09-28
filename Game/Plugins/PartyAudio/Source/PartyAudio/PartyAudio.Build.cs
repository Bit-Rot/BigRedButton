using UnrealBuildTool;

public class PartyAudio : ModuleRules
{
    public PartyAudio(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bWarningsAsErrors = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PhysicsCore", // EPhysicalSurface / UPhysicalMaterial for surface-aware impacts
        });
    }
}
