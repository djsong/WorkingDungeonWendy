// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyVoiceChat.h"
#include "HAL/PlatformProcess.h" // For FPlatformProcess::Sleep()
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"
#include "AudioCaptureCore.h"
#include "AudioDeviceManager.h"
#include "AudioMixerDevice.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "VoiceModule.h"
#include "WendyCommon.h"
#include "WendyVoicePackets.h"

DEFINE_LOG_CATEGORY_STATIC(LogWendyVoiceChat, Log, All);

static TAutoConsoleVariable<int32> CVarWdVoiceEnabled(
	TEXT("wd.Voice.Enabled"),
	1,
	TEXT("Master switch for voice chat. 0 stops capturing and playing without tearing anything down."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdVoiceLoopback(
	TEXT("wd.Voice.Loopback"),
	0,
	TEXT("Route encoded audio straight back into playback so you hear your own voice through the full Opus round-trip.")
	TEXT(" This was how the audio path was proven before any networking existed (Phase 1); leave it off for")
	TEXT(" normal use and switch it on to isolate an audio problem from a network one.")
	TEXT(" USE HEADPHONES - loopback through speakers will feed back."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdVoiceMicGain(
	TEXT("wd.Voice.MicGain"),
	5.0f,
	TEXT("Linear gain applied to captured microphone audio before encoding."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdVoicePushToTalk(
	TEXT("wd.Voice.PushToTalk"),
	0,
	TEXT("0 = open mic: transmit whenever the microphone picks up sound above voice.SilenceDetectionThreshold.")
	TEXT(" 1 = push-to-talk: transmit only while wd.Voice.PushToTalkKey is held.")
	TEXT(" Open mic is the default because it is what conversation actually feels like; push-to-talk is there")
	TEXT(" for when background noise or privacy matters more."),
	ECVF_Default);

// Note: wd.Voice.PushToTalkKey lives in WendyDungeonPlayerController.cpp, where the key is matched.

static TAutoConsoleVariable<int32> CVarWdVoiceMuteInput(
	TEXT("wd.Voice.MuteInput"),
	0,
	TEXT("1 mutes your microphone outright - nothing is transmitted regardless of push-to-talk or open mic."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdVoiceMuteOutput(
	TEXT("wd.Voice.MuteOutput"),
	0,
	TEXT("1 silences incoming voice without affecting your own transmission (that is wd.Voice.MuteInput)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdVoiceJitterBufferMs(
	TEXT("wd.Voice.JitterBufferMs"),
	60,
	TEXT("Milliseconds of audio to bank per speaker before playing them, absorbing uneven packet arrival.")
	TEXT(" Higher = fewer dropouts but more delay; lower = snappier but more prone to gaps.")
	TEXT(" Tune against the underruns/s figure in wd.Voice.ShowDebug: if it is above zero while someone")
	TEXT(" talks steadily, raise this. 0 disables buffering entirely (lowest latency, most fragile)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdVoiceKeepAliveInterval(
	TEXT("wd.Voice.KeepAliveInterval"),
	1.0f,
	TEXT("Seconds between empty keepalive datagrams while nobody is talking. These exist so the server keeps")
	TEXT(" learning each client's address, and so NAT/firewall mappings don't lapse during silence."),
	ECVF_Default);

//////////////////////////

FWendyVoiceMixerState::FWendyVoiceMixerState()
{
	for (int32 SlotIdx = 0; SlotIdx < MaxSpeakers; ++SlotIdx)
	{
		Slots[SlotIdx].Buffer.SetCapacity(WENDY_VOICE_PLAYBACK_BUFFER_SAMPLES);
	}

	// Sized once so the audio render thread never allocates.
	MixScratch.SetNumUninitialized(4096);
}

int32 FWendyVoiceMixerState::MixInto(int16* OutSamples, int32 NumSamples)
{
	if (OutSamples == nullptr || NumSamples <= 0)
	{
		return 0;
	}

	if (MixScratch.Num() < NumSamples)
	{
		MixScratch.SetNumUninitialized(NumSamples);
	}

	// Jitter buffer: how much audio to bank before a speaker starts playing. Packets arrive unevenly, so
	// playing the instant one lands means the very next late packet becomes an audible gap. Holding a
	// cushion trades that latency for the ability to ride through a hiccup.
	const int32 JitterBufferMs = FMath::Max(CVarWdVoiceJitterBufferMs.GetValueOnAnyThread(), 0);
	const int32 PrimeTargetSamples = (WENDY_VOICE_SAMPLE_RATE * JitterBufferMs) / 1000;

	int32 MixedSamples = 0;

	for (int32 SlotIdx = 0; SlotIdx < MaxSpeakers; ++SlotIdx)
	{
		FWendyVoiceSpeakerSlot& Slot = Slots[SlotIdx];
		if (Slot.bActive.GetValue() == 0)
		{
			continue;
		}

		// Still filling: stay silent rather than emit a fragment we'd immediately underrun after.
		if (false == Slot.bPrimed)
		{
			if (static_cast<int32>(Slot.Buffer.Num()) < PrimeTargetSamples)
			{
				continue;
			}
			Slot.bPrimed = true;
		}

		const int32 PoppedSamples = Slot.Buffer.Pop(MixScratch.GetData(), static_cast<uint32>(NumSamples));

		// Couldn't fill the request, so this speaker has run dry. Use whatever did arrive, then go back to
		// filling - otherwise we'd stutter on every callback instead of pausing once and recovering.
		if (PoppedSamples < NumSamples)
		{
			Slot.bPrimed = false;
			UnderrunCount.Increment();
		}

		if (PoppedSamples <= 0)
		{
			continue;
		}

		if (MixedSamples == 0)
		{
			// First contributor writes rather than accumulates, so we never have to pre-clear the output.
			FMemory::Memcpy(OutSamples, MixScratch.GetData(), PoppedSamples * sizeof(int16));
		}
		else
		{
			const int32 OverlapSamples = FMath::Min(MixedSamples, PoppedSamples);
			for (int32 SampleIdx = 0; SampleIdx < OverlapSamples; ++SampleIdx)
			{
				// Sum in wider precision and clamp, so several people talking at once can't wrap around.
				const int32 Summed = static_cast<int32>(OutSamples[SampleIdx]) + static_cast<int32>(MixScratch[SampleIdx]);
				OutSamples[SampleIdx] = static_cast<int16>(FMath::Clamp(Summed,
					static_cast<int32>(MIN_int16), static_cast<int32>(MAX_int16)));
			}
			// This speaker supplied more than everyone before them; the tail is theirs alone.
			for (int32 SampleIdx = OverlapSamples; SampleIdx < PoppedSamples; ++SampleIdx)
			{
				OutSamples[SampleIdx] = MixScratch[SampleIdx];
			}
		}

		MixedSamples = FMath::Max(MixedSamples, PoppedSamples);
	}

	return MixedSamples;
}

//////////////////////////

FWendyVoiceChat::FWendyVoiceChat()
{
}

FWendyVoiceChat::~FWendyVoiceChat()
{
	ShutdownVoice();
}

bool FWendyVoiceChat::InitSocket()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		return false;
	}

	// UDP, deliberately: voice wants low latency and tolerates loss, and must not queue behind the image
	// stream's TCP traffic (which saturates the link by design).
	VoiceSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("WendyVoiceChat Socket"), FNetworkProtocolTypes::IPv4);
	if (VoiceSocket == nullptr)
	{
		UE_LOG(LogWendyVoiceChat, Warning, TEXT("Failed to create voice UDP socket."));
		return false;
	}

	VoiceSocket->SetNonBlocking(true);
	VoiceSocket->SetReuseAddr();

	RecvFromAddr = SocketSubsystem->CreateInternetAddr();

	if (bIsServer)
	{
		TSharedRef<FInternetAddr> BindAddr = SocketSubsystem->CreateInternetAddr();
		BindAddr->SetAnyAddress();
		BindAddr->SetPort(WENDY_VOICE_PORT_NUMBER);

		if (false == VoiceSocket->Bind(*BindAddr))
		{
			UE_LOG(LogWendyVoiceChat, Warning, TEXT("Voice socket failed to bind UDP port %d."), WENDY_VOICE_PORT_NUMBER);
			return false;
		}

		UE_LOG(LogWendyVoiceChat, Log, TEXT("Voice server listening on UDP %d."), WENDY_VOICE_PORT_NUMBER);
	}
	else
	{
		// Any local port will do; the server learns whatever we end up with from our first datagram.
		TSharedRef<FInternetAddr> BindAddr = SocketSubsystem->CreateInternetAddr();
		BindAddr->SetAnyAddress();
		BindAddr->SetPort(0);
		if (false == VoiceSocket->Bind(*BindAddr))
		{
			UE_LOG(LogWendyVoiceChat, Warning, TEXT("Voice socket failed to bind a local UDP port."));
			return false;
		}

		ServerAddr = SocketSubsystem->CreateInternetAddr();
		bool bIsIpValid = false;
		ServerAddr->SetIp(*ConnectIpAddrIfClient, bIsIpValid);
		ServerAddr->SetPort(WENDY_VOICE_PORT_NUMBER);

		if (false == bIsIpValid)
		{
			UE_LOG(LogWendyVoiceChat, Warning, TEXT("Voice server IP '%s' is not valid."), *ConnectIpAddrIfClient);
			return false;
		}

		UE_LOG(LogWendyVoiceChat, Log, TEXT("Voice client targeting %s:%d."), *ConnectIpAddrIfClient, WENDY_VOICE_PORT_NUMBER);
	}

	return true;
}

void FWendyVoiceChat::QueryMicrophoneDeviceName()
{
	// CAVEAT: IVoiceCapture cannot tell us which device it opened - it only ACCEPTS a name ("" = default),
	// with no getter and no enumeration. So we ask AudioCaptureCore for the system default instead, which is
	// what "" resolves to in practice. Inferred rather than reported.
	MicrophoneDeviceName = TEXT("(unknown)");

	Audio::FAudioCapture DeviceQuery;
	Audio::FCaptureDeviceInfo DefaultDeviceInfo;
	if (DeviceQuery.GetCaptureDeviceInfo(DefaultDeviceInfo, Audio::DefaultDeviceIndex))
	{
		if (false == DefaultDeviceInfo.DeviceName.IsEmpty())
		{
			MicrophoneDeviceName = DefaultDeviceInfo.DeviceName;
		}
	}

	UE_LOG(LogWendyVoiceChat, Log, TEXT("Voice microphone device: %s"), *MicrophoneDeviceName);
}

FString FWendyVoiceChat::GetPlaybackDeviceName(const UObject* InWorldContextObject)
{
	// Asked of the engine each time rather than cached, so swapping headphones mid-session shows up.
	// Uses the engine's own accessor rather than casting an FAudioDevice ourselves - it performs the
	// mixer-type check internally and returns null when the device isn't a mixer device.
	if (InWorldContextObject != nullptr)
	{
		if (Audio::FMixerDevice* MixerDevice = FAudioDeviceManager::GetAudioMixerDeviceFromWorldContext(InWorldContextObject))
		{
			const FString& PlatformDeviceName = MixerDevice->GetPlatformDeviceInfo().Name;
			if (false == PlatformDeviceName.IsEmpty())
			{
				return PlatformDeviceName;
			}
		}
	}

	return TEXT("(unknown)");
}

bool FWendyVoiceChat::IsPushToTalkEnabled()
{
	return (CVarWdVoicePushToTalk.GetValueOnAnyThread() > 0);
}

bool FWendyVoiceChat::IsMicrophoneActive() const
{
	return bVoiceInitialized && ShouldTransmitVoice();
}

bool FWendyVoiceChat::IsSpeakerActive() const
{
	// wd.Voice.Enabled is checked here as well as in ShouldTransmitVoice: the master switch makes UpdateTick
	// return before ReceiveVoiceDatagrams, so nothing is decoded and nothing can be heard. Without this the
	// HUD would show a live speaker while incoming voice was in fact going nowhere.
	return bVoiceInitialized
		&& (CVarWdVoiceEnabled.GetValueOnAnyThread() > 0)
		&& (CVarWdVoiceMuteOutput.GetValueOnAnyThread() <= 0);
}

bool FWendyVoiceChat::ShouldTransmitVoice() const
{
	if (CVarWdVoiceEnabled.GetValueOnAnyThread() <= 0 || CVarWdVoiceMuteInput.GetValueOnAnyThread() > 0)
	{
		return false;
	}

	// Open mic transmits whenever the capture hands us audio; the engine's silence detection has already
	// decided that is worth sending. Push-to-talk adds the extra requirement of holding the key.
	if (IsPushToTalkEnabled())
	{
		return PushToTalkActive.GetValue() != 0;
	}

	return true;
}

FString FWendyVoiceChat::GetActiveSpeakerNames() const
{
	FScopeLock SpeakerNamesLock(&ActiveSpeakerNamesMutex);
	return ActiveSpeakerNames;
}

void FWendyVoiceChat::InitVoice(const FWendyWorldConnectingInfo& InConnectingInfo)
{
	if (bVoiceInitialized)
	{
		return;
	}

	bIsServer = InConnectingInfo.bMyselfServer;
	ConnectIpAddrIfClient = InConnectingInfo.ServerIp;
	SelfIdentification = InConnectingInfo.UserId;

	if (false == FVoiceModule::IsAvailable() || false == FVoiceModule::Get().IsVoiceEnabled())
	{
		// Almost always means the [Voice] bEnabled=true key is missing from DefaultEngine.ini.
		UE_LOG(LogWendyVoiceChat, Warning, TEXT("Voice module unavailable or disabled. Is [Voice] bEnabled=true set in DefaultEngine.ini?"));
		return;
	}

	QueryMicrophoneDeviceName();

	// Empty device name = system default input device.
	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture(FString(), WENDY_VOICE_SAMPLE_RATE, WENDY_VOICE_NUM_CHANNELS);
	if (false == VoiceCapture.IsValid())
	{
		UE_LOG(LogWendyVoiceChat, Warning, TEXT("Failed to create voice capture. Is a microphone connected and permitted?"));
		return;
	}

	// Encode hint left at the engine default (speech-oriented), which is what we want here anyway.
	VoiceEncoder = FVoiceModule::Get().CreateVoiceEncoder(WENDY_VOICE_SAMPLE_RATE, WENDY_VOICE_NUM_CHANNELS);
	if (false == VoiceEncoder.IsValid())
	{
		UE_LOG(LogWendyVoiceChat, Warning, TEXT("Failed to create Opus encoder."));
		VoiceCapture.Reset();
		return;
	}

	// A compressed 20 ms frame is only tens of bytes, but the encoder prepends a header, and the DECODER
	// insists on several whole frames of free space before it will decode anything at all - hence the
	// deliberately oversized decode scratch (see WENDY_VOICE_DECODE_BUFFER_SAMPLES).
	RawCaptureScratch.SetNumUninitialized(WENDY_VOICE_FRAME_BYTES * 16);
	EncodeScratch.SetNumUninitialized(WENDY_VOICE_FRAME_BYTES * 4);
	DecodeScratch.SetNumUninitialized(WENDY_VOICE_DECODE_BUFFER_SAMPLES);
	DatagramScratch.SetNumUninitialized(FWendyVoicePacket::MaxDatagramBytes);
	CaptureAccumulator.Reset();

	MixerState = MakeShared<FWendyVoiceMixerState, ESPMode::ThreadSafe>();

	if (false == InitSocket())
	{
		ShutdownVoice();
		return;
	}

	if (false == VoiceCapture->Start())
	{
		UE_LOG(LogWendyVoiceChat, Warning, TEXT("Voice capture failed to start."));
		ShutdownVoice();
		return;
	}

	LastDebugPublishTime = FPlatformTime::Seconds();
	LastDatagramSendTime = LastDebugPublishTime;
	bVoiceInitialized = true;

	UE_LOG(LogWendyVoiceChat, Log, TEXT("Wendy voice chat initialized (%d Hz, %d ch, %d samples/frame, %s)."),
		WENDY_VOICE_SAMPLE_RATE, WENDY_VOICE_NUM_CHANNELS, WENDY_VOICE_FRAME_SAMPLES,
		bIsServer ? TEXT("server") : TEXT("client"));
}

void FWendyVoiceChat::ShutdownVoice()
{
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Stop();
		VoiceCapture->Shutdown();
		VoiceCapture.Reset();
	}
	if (VoiceEncoder.IsValid())
	{
		VoiceEncoder->Destroy();
		VoiceEncoder.Reset();
	}

	if (MixerState.IsValid())
	{
		for (int32 SlotIdx = 0; SlotIdx < FWendyVoiceMixerState::MaxSpeakers; ++SlotIdx)
		{
			FWendyVoiceSpeakerSlot& Slot = MixerState->Slots[SlotIdx];
			Slot.bActive.Set(0);
			if (Slot.Decoder.IsValid())
			{
				Slot.Decoder->Destroy();
				Slot.Decoder.Reset();
			}
		}
	}

	if (VoiceSocket != nullptr)
	{
		VoiceSocket->Close();
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem != nullptr)
		{
			SocketSubsystem->DestroySocket(VoiceSocket);
		}
		VoiceSocket = nullptr;
	}

	ServerAddr.Reset();
	RecvFromAddr.Reset();
	KnownEndpoints.Reset();

	// Deliberately not destroyed: the audio thread may still hold this and be mid-callback. Dropping our
	// reference is enough - the last holder frees it.
	MixerState.Reset();

	bVoiceInitialized = false;
}

void FWendyVoiceChat::UpdateTick()
{
	if (false == bVoiceInitialized)
	{
		return;
	}

	if (CVarWdVoiceEnabled.GetValueOnAnyThread() <= 0)
	{
		// Keep draining the mic so that turning voice back on doesn't send a backlog of stale audio.
		uint32 DiscardAvailable = 0;
		VoiceCapture->GetVoiceData(RawCaptureScratch.GetData(), RawCaptureScratch.Num(), DiscardAvailable);
		CaptureAccumulator.Reset();

		// Same reasoning in the other direction: discard anything that arrives while we're switched off,
		// otherwise re-enabling would play out whatever had piled up in the socket as a burst of stale audio.
		if (VoiceSocket != nullptr && RecvFromAddr.IsValid())
		{
			int32 DiscardLoopLimit = 64;
			int32 DiscardedBytes = 0;
			while (DiscardLoopLimit-- > 0
				&& VoiceSocket->RecvFrom(DatagramScratch.GetData(), DatagramScratch.Num(), DiscardedBytes, *RecvFromAddr)
				&& DiscardedBytes > 0)
			{
			}
		}

		UpdateDebugCountersPerSecond();
		return;
	}

	// Always polled, even when we aren't transmitting, so the on-screen mic meter still responds - that is
	// how you tell "muted" apart from "microphone isn't working".
	PollCapturedAudio();

	const bool bShouldTransmit = ShouldTransmitVoice();
	

	// Encode every whole frame we now have, then drop them all in one go (cheaper than shifting per frame).
	// While gated we still consume the samples rather than letting a backlog build up and burst later.
	int32 ConsumedSamples = 0;
	while ((CaptureAccumulator.Num() - ConsumedSamples) >= WENDY_VOICE_FRAME_SAMPLES)
	{
		if (bShouldTransmit)
		{
			EncodeAndRouteFrame(CaptureAccumulator.GetData() + ConsumedSamples);
		}
		ConsumedSamples += WENDY_VOICE_FRAME_SAMPLES;
	}
	if (ConsumedSamples > 0)
	{
		CaptureAccumulator.RemoveAt(0, ConsumedSamples);
	}

	// Silence produces no frames at all, so without this the server would never learn a quiet client's
	// address (and any NAT mapping would lapse).
	const double CurrTime = FPlatformTime::Seconds();
	if ((CurrTime - LastDatagramSendTime) >= CVarWdVoiceKeepAliveInterval.GetValueOnAnyThread())
	{
		// Carries no audio, so the sequence number is irrelevant - the receiver skips empty datagrams.
		SendVoiceDatagram(NextSequenceNumber, nullptr, 0);
	}

	ReceiveVoiceDatagrams();
	ReleaseTimedOutSpeakers();

	UpdateDebugCountersPerSecond();
}

void FWendyVoiceChat::PollCapturedAudio()
{
	uint32 AvailableBytes = 0;
	const EVoiceCaptureState::Type CaptureState = VoiceCapture->GetCaptureState(AvailableBytes);
	DebugCaptureState.Set(static_cast<int32>(CaptureState));

	// NoData just means the input sat below voice.SilenceDetectionThreshold - the device is fine.
	if (CaptureState != EVoiceCaptureState::Ok || AvailableBytes == 0)
	{
		return;
	}

	uint32 ReadBytes = 0;
	const EVoiceCaptureState::Type ReadState = VoiceCapture->GetVoiceData(
		RawCaptureScratch.GetData(), static_cast<uint32>(RawCaptureScratch.Num()), ReadBytes);

	if (ReadState != EVoiceCaptureState::Ok || ReadBytes == 0)
	{
		return;
	}

	const int32 ReadSamples = static_cast<int32>(ReadBytes / sizeof(int16));
	const int16* ReadSamplePtr = reinterpret_cast<const int16*>(RawCaptureScratch.GetData());
	const float MicGain = CVarWdVoiceMicGain.GetValueOnAnyThread();

	const int32 WriteStart = CaptureAccumulator.Num();
	CaptureAccumulator.AddUninitialized(ReadSamples);

	int32 PeakSample = 0;
	for (int32 SampleIdx = 0; SampleIdx < ReadSamples; ++SampleIdx)
	{
		const int32 GainedSample = FMath::Clamp(
			FMath::RoundToInt(static_cast<float>(ReadSamplePtr[SampleIdx]) * MicGain),
			static_cast<int32>(MIN_int16), static_cast<int32>(MAX_int16));

		CaptureAccumulator[WriteStart + SampleIdx] = static_cast<int16>(GainedSample);
		PeakSample = FMath::Max(PeakSample, FMath::Abs(GainedSample));
	}

	// Published as 0-100 so the on-screen readout is easy to eyeball.
	DebugMicPeak.Set(FMath::Clamp((PeakSample * 100) / MAX_int16, 0, 100));
}

void FWendyVoiceChat::EncodeAndRouteFrame(const int16* InFrameSamples)
{
	uint32 CompressedSize = static_cast<uint32>(EncodeScratch.Num());
	VoiceEncoder->Encode(reinterpret_cast<const uint8*>(InFrameSamples), static_cast<uint32>(WENDY_VOICE_FRAME_BYTES),
		EncodeScratch.GetData(), CompressedSize);

	if (CompressedSize == 0)
	{
		return;
	}

	++FramesThisSecond;
	EncodedBytesThisSecond += static_cast<int32>(CompressedSize);

	const uint16 SequenceNumber = NextSequenceNumber++;

	SendVoiceDatagram(SequenceNumber, EncodeScratch.GetData(), static_cast<int32>(CompressedSize));

	// Debug aid only: hear yourself through the exact same decode path a remote speaker takes.
	if (CVarWdVoiceLoopback.GetValueOnAnyThread() > 0)
	{
		QueueFrameForSpeaker(SelfIdentification, SequenceNumber, EncodeScratch.GetData(), static_cast<int32>(CompressedSize));
	}
}

void FWendyVoiceChat::SendVoiceDatagram(uint16 InSequenceNumber, const uint8* InCompressedData, int32 InCompressedSize)
{
	if (VoiceSocket == nullptr)
	{
		return;
	}

	const int32 DatagramBytes = FWendyVoicePacket::Write(
		DatagramScratch.GetData(), DatagramScratch.Num(),
		SelfIdentification, InSequenceNumber,
		InCompressedData, InCompressedSize);

	if (DatagramBytes <= 0)
	{
		return;
	}

	int32 BytesSent = 0;
	if (bIsServer)
	{
		// Phase 2a: the server talks to every client it knows of. Relaying clients to each OTHER is 2c.
		for (const FWendyVoiceRemoteEndpoint& Endpoint : KnownEndpoints)
		{
			if (Endpoint.Addr.IsValid())
			{
				VoiceSocket->SendTo(DatagramScratch.GetData(), DatagramBytes, BytesSent, *Endpoint.Addr);
				++PacketsSentThisSecond;
			}
		}
	}
	else if (ServerAddr.IsValid())
	{
		VoiceSocket->SendTo(DatagramScratch.GetData(), DatagramBytes, BytesSent, *ServerAddr);
		++PacketsSentThisSecond;
	}

	LastDatagramSendTime = FPlatformTime::Seconds();
}

void FWendyVoiceChat::ReceiveVoiceDatagrams()
{
	if (VoiceSocket == nullptr || false == RecvFromAddr.IsValid())
	{
		return;
	}

	// Bounded so a flood can't monopolise the voice thread.
	int32 RecvLoopLimit = 64;
	while (RecvLoopLimit-- > 0)
	{
		int32 BytesRead = 0;
		if (false == VoiceSocket->RecvFrom(DatagramScratch.GetData(), DatagramScratch.Num(), BytesRead, *RecvFromAddr))
		{
			break;
		}
		if (BytesRead <= 0)
		{
			break;
		}

		FWendyVoicePacket Packet;
		if (false == Packet.Read(DatagramScratch.GetData(), BytesRead))
		{
			continue;
		}

		++PacketsRecvThisSecond;

		// Never play our own voice back from the wire (the server would otherwise echo us to ourselves).
		if (Packet.SpeakerId == SelfIdentification)
		{
			continue;
		}

		if (bIsServer)
		{
			// Address learning: this is how the server discovers where to send its own voice back to.
			const double CurrTime = FPlatformTime::Seconds();
			FWendyVoiceRemoteEndpoint* Existing = KnownEndpoints.FindByPredicate(
				[&Packet](const FWendyVoiceRemoteEndpoint& InEndpoint) { return InEndpoint.SpeakerId == Packet.SpeakerId; });

			if (Existing != nullptr)
			{
				// Refresh in case the client reconnected from a different port.
				Existing->Addr->SetRawIp(RecvFromAddr->GetRawIp());
				Existing->Addr->SetPort(RecvFromAddr->GetPort());
				Existing->LastSeenTime = CurrTime;
			}
			else
			{
				ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
				if (SocketSubsystem != nullptr)
				{
					FWendyVoiceRemoteEndpoint NewEndpoint;
					NewEndpoint.SpeakerId = Packet.SpeakerId;
					NewEndpoint.Addr = SocketSubsystem->CreateInternetAddr();
					NewEndpoint.Addr->SetRawIp(RecvFromAddr->GetRawIp());
					NewEndpoint.Addr->SetPort(RecvFromAddr->GetPort());
					NewEndpoint.LastSeenTime = CurrTime;
					KnownEndpoints.Add(NewEndpoint);

					UE_LOG(LogWendyVoiceChat, Log, TEXT("Voice: learned client '%s' at %s"),
						*NewEndpoint.SpeakerId, *NewEndpoint.Addr->ToString(true));
				}
			}

			// Relay this speaker on to every OTHER client, so clients hear each other rather than only the
			// server. Forwarding the RAW datagram is what makes this so cheap: the original SpeakerId travels
			// with it, so attribution stays correct and no re-encoding is needed. Safe to send straight from
			// DatagramScratch because nothing rewrites it between the RecvFrom above and here.
			// Keepalives are deliberately not relayed - they exist only for the address learning above, and
			// clients never need each other's addresses in a star relay.
			if (Packet.CompressedSize > 0)
			{
				for (const FWendyVoiceRemoteEndpoint& Endpoint : KnownEndpoints)
				{
					if (Endpoint.SpeakerId != Packet.SpeakerId && Endpoint.Addr.IsValid())
					{
						int32 RelayBytesSent = 0;
						VoiceSocket->SendTo(DatagramScratch.GetData(), BytesRead, RelayBytesSent, *Endpoint.Addr);
						++PacketsSentThisSecond;
					}
				}
			}
		}

		// Keepalives carry no audio; they exist purely for the address learning above.
		if (Packet.CompressedSize > 0)
		{
			QueueFrameForSpeaker(Packet.SpeakerId, Packet.SequenceNumber, Packet.CompressedData, Packet.CompressedSize);
		}
	}
}

FWendyVoiceSpeakerSlot* FWendyVoiceChat::FindOrClaimSpeakerSlot(const FString& InSpeakerId)
{
	if (false == MixerState.IsValid())
	{
		return nullptr;
	}

	for (int32 SlotIdx = 0; SlotIdx < FWendyVoiceMixerState::MaxSpeakers; ++SlotIdx)
	{
		FWendyVoiceSpeakerSlot& Slot = MixerState->Slots[SlotIdx];
		if (Slot.bActive.GetValue() != 0 && Slot.SpeakerId == InSpeakerId)
		{
			return &Slot;
		}
	}

	for (int32 SlotIdx = 0; SlotIdx < FWendyVoiceMixerState::MaxSpeakers; ++SlotIdx)
	{
		FWendyVoiceSpeakerSlot& Slot = MixerState->Slots[SlotIdx];
		if (Slot.bActive.GetValue() == 0)
		{
			Slot.SpeakerId = InSpeakerId;
			Slot.LastSequenceNumber = 0;
			Slot.bHasReceivedAny = false;
			// Start out filling rather than playing, so a new speaker doesn't begin with an instant underrun.
			Slot.bPrimed = false;
			// Safe to touch the buffer only because bActive is still 0 here, so the audio thread is
			// skipping this slot entirely. Doubles as discarding any audio left by a previous occupant.
			Slot.Buffer.SetCapacity(WENDY_VOICE_PLAYBACK_BUFFER_SAMPLES);

			if (false == Slot.Decoder.IsValid())
			{
				Slot.Decoder = FVoiceModule::Get().CreateVoiceDecoder(WENDY_VOICE_SAMPLE_RATE, WENDY_VOICE_NUM_CHANNELS);
			}
			else
			{
				Slot.Decoder->Reset();
			}

			if (false == Slot.Decoder.IsValid())
			{
				return nullptr;
			}

			// Published last: the audio thread must not see this slot until it is fully set up.
			Slot.bActive.Set(1);

			UE_LOG(LogWendyVoiceChat, Log, TEXT("Voice: speaker '%s' took slot %d"), *InSpeakerId, SlotIdx);
			return &Slot;
		}
	}

	return nullptr;
}

void FWendyVoiceChat::QueueFrameForSpeaker(const FString& InSpeakerId, uint16 InSequenceNumber,
	const uint8* InCompressedData, int32 InCompressedSize)
{
	if (InCompressedData == nullptr || InCompressedSize <= 0)
	{
		return;
	}

	FWendyVoiceSpeakerSlot* Slot = FindOrClaimSpeakerSlot(InSpeakerId);
	if (Slot == nullptr || false == Slot->Decoder.IsValid())
	{
		return;
	}

	// UDP reorders, so anything older than what we already played is stale - drop rather than play it late.
	if (Slot->bHasReceivedAny)
	{
		const int16 SequenceDelta = static_cast<int16>(InSequenceNumber - Slot->LastSequenceNumber);
		if (SequenceDelta <= 0)
		{
			return;
		}
	}
	Slot->LastSequenceNumber = InSequenceNumber;
	Slot->bHasReceivedAny = true;
	Slot->LastPacketTime = FPlatformTime::Seconds();

	uint32 DecodedBytes = static_cast<uint32>(DecodeScratch.Num() * sizeof(int16));
	Slot->Decoder->Decode(InCompressedData, static_cast<uint32>(InCompressedSize),
		reinterpret_cast<uint8*>(DecodeScratch.GetData()), DecodedBytes);

	const int32 DecodedSamples = static_cast<int32>(DecodedBytes / sizeof(int16));
	if (DecodedSamples > 0)
	{
		Slot->Buffer.Push(DecodeScratch.GetData(), static_cast<uint32>(DecodedSamples));
	}
}

void FWendyVoiceChat::ReleaseTimedOutSpeakers()
{
	if (false == MixerState.IsValid())
	{
		return;
	}

	const double CurrTime = FPlatformTime::Seconds();
	int32 ActiveSpeakers = 0;

	for (int32 SlotIdx = 0; SlotIdx < FWendyVoiceMixerState::MaxSpeakers; ++SlotIdx)
	{
		FWendyVoiceSpeakerSlot& Slot = MixerState->Slots[SlotIdx];
		if (Slot.bActive.GetValue() == 0)
		{
			continue;
		}

		if ((CurrTime - Slot.LastPacketTime) > WENDY_VOICE_SPEAKER_TIMEOUT_SECONDS)
		{
			// Freed for reuse. The decoder object is kept and simply Reset() next time, to avoid churn.
			Slot.bActive.Set(0);
			UE_LOG(LogWendyVoiceChat, Log, TEXT("Voice: speaker '%s' timed out, freeing slot %d"), *Slot.SpeakerId, SlotIdx);
			continue;
		}

		++ActiveSpeakers;
	}

	DebugActiveSpeakers.Set(ActiveSpeakers);

	// Endpoints expire on the same rule, so a departed client stops being sent to.
	if (bIsServer)
	{
		KnownEndpoints.RemoveAll([CurrTime](const FWendyVoiceRemoteEndpoint& InEndpoint)
			{
				return (CurrTime - InEndpoint.LastSeenTime) > WENDY_VOICE_SPEAKER_TIMEOUT_SECONDS;
			});
		DebugKnownEndpoints.Set(KnownEndpoints.Num());
	}
}

void FWendyVoiceChat::UpdateDebugCountersPerSecond()
{
	if (MixerState.IsValid())
	{
		// Deepest queue across speakers - that is what actually risks latency.
		int32 DeepestQueue = 0;
		for (int32 SlotIdx = 0; SlotIdx < FWendyVoiceMixerState::MaxSpeakers; ++SlotIdx)
		{
			const FWendyVoiceSpeakerSlot& Slot = MixerState->Slots[SlotIdx];
			if (Slot.bActive.GetValue() != 0)
			{
				DeepestQueue = FMath::Max(DeepestQueue, static_cast<int32>(Slot.Buffer.Num()));
			}
		}
		DebugPlaybackQueuedSamples.Set(DeepestQueue);
	}

	const double CurrTime = FPlatformTime::Seconds();
	if (CurrTime - LastDebugPublishTime >= 1.0)
	{
		// Rebuilt only once a second, which is why taking a lock for it is fine.
		if (MixerState.IsValid())
		{
			TArray<FString> SpeakerNames;
			for (int32 SlotIdx = 0; SlotIdx < FWendyVoiceMixerState::MaxSpeakers; ++SlotIdx)
			{
				const FWendyVoiceSpeakerSlot& Slot = MixerState->Slots[SlotIdx];
				if (Slot.bActive.GetValue() != 0)
				{
					SpeakerNames.Add(Slot.SpeakerId);
				}
			}

			FScopeLock SpeakerNamesLock(&ActiveSpeakerNamesMutex);
			ActiveSpeakerNames = (SpeakerNames.Num() > 0) ? FString::Join(SpeakerNames, TEXT(", ")) : TEXT("-");
		}

		// The mixer counts underruns cumulatively on the audio thread; publish the per-second delta.
		if (MixerState.IsValid())
		{
			const int32 CurrUnderrunCount = MixerState->UnderrunCount.GetValue();
			DebugUnderrunsPerSec.Set(FMath::Max(CurrUnderrunCount - LastUnderrunCountSnapshot, 0));
			LastUnderrunCountSnapshot = CurrUnderrunCount;
		}

		DebugFramesPerSec.Set(FramesThisSecond);
		DebugEncodedBytesPerSec.Set(EncodedBytesThisSecond);
		DebugPacketsSentPerSec.Set(PacketsSentThisSecond);
		DebugPacketsRecvPerSec.Set(PacketsRecvThisSecond);
		FramesThisSecond = 0;
		EncodedBytesThisSecond = 0;
		PacketsSentThisSecond = 0;
		PacketsRecvThisSecond = 0;
		LastDebugPublishTime = CurrTime;
	}
}

//////////////////////////

FWendyVoiceChatThreadWorker::FWendyVoiceChatThreadWorker(const FWendyWorldConnectingInfo& InConnectingInfo)
	: ConnectingInfo(InConnectingInfo)
{
}

FWendyVoiceChatThreadWorker::~FWendyVoiceChatThreadWorker()
{
}

bool FWendyVoiceChatThreadWorker::Init()
{
	UE_LOG(LogWendyVoiceChat, Log, TEXT("FWendyVoiceChatThreadWorker Init"));

	// Note: InitVoice() is called by UWendyGameInstance::InitVoiceChat on the game thread before this thread
	// starts, so the mixer state exists before any listener asks for it. Calling again is harmless
	// (InitVoice guards on bVoiceInitialized) and covers a worker started without that step.
	VoiceChat.InitVoice(ConnectingInfo);

	return true;
}

uint32 FWendyVoiceChatThreadWorker::Run()
{
	UE_LOG(LogWendyVoiceChat, Log, TEXT("FWendyVoiceChatThreadWorker Running"));

	while (StopTaskCounter.GetValue() == 0)
	{
		VoiceChat.UpdateTick();

		// Frames are 20 ms, so polling far faster than that only burns CPU. Unlike the image thread this
		// one deliberately sleeps - there is nothing to gain from spinning.
		FPlatformProcess::Sleep(0.005f);
	}

	UE_LOG(LogWendyVoiceChat, Log, TEXT("FWendyVoiceChatThreadWorker Stopping"));

	return 0;
}

void FWendyVoiceChatThreadWorker::Stop()
{
	StopTaskCounter.Increment();
}

void FWendyVoiceChatThreadWorker::Exit()
{
	VoiceChat.ShutdownVoice();

	UE_LOG(LogWendyVoiceChat, Log, TEXT("FWendyVoiceChatThreadWorker Exit"));
}
