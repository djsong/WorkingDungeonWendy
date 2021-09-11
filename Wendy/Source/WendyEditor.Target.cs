// Copyright Working Dungeon Wendy, by DJ Song

using UnrealBuildTool;
using System.Collections.Generic;

public class WendyEditorTarget : TargetRules
{
	public WendyEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("Wendy");
	}
}
