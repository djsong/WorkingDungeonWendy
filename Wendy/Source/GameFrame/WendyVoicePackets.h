// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"

/** Voice gets its own UDP port, next door to the TCP image/input port (9918). */
const int32 WENDY_VOICE_PORT_NUMBER = 9919;

/** An Opus frame at 16 kHz mono is only tens of bytes; this is deliberate headroom. */
const int32 WENDY_VOICE_MAX_COMPRESSED_BYTES = 512;

/**
 * One UDP datagram of voice.
 *
 * Unlike the TCP image packets these are self-delimiting - one datagram is exactly one packet - so there is
 * no length-prefix framing, no accumulation buffer and none of the SerializeFromRecvBuffer machinery. Just a
 * tight header followed by the Opus payload:
 *
 *     [ SpeakerId : WD_USER_ID_MAX_LEN_PLUS_ONE TCHARs ][ Sequence : uint16 ][ Size : uint16 ][ Opus data ]
 *
 * A datagram with Size == 0 is a keepalive: it carries no audio and exists only so the server keeps learning
 * (and NAT keeps holding open) the sender's address while nobody is talking.
 */
struct FWendyVoicePacket
{
	static constexpr int32 SpeakerIdBytes = WD_USER_ID_MAX_LEN_PLUS_ONE * sizeof(TCHAR);
	static constexpr int32 HeaderBytes = SpeakerIdBytes + sizeof(uint16) + sizeof(uint16);
	static constexpr int32 MaxDatagramBytes = HeaderBytes + WENDY_VOICE_MAX_COMPRESSED_BYTES;

	FString SpeakerId;
	uint16 SequenceNumber = 0;
	/** Points INTO the datagram buffer rather than owning a copy, so it is only valid while that buffer is. */
	const uint8* CompressedData = nullptr;
	int32 CompressedSize = 0;

	/** Packs a datagram. Returns bytes written, or 0 if it wouldn't fit. InCompressedSize may be 0 (keepalive). */
	static int32 Write(uint8* OutDatagram, int32 InMaxDatagramBytes,
		const FString& InSpeakerId, uint16 InSequenceNumber,
		const uint8* InCompressedData, int32 InCompressedSize);

	/** Parses a received datagram. Returns false if it is malformed or truncated. */
	bool Read(const uint8* InDatagram, int32 InDatagramSize);
};
