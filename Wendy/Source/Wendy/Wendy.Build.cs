// Copyright Working Dungeon Wendy, by DJ Song

using UnrealBuildTool;

public class Wendy : ModuleRules
{
	public Wendy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
            "ApplicationCore",
            "AudioMixer",        // USynthComponent, for voice playback
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "InputCore",
            "NetCore",
            "RenderCore",
            "SignalProcessing",  // Audio::TCircularAudioBuffer (SPSC), for voice playback hand-off
            "Slate",
            "SlateCore",
            "UMG",
			"Sockets",
            "Voice"              // IVoiceCapture + Opus IVoiceEncoder/IVoiceDecoder
		});

        PublicIncludePaths.AddRange(new string[] {
            "Wendy",
            "Wendy/GameFrame",
            "Wendy/GameObjects",
            "Wendy/UI"
        });
    }
}
