using UnrealBuildTool;

public class ValStrikeTarget : TargetRules
{
    public ValStrikeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("ValStrike");
    }
}
