using UnrealBuildTool;
using System.Collections.Generic;

public class PartyButtonsTarget : TargetRules
{
    public PartyButtonsTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.AddRange(new string[] { "PartyButtons" });
    }
}
