using UnrealBuildTool;

public class ValStrikeClientTarget : TargetRules
{
    public ValStrikeClientTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("ValStrike");
    }
}
