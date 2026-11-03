using UnrealBuildTool;

public class ValStrikeServerTarget : TargetRules
{
    public ValStrikeServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("ValStrike");
    }
}
