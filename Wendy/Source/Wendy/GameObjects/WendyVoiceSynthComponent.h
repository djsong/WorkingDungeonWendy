// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "WendyVoiceChat.h"
#include "WendyVoiceSynthComponent.generated.h"

/**
 * Plays back voice audio for the local listener.
 *
 * Voice is deliberately NON-positional (flat), so a single component mixes every speaker rather than one
 * being attached to each speaking character. Decoding still happens per speaker on the voice thread - each
 * needs its own Opus decoder state - and this component only sums the resulting PCM.
 *
 * The audio render thread must never touch a UObject or the voice-chat object, so the shared mixer state
 * (a fixed array of per-speaker ring buffers) is grabbed once at BeginPlay and read directly afterwards.
 */
UCLASS()
class UWendyVoiceSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UWendyVoiceSynthComponent(const FObjectInitializer& ObjectInitializer);

	// Begin USynthComponent interface
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
	// End USynthComponent interface

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Shown on screen when wd.Voice.ShowDebug is on. Game thread only. */
	void DrawVoiceDebugReadout();

	/** Set once at BeginPlay and never reassigned, so the audio thread always sees a stable value. */
	FWendyVoiceMixerStatePtr MixerState;

	/** Reused every callback so the audio thread doesn't allocate. */
	TArray<int16> PopScratch;

	/** Only the local listener plays anything. */
	bool bIsLocalListener = false;
};
