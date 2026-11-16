using UnrealBuildTool;

public class XiaomaLogicEditorTarget : TargetRules
{
    public XiaomaLogicEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("XiaomaLogic");
    }
}
