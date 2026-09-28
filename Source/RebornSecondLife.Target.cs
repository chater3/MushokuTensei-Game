using UnrealBuildTool;
using System.Collections.Generic;

public class RebornSecondLifeTarget : TargetRules
{
	public RebornSecondLifeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RebornSecondLife");
	}
}
