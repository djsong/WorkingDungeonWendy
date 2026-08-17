// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "WendyCommon.h"

class FInternetAddr;
class FSocket;
class IVoiceCapture;
class IVoiceEncoder;
class IVoiceDecoder;

/** Mono 16 kHz is the engine's default voice rate, and is plenty for speech while keeping Opus frames tiny. */
const int32 WENDY_VOICE_SAMPLE_RATE = 16000;
const int32 WENDY_VOICE_NUM_CHANNELS = 1;
/** 20 ms per Opus frame - the usual balance between per-packet overhead and added latency. */
const int32 WENDY_VOICE_FRAME_SAMPLES = (WENDY_VOICE_SAMPLE_RATE / 50);
const int32 WENDY_VOICE_FRAME_BYTES = WENDY_VOICE_FRAME_SAMPLES * sizeof(int16);
/** Per-speaker ring holds ~1 second. Far more than the jitter we expect - the point of the slack is that the
 * producer and consumer never contend, since TCircularAudioBuffer truncates rather than blocks if they do. */
const int32 WENDY_VOICE_PLAYBACK_BUFFER_SAMPLES = WENDY_VOICE_SAMPLE_RATE;

/** UE's Opus decoder refuses to decode a frame unless the REMAINING output buffer is at least
 * MAX_OPUS_FRAMES (6) whole frames' worth, however little we actually hand it. Undersize this and every
 * decode silently does nothing except log "Decompression buffer too small to decode voice".
 * Doubled again for headroom - it's only a few KB. */
const int32 WENDY_VOICE_OPUS_MAX_FRAMES_PER_DECODE = 6;
const int32 WENDY_VOICE_DECODE_BUFFER_SAMPLES = WENDY_VOICE_FRAME_SAMPLES * WENDY_VOICE_OPUS_MAX_FRAMES_PER_DECODE * 2;

/** A speaker is dropped once nothing has been heard from them for this long, freeing their slot. */
const double WENDY_VOICE_SPEAKER_TIMEOUT_SECONDS = 5.0;

/**
 * One remote speaker's decoded audio, waiting for the audio thread to mix it.
 * Everything except Buffer and bActive is touched by the voice thread alone.
 */
struct FWendyVoiceSpeakerSlot
{
	FString SpeakerId;
	TSharedPtr<IVoiceDecoder> Decoder;
	uint16 LastSequenceNumber = 0;
	bool bHasReceivedAny = false;
	double LastPacketTime = 0.0;

	/** Written by the voice thread, read by the audio render thread. SPSC-safe. */
	Audio::TCircularAudioBuffer<int16> Buffer;

	/** Read by the audio thread to decide whether this slot is worth mixing. 0 = free. */
	FThreadSafeCounter bActive;
};

/**
 * The fixed set of speaker slots, shared between the voice thread and the audio render thread.
 *
 * Deliberately a fixed-size array allocated once: the audio thread walks it every callback without any
 * locking, so it must never reallocate or have elements added/removed underneath it. Claiming and releasing
 * a speaker is therefore just flipping bActive on an existing slot.
 */
class FWendyVoiceMixerState
{
public:
	static constexpr int32 MaxSpeakers = 8;

	FWendyVoiceMixerState();

	/** Audio thread: sums every active speaker into OutSamples. Returns how many samples actually carried
	 * audio (0 when nobody is talking), so the caller can fill the remainder with silence. */
	int32 MixInto(int16* OutSamples, int32 NumSamples);

	FWendyVoiceSpeakerSlot Slots[MaxSpeakers];

private:
	/** Audio thread only. Sized once at construction so the callback never allocates. */
	TArray<int16> MixScratch;
};

typedef TSharedPtr<FWendyVoiceMixerState, ESPMode::ThreadSafe> FWendyVoiceMixerStatePtr;

/** Server side: a client we have heard from, and can therefore send back to. */
struct FWendyVoiceRemoteEndpoint
{
	FString SpeakerId;
	TSharedPtr<FInternetAddr> Addr;
	double LastSeenTime = 0.0;
};

/**
 * Voice capture, encode, transport and decode. Playback is the synth component's job.
 *
 * Phase 2a: UDP between server and client only (no client-to-client relay yet - that is 2c). The server
 * binds the port and learns each client's address from the datagrams it receives; clients send to the
 * server's address, which they already know from the same connecting info the image network uses.
 */
class FWendyVoiceChat
{
public:
	FWendyVoiceChat();
	~FWendyVoiceChat();

	void InitVoice(const FWendyWorldConnectingInfo& InConnectingInfo);
	void ShutdownVoice();

	/** Runs on the voice worker thread. */
	void UpdateTick();

	bool IsVoiceInitialized() const { return bVoiceInitialized; }

	/** Shared with the synth component so the audio thread never has to touch a UObject or this class. */
	FWendyVoiceMixerStatePtr GetMixerState() const { return MixerState; }

