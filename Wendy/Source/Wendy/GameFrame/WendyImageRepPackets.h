// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"

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
		, PacketSizeBytes(InPacketSizeBytes)
	{
	}

	EWendyImageRepPacketType PacketID;
	uint32 PacketSizeBytes;

	static int32 GetPacketHeaderSize()
	{
		// Just this base class is the header.
		return sizeof(FWendyImageRepPacketBase);
	}


	bool SerializeToSendBuffer(uint8* OutSendBuffer, uint32& InOutSendBufferPointer);

	/** Assumes PacketSizeBytes is already calculated before get to here.
	 * InOutRecvBuffer will get pushed front as much as serialized bytes, also modifies InOutRecvBufferPointer.
	 * Returns true on serialize completion. */
	bool SerializeFromRecvBuffer(uint8* InOutRecvBuffer, uint32& InOutRecvBufferPointer);
	/** In the case of serializing header, RecvBuffer and pointer won't get modified,
	 * so you can use it for checking whether pointing buffer position contains some packet header data.
	 * Returns true on serialize completion. */
	bool SerializeFromRecvBuffer_HeaderOnly(uint8* InRecvBuffer, uint32 InRecvBufferPointer);

	/** While this is at the base class, you must use it for derived class (struct) */
	bool HasReceivedEnoughForPacketSerialize(uint32 InRecvBufferPointer) const;
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
	void ToReplicateInfo(FString& OutImageOwnerId, FWendyDesktopImageReplicateInfo& OutImageReplicateInfo) const;
};


struct FWendyImageRepPacket_RemoteInput : public FWendyImageRepPacketBase
{
	FWendyImageRepPacket_RemoteInput()
		: FWendyImageRepPacketBase(EWendyImageRepPacketType::WIRP_REMOTEINPUT, CalculatePacketSizeBytes())
		, MonitorHitUV(-1.0f, -1.0f)
		, InputKey(EWendyRemoteInputKeys::None)
		, InputEvent(EWendyRemoteInputEvents::None)
	{
		FMemory::Memset(TargetUserId, 0);
	}

	TCHAR TargetUserId[WD_USER_ID_MAX_LEN_PLUS_ONE];
	FVector2D MonitorHitUV;
	EWendyRemoteInputKeys InputKey = EWendyRemoteInputKeys::None;
	EWendyRemoteInputEvents InputEvent;


	uint32 CalculatePacketSizeBytes() const;

	void FromHitAndInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ToHitAndInputInfo(FWendyMonitorHitAndInputInfo& OutInfo);
};