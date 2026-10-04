// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"

/** Image-packet wire format switches. Image replication is the most sensitive part of Wendy, so like
 * WD_DECOUPLED_IMAGE_SEND these default OFF and can be A/B'd independently.
 *
 * 0 = every image packet is sent at the full struct size, even when it carries fewer pixels than the
 *     fixed ImageData array can hold (the original behaviour).
 * 1 = a packet is only as long as the pixels it actually carries. The receiving side takes the packet
 *     length from the header on the wire instead of assuming the local struct size.
 *
 * This is a prerequisite for WD_COMPRESSED_IMAGE_PACKET: a compressed payload is variable length by
 * definition, so the framing has to stop assuming a fixed size first. On its own it also trims the
 * partially-filled packet produced at each wrap-around of the image. */
#define WD_VARIABLE_SIZE_IMAGE_PACKET 1

/** 0 = pixels travel raw (3 bytes each).
 * 1 = each bunch's pixels are LZ4 compressed before sending, and a per-packet flag says whether the payload
 *     actually ended up compressed - a bunch that doesn't compress (photographic content, noise) is sent raw
 *     rather than slightly larger.
 *
 * Bandwidth is the measured bottleneck, not CPU, so trading compress/decompress work for fewer bytes is the
 * right direction here. Expect a lot on flat UI and text and very little on photos or video: LZ4 works within
 * one bunch (~1 KB), which is a small window, so ratios are well short of whole-frame compression. */
#define WD_COMPRESSED_IMAGE_PACKET 1

#if WD_COMPRESSED_IMAGE_PACKET && !WD_VARIABLE_SIZE_IMAGE_PACKET
	#error WD_COMPRESSED_IMAGE_PACKET needs WD_VARIABLE_SIZE_IMAGE_PACKET: a compressed payload is variable length, so the framing must stop assuming a fixed packet size first.
#endif

enum class EWendyImageRepPacketType : uint8
{
	WIRP_USERINFO,

	/** Definitely the main shit. */
	WIRP_IMAGEDATA,

	WIRP_REMOTEINPUT,

	// What else could be?


	WIRP_END
};

struct FWendyImageRepPacketBase
{
	FWendyImageRepPacketBase(EWendyImageRepPacketType InPacketId, uint32 InPacketSizeBytes)
		: PacketID(InPacketId)
#if WD_COMPRESSED_IMAGE_PACKET
		, bCompressedPayload(0)
#endif
		, PacketSizeBytes(InPacketSizeBytes)
	{
	}

	EWendyImageRepPacketType PacketID;

#if WD_COMPRESSED_IMAGE_PACKET
	/** 1 when this packet's payload is an LZ4 blob rather than raw bytes. Decided per packet, because some
	 * bunches don't compress and are better sent raw.
	 *
	 * Deliberately placed here: PacketID is one byte and PacketSizeBytes wants 4-byte alignment, so the base
	 * already carried 3 bytes of padding. Living in that padding means enabling compression changes no
	 * packet's size and shifts no field offset - see the static_asserts below that hold us to that. */
	uint8 bCompressedPayload;
#endif

	uint32 PacketSizeBytes;

	static int32 GetPacketHeaderSize()
	{
		// Just this base class is the header.
		return sizeof(FWendyImageRepPacketBase);
	}


	/** InMaxUsableBytes caps how far into SendBuffer this packet is allowed to write. Bulk image data passes a
	 * limit BELOW the buffer size so a reserve is always left for small latency-critical packets (remote
	 * input, user info), which have no requeue and are simply dropped if they don't fit. Control traffic
	 * passes the full buffer size. */
	bool SerializeToSendBuffer(uint8* OutSendBuffer, uint32& InOutSendBufferPointer, uint32 InMaxUsableBytes);

	/** Assumes PacketSizeBytes is already calculated before get to here.
	 * Reads the next packet from the unread region [InOutRecvBufferReadOffset, InRecvBufferPointer) of InRecvBuffer.
	 * On success advances InOutRecvBufferReadOffset by the packet size (no memory shift; the caller compacts once per drain).
	 * Returns true on serialize completion. */
	bool SerializeFromRecvBuffer(uint8* InRecvBuffer, uint32& InOutRecvBufferReadOffset, uint32 InRecvBufferPointer);
	/** In the case of serializing header, RecvBuffer and offsets won't get modified,
	 * so you can use it for checking whether the current read cursor points at some packet header data.
	 * Returns true on serialize completion. */
	bool SerializeFromRecvBuffer_HeaderOnly(uint8* InRecvBuffer, uint32 InRecvBufferReadOffset, uint32 InRecvBufferPointer);