	//////////////////////////
	// Debug readout. Written on the voice thread, read on the game thread.
	int32 GetDebugCaptureState() const { return DebugCaptureState.GetValue(); }
	int32 GetDebugMicPeak() const { return DebugMicPeak.GetValue(); }
	int32 GetDebugFramesPerSec() const { return DebugFramesPerSec.GetValue(); }
	int32 GetDebugEncodedBytesPerSec() const { return DebugEncodedBytesPerSec.GetValue(); }
	int32 GetDebugPlaybackQueuedSamples() const { return DebugPlaybackQueuedSamples.GetValue(); }
	int32 GetDebugPacketsSentPerSec() const { return DebugPacketsSentPerSec.GetValue(); }
	int32 GetDebugPacketsRecvPerSec() const { return DebugPacketsRecvPerSec.GetValue(); }
	int32 GetDebugActiveSpeakers() const { return DebugActiveSpeakers.GetValue(); }
	int32 GetDebugKnownEndpoints() const { return DebugKnownEndpoints.GetValue(); }

private:
	bool InitSocket();

	/** Pull whatever the mic has ready into CaptureAccumulator. */
	void PollCapturedAudio();
	/** Encode one 20 ms frame and route it onward (over UDP, and/or straight back if loopback is on). */
	void EncodeAndRouteFrame(const int16* InFrameSamples);

	void SendVoiceDatagram(uint16 InSequenceNumber, const uint8* InCompressedData, int32 InCompressedSize);
	void ReceiveVoiceDatagrams();
	/** Common landing point for audio from anywhere - the network, or the loopback shortcut. */
	void QueueFrameForSpeaker(const FString& InSpeakerId, uint16 InSequenceNumber,
		const uint8* InCompressedData, int32 InCompressedSize);
	/** Finds this speaker's slot, claiming a free one if they are new. Null when all slots are taken. */
	FWendyVoiceSpeakerSlot* FindOrClaimSpeakerSlot(const FString& InSpeakerId);
	void ReleaseTimedOutSpeakers();

	void UpdateDebugCountersPerSecond();

	// Basic information given at beginning, mirroring FWendyImageRepNetwork.
	bool bIsServer = false;
	FString ConnectIpAddrIfClient;
	FString SelfIdentification;

	TSharedPtr<IVoiceCapture> VoiceCapture;
	TSharedPtr<IVoiceEncoder> VoiceEncoder;

	FSocket* VoiceSocket = nullptr;
	/** Client: where the server lives. Server: unused (it replies to learned endpoints instead). */
	TSharedPtr<FInternetAddr> ServerAddr;
	/** Server: every client we have heard from. Voice thread only. */
	TArray<FWendyVoiceRemoteEndpoint> KnownEndpoints;
	/** Reused for RecvFrom so the voice thread doesn't allocate per datagram. */
	TSharedPtr<FInternetAddr> RecvFromAddr;

	uint16 NextSequenceNumber = 0;
	double LastDatagramSendTime = 0.0;

	/** Mic PCM that doesn't yet make up a whole Opus frame. */
	TArray<int16> CaptureAccumulator;
	/** Reused every frame so the voice thread doesn't allocate inside its loop. */
	TArray<uint8> RawCaptureScratch;
	TArray<uint8> EncodeScratch;
	TArray<int16> DecodeScratch;
	TArray<uint8> DatagramScratch;

	FWendyVoiceMixerStatePtr MixerState;

	bool bVoiceInitialized = false;

	FThreadSafeCounter DebugCaptureState;
	FThreadSafeCounter DebugMicPeak;
	FThreadSafeCounter DebugFramesPerSec;
	FThreadSafeCounter DebugEncodedBytesPerSec;
	FThreadSafeCounter DebugPlaybackQueuedSamples;
	FThreadSafeCounter DebugPacketsSentPerSec;
	FThreadSafeCounter DebugPacketsRecvPerSec;
	FThreadSafeCounter DebugActiveSpeakers;
	FThreadSafeCounter DebugKnownEndpoints;
	/** Accumulated within the current second, then published to the *PerSec counters above. */
	int32 FramesThisSecond = 0;
	int32 EncodedBytesThisSecond = 0;
	int32 PacketsSentThisSecond = 0;
	int32 PacketsRecvThisSecond = 0;
	double LastDebugPublishTime = 0.0;
};

//////////////////////////

/** Mirrors FWendyImageRepNetworkThreadWorker: the voice work is all polling, so it gets its own thread. */
class FWendyVoiceChatThreadWorker : public FRunnable
{
public:
	FWendyVoiceChatThreadWorker(const FWendyWorldConnectingInfo& InConnectingInfo);
	virtual ~FWendyVoiceChatThreadWorker();

	// Begin FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
	// End FRunnable interface

	FWendyVoiceChat& GetVoiceChat() { return VoiceChat; }

private:
	FThreadSafeCounter StopTaskCounter;

	FWendyWorldConnectingInfo ConnectingInfo;
	FWendyVoiceChat VoiceChat;
};
