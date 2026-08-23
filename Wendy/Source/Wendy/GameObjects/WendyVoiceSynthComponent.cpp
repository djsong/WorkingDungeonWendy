// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyVoiceSynthComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "WendyCommon.h"
#include "WendyGameInstance.h"

static TAutoConsoleVariable<float> CVarWdVoicePlaybackGain(
	TEXT("wd.Voice.PlaybackGain"),
	1.0f,
	TEXT("Linear gain applied to decoded voice audio during playback."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdVoiceMuteOutput(
	TEXT("wd.Voice.MuteOutput"),
	0,
	TEXT("1 silences incoming voice without affecting your own transmission (that is wd.Voice.MuteInput)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdVoiceShowDebug(
	TEXT("wd.Voice.ShowDebug"),
	0,
	TEXT("Show the on-screen voice readout: capture state, mic level, frames/sec, encoded bytes/sec and playback queue depth.")
	TEXT(" This is the 'visible' checkpoint - it tells you the mic and config are working before anything is audible."),
	ECVF_Default);

UWendyVoiceSynthComponent::UWendyVoiceSynthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

	// Voice is flat by design - everyone is heard equally regardless of where they are in the dungeon.
	bAllowSpatialization = false;
	// Explicitly NOT auto-activating: every character carries this component, but only the local listener's
	// copy should ever generate audio, so BeginPlay starts it selectively.
	bAutoActivate = false;

	NumChannels = WENDY_VOICE_NUM_CHANNELS;
}

bool UWendyVoiceSynthComponent::Init(int32& SampleRate)
{
	// Run the synth at the voice rate so no resampling is needed between decode and playback.
	SampleRate = WENDY_VOICE_SAMPLE_RATE;
	NumChannels = WENDY_VOICE_NUM_CHANNELS;

	// Sized up front so the audio render thread never has to allocate inside its callback.
	PopScratch.SetNumUninitialized(4096);

	return true;
}

void UWendyVoiceSynthComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only the local player listens; every other character's copy of this component stays silent.
	const APawn* OwnerAsPawn = Cast<APawn>(GetOwner());
	bIsLocalListener = (OwnerAsPawn != nullptr && OwnerAsPawn->IsLocallyControlled());
	if (false == bIsLocalListener)
	{
		return;
	}

	UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (false == IsValid(WdGameInst))
	{
		return;
	}

	// Grab the mixer state BEFORE Start(), so the audio thread never observes a half-set-up component.
	MixerState = WdGameInst->GetVoiceMixerState();
	if (false == MixerState.IsValid())
	{
		UE_LOG(LogWendy, Warning, TEXT("Voice mixer state unavailable; voice chat may have failed to initialize."));
		return;
	}

	Start();
}

void UWendyVoiceSynthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsActive())
	{
		Stop();
	}

	Super::EndPlay(EndPlayReason);
}

int32 UWendyVoiceSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	// Audio render thread. Touch nothing but the shared ring buffer and our own scratch.
	if (NumSamples <= 0)
	{
		return 0;
	}

	int32 PoppedSamples = 0;
	if (MixerState.IsValid())
	{
		// Normally pre-sized in Init(); this only ever fires if the mixer asks for an unusually large block.
		if (PopScratch.Num() < NumSamples)
		{
			PopScratch.SetNumUninitialized(NumSamples);
		}

		// Sums every active speaker; returns 0 when nobody is talking.
		// Still drained while muted, so unmuting resumes live rather than replaying a backlog.
		PoppedSamples = MixerState->MixInto(PopScratch.GetData(), NumSamples);

		const bool bMuteOutput = CVarWdVoiceMuteOutput.GetValueOnAnyThread() > 0;
		const float PlaybackGain = bMuteOutput ? 0.0f : CVarWdVoicePlaybackGain.GetValueOnAnyThread();
		// int16 -> normalized float, which is what the mixer expects.
		const float ToFloatScale = PlaybackGain / static_cast<float>(-static_cast<int32>(MIN_int16));
		for (int32 SampleIdx = 0; SampleIdx < PoppedSamples; ++SampleIdx)
		{
			OutAudio[SampleIdx] = static_cast<float>(PopScratch[SampleIdx]) * ToFloatScale;
		}
	}

	// Underrun (nobody talking, or the buffer ran dry) is normal - fill the rest with silence.
	for (int32 SampleIdx = PoppedSamples; SampleIdx < NumSamples; ++SampleIdx)
	{
		OutAudio[SampleIdx] = 0.0f;
	}

	return NumSamples;
}

void UWendyVoiceSynthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsLocalListener && CVarWdVoiceShowDebug.GetValueOnGameThread() > 0)
	{
		DrawVoiceDebugReadout();
	}
}

void UWendyVoiceSynthComponent::DrawVoiceDebugReadout()
{
	UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (false == IsValid(WdGameInst) || GEngine == nullptr)
	{
		return;
	}

	FWendyVoiceChat* VoiceChat = WdGameInst->GetVoiceChat();
	if (VoiceChat == nullptr)
	{
		return;
	}

	// Matches EVoiceCaptureState::Type ordering.
	static const TCHAR* CaptureStateNames[] = {
		TEXT("UnInitialized"), TEXT("NotCapturing"), TEXT("Ok"), TEXT("NoData"),
		TEXT("Stopping"), TEXT("BufferTooSmall"), TEXT("Error")
	};

	const int32 CaptureState = VoiceChat->GetDebugCaptureState();
	const TCHAR* CaptureStateName = (CaptureState >= 0 && CaptureState < UE_ARRAY_COUNT(CaptureStateNames))
		? CaptureStateNames[CaptureState] : TEXT("?");

	const int32 MicPeak = VoiceChat->GetDebugMicPeak();
	// A crude level meter is far easier to read at a glance than a number while you talk into the mic.
	const int32 MeterBars = FMath::Clamp(MicPeak / 5, 0, 20);
	FString MicMeter = FString::ChrN(MeterBars, TEXT('|'));
	MicMeter.Append(FString::ChrN(20 - MeterBars, TEXT('.')));

	// "why is nothing going out" is the question this line has to answer at a glance.
	const TCHAR* TransmitStateName = (VoiceChat->GetDebugTransmitting() != 0) ? TEXT("LIVE") : TEXT("SILENT");

	// wd.Voice.PushToTalk is file-static over in WendyVoiceChat.cpp, so look it up by name - the same way
	// the engine's own voice capture reads its threshold CVars.
	static IConsoleVariable* PushToTalkCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("wd.Voice.PushToTalk"));
	const bool bPushToTalkMode = (PushToTalkCVar != nullptr && PushToTalkCVar->GetInt() > 0);
	const TCHAR* MicModeName = bPushToTalkMode ? TEXT("push-to-talk") : TEXT("open mic");

	const FString DebugMsg = FString::Printf(
		TEXT("Wendy Voice | %s (%s)%s  capture:%s  mic:[%s]%3d%%  %d frames/s  %d B/s\n")
		TEXT("            | mic device (system default): %s\n")
		TEXT("            | net  tx:%d/s  rx:%d/s  endpoints:%d  queued:%d (%.0f ms)  underruns:%d/s\n")
		TEXT("            | hearing: %s"),
		TransmitStateName,
		MicModeName,
		(CVarWdVoiceMuteOutput.GetValueOnGameThread() > 0) ? TEXT("  [OUTPUT MUTED]") : TEXT(""),
		CaptureStateName,
		*MicMeter,
		MicPeak,
		VoiceChat->GetDebugFramesPerSec(),
		VoiceChat->GetDebugEncodedBytesPerSec(),
		*VoiceChat->GetCaptureDeviceName(),
		VoiceChat->GetDebugPacketsSentPerSec(),
		VoiceChat->GetDebugPacketsRecvPerSec(),
		VoiceChat->GetDebugKnownEndpoints(),
		VoiceChat->GetDebugPlaybackQueuedSamples(),
		(1000.0f * VoiceChat->GetDebugPlaybackQueuedSamples()) / static_cast<float>(WENDY_VOICE_SAMPLE_RATE),
		VoiceChat->GetDebugUnderrunsPerSec(),
		*VoiceChat->GetDebugSpeakerNames());

	const uint64 MessageKey = static_cast<uint64>(reinterpret_cast<UPTRINT>(this));
	GEngine->AddOnScreenDebugMessage(MessageKey, 2.0f, FColor::Green, DebugMsg);
}