	/** While this is at the base class, you must use it for derived class (struct).
	 * InAvailableBytes is the number of unread bytes currently sitting at the read cursor. */
	bool HasReceivedEnoughForPacketSerialize(uint32 InAvailableBytes) const;
};

/** Not directly about image, but we just need this to server holds all necessary information of clients. */
struct FWendyImageRepPacket_UserInfo : public FWendyImageRepPacketBase
{
	FWendyImageRepPacket_UserInfo()
		: FWendyImageRepPacketBase(EWendyImageRepPacketType::WIRP_USERINFO, CalculatePacketSizeBytes())
	{
		FMemory::Memset(UserId, 0);
	}

	TCHAR UserId[WD_USER_ID_MAX_LEN_PLUS_ONE];

	uint32 CalculatePacketSizeBytes() const;

	void FromUserIdStr(const FString& InUserIdStr);
	FString ToUserIdStr() const;
};


/** Not to FWendyImageRepPacket_ImageData over MAX_PACKET_SIZE */
const int32 WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE = ((MAX_PACKET_SIZE - 42) / sizeof(FWendyReplicatedColor));


/** From or to FWendyDesktopImageReplicateInfo */
struct FWendyImageRepPacket_ImageData : public FWendyImageRepPacketBase
{
	FWendyImageRepPacket_ImageData()
		: FWendyImageRepPacketBase(EWendyImageRepPacketType::WIRP_IMAGEDATA, CalculatePacketSizeBytes())
		, UpdateBeginIndex(0)
		, UpdateElemNum(0)
	{
		FMemory::Memset(ImageOwnerId, 0);
		FMemory::Memset(ImageData, 0);
	}

	/** Index of final buffer array, where this image data pointing. */
	int32 UpdateBeginIndex;

	int32 UpdateElemNum;

	TCHAR ImageOwnerId[WD_USER_ID_MAX_LEN_PLUS_ONE];

	FWendyReplicatedColor ImageData[WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE];

	uint32 CalculatePacketSizeBytes() const;

	void FromReplicateInfo(const FString& InImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfo);

	/** False means this packet couldn't be turned back into pixels (only possible when the payload arrived
	 * compressed and failed to decompress); the caller should drop it rather than apply garbage. */
	bool ToReplicateInfo(FString& OutImageOwnerId, FWendyDesktopImageReplicateInfo& OutImageReplicateInfo) const;
};

/** WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE is derived from a hard-coded 42-byte prefix, so that number has to keep
 * matching where ImageData actually starts. It was an unchecked assumption; these hold it to the compiler.
 * The compression flag above is only free because it fits in padding, which is exactly what this catches. */
static_assert(STRUCT_OFFSET(FWendyImageRepPacket_ImageData, ImageData) == 42,
	"ImageData no longer starts 42 bytes in - update the 42 in WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE to match.");
static_assert(sizeof(FWendyImageRepPacket_ImageData) <= MAX_PACKET_SIZE,
	"FWendyImageRepPacket_ImageData outgrew MAX_PACKET_SIZE.");


struct FWendyImageRepPacket_RemoteInput : public FWendyImageRepPacketBase
{
	FWendyImageRepPacket_RemoteInput()
		: FWendyImageRepPacketBase(EWendyImageRepPacketType::WIRP_REMOTEINPUT, CalculatePacketSizeBytes())
		, MonitorHitUV(-1.0f, -1.0f)
		, InputKey(EWendyRemoteInputKeys::None)
		, InputEvent(EWendyRemoteInputEvents::None)
		, bRelativeMouseMove(false)
		, MouseDelta(0.0f, 0.0f)
	{
		FMemory::Memset(TargetUserId, 0);
	}

	TCHAR TargetUserId[WD_USER_ID_MAX_LEN_PLUS_ONE];
	FVector2D MonitorHitUV;
	EWendyRemoteInputKeys InputKey = EWendyRemoteInputKeys::None;
	EWendyRemoteInputEvents InputEvent;
	/** See FWendyMonitorHitAndInputInfo: suppresses absolute cursor placement on the receiving side. */
	bool bRelativeMouseMove;
	FVector2D MouseDelta;


	uint32 CalculatePacketSizeBytes() const;

	void FromHitAndInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ToHitAndInputInfo(FWendyMonitorHitAndInputInfo& OutInfo);
};