using UnrealBuildTool;

public class XiaomaLogicTarget : TargetRules
{
    public XiaomaLogicTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("XiaomaLogic");
    }
}
