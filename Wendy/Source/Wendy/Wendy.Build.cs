// Copyright Working Dungeon Wendy, by DJ Song

using UnrealBuildTool;

public class Wendy : ModuleRules
{
	public Wendy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
            "ApplicationCore",
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "InputCore",
            "NetCore",            
            "RenderCore",
            "Slate",
            "SlateCore",
            "UMG",
			"Sockets"
		});

        PublicIncludePaths.AddRange(new string[] {
            "Wendy",
            "Wendy/GameFrame",
            "Wendy/GameObjects",
            "Wendy/UI"
        });
    }
}
