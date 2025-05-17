// Copyright Working Dungeon Wendy, by DJ Song

using UnrealBuildTool;
using System.Collections.Generic;

public class WendyTarget : TargetRules
{
	public WendyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.Add("Wendy");
	}
}
