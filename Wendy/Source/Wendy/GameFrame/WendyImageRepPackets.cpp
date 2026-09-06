// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyImageRepPackets.h"
#include "WendyImageRepNetwork.h"

bool FWendyImageRepPacketBase::SerializeToSendBuffer(uint8* OutSendBuffer, uint32& InOutSendBufferPointer)
{
	// If over, wait until some buffered data get sent.
	if (InOutSendBufferPointer + PacketSizeBytes < RECEIVE_SEND_BUFFER_SIZE)
	{
		FMemory::Memcpy(OutSendBuffer + InOutSendBufferPointer, reinterpret_cast<uint8*>(this), PacketSizeBytes);
		InOutSendBufferPointer += PacketSizeBytes;
		return true;
	}
	return false;
}

bool FWendyImageRepPacketBase::SerializeFromRecvBuffer(uint8* InRecvBuffer, uint32& InOutRecvBufferReadOffset, uint32 InRecvBufferPointer)
{
	// InRecvBufferPointer is the write head (total valid bytes); the unread region is [ReadOffset, Pointer).
	const uint32 AvailableBytes = InRecvBufferPointer - InOutRecvBufferReadOffset;

#if WD_VARIABLE_SIZE_IMAGE_PACKET
	// The sender may have written fewer bytes than this struct's full size, so the authoritative length is
	// the one on the wire - our own PacketSizeBytes here is just the locally constructed maximum.
	if (static_cast<int32>(AvailableBytes) < GetPacketHeaderSize())
	{
		return false;
	}

	FWendyImageRepPacketBase WireHeader(EWendyImageRepPacketType::WIRP_END, 0);
	FMemory::Memcpy(&WireHeader, InRecvBuffer + InOutRecvBufferReadOffset, GetPacketHeaderSize());
	const uint32 WirePacketSizeBytes = WireHeader.PacketSizeBytes;

	// Never trust a length off the wire: it must be at least a header, and can never exceed the struct we
	// are about to copy into. Either would mean a corrupt stream, so bail rather than overrun.
	if (WirePacketSizeBytes < static_cast<uint32>(GetPacketHeaderSize()) || WirePacketSizeBytes > PacketSizeBytes)
	{
		ensureMsgf(false, TEXT("Bad image packet length on the wire: %u (local max %u)"), WirePacketSizeBytes, PacketSizeBytes);
		return false;
	}

	if (AvailableBytes >= WirePacketSizeBytes)
	{
		// Overwrites PacketSizeBytes with the wire value, which is what we want; any tail beyond it keeps
		// the constructor's zeroes.
		FMemory::Memcpy(this, InRecvBuffer + InOutRecvBufferReadOffset, WirePacketSizeBytes);

		// Advance the read cursor only; no memory shift here. The caller compacts once per drain.
		InOutRecvBufferReadOffset += WirePacketSizeBytes;
		return true;
	}
#else
	if (HasReceivedEnoughForPacketSerialize(AvailableBytes))
	{
		FMemory::Memcpy(this, InRecvBuffer + InOutRecvBufferReadOffset, PacketSizeBytes);

		// Advance the read cursor only; no memory shift here. The caller compacts once per drain.
		InOutRecvBufferReadOffset += PacketSizeBytes;
		return true;
	}
#endif
	/*else
	{
		UE_LOG(LogWendy, Warning, TEXT("Network Checking #2, haven't recv enough %u"), AvailableBytes);
	}*/

	return false;
}

bool FWendyImageRepPacketBase::SerializeFromRecvBuffer_HeaderOnly(uint8* InRecvBuffer, uint32 InRecvBufferReadOffset, uint32 InRecvBufferPointer)
{
	const uint32 AvailableBytes = InRecvBufferPointer - InRecvBufferReadOffset;
	if (static_cast<int32>(AvailableBytes) >= GetPacketHeaderSize())
	{
		FMemory::Memcpy(this, InRecvBuffer + InRecvBufferReadOffset, GetPacketHeaderSize());
		return true;
	}
	return false;
}

bool FWendyImageRepPacketBase::HasReceivedEnoughForPacketSerialize(uint32 InAvailableBytes) const
{
	ensureMsgf(static_cast<int32>(PacketSizeBytes) > GetPacketHeaderSize(), TEXT("Are you calling it as base struct?"));
	return (InAvailableBytes >= PacketSizeBytes);
}

/////////////////////////////////////////////

uint32 FWendyImageRepPacket_UserInfo::CalculatePacketSizeBytes() const
{
	const uint32 RetVal = static_cast<uint32>(sizeof(FWendyImageRepPacket_UserInfo));
	return RetVal;
}

void FWendyImageRepPacket_UserInfo::FromUserIdStr(const FString& InUserIdStr)
{
	ensureMsgf(InUserIdStr.Len() < WD_USER_ID_MAX_LEN_PLUS_ONE, TEXT("This Id (%s) cannot be fully sent through network"), *InUserIdStr);
	FMemory::Memcpy(this->UserId, InUserIdStr.GetCharArray().GetData(), WD_USER_ID_MAX_LEN_PLUS_ONE * sizeof(TCHAR));
}

FString FWendyImageRepPacket_UserInfo::ToUserIdStr() const
{
	return FString(this->UserId);
}

/////////////////////////////////////////////

uint32 FWendyImageRepPacket_ImageData::CalculatePacketSizeBytes() const
{
	//const uint32 RetVal = static_cast<uint32>(sizeof(FWendyImageRepPacketBase) + sizeof(UpdateBeginIndex) + sizeof(UpdateElemNum) + sizeof(ImageOwnerId) + sizeof(ImageData));
	const uint32 RetVal = static_cast<uint32>(sizeof(FWendyImageRepPacket_ImageData));

	ensureMsgf(RetVal <= MAX_PACKET_SIZE, TEXT("Packet size over the maximum %d - %d"), RetVal, MAX_PACKET_SIZE);
	return RetVal;
}

void FWendyImageRepPacket_ImageData::FromReplicateInfo(const FString& InImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfo)
{
	this->UpdateBeginIndex = ImageReplicateInfo.UpdateBeginIndex;
	this->UpdateElemNum = ImageReplicateInfo.UpdateElemNum;
	ensureMsgf(InImageOwnerId.Len() < WD_USER_ID_MAX_LEN_PLUS_ONE, TEXT("This Id (%s) cannot be fully sent through network"), *InImageOwnerId);
	FMemory::Memcpy(this->ImageOwnerId, InImageOwnerId.GetCharArray().GetData(), WD_USER_ID_MAX_LEN_PLUS_ONE * sizeof(TCHAR));
	ensureMsgf(ImageReplicateInfo.ImageData.Num() >= ImageReplicateInfo.UpdateElemNum, TEXT("UpdateElemNum should be the same or smaller than ImageData array."));

	const int32 UsedElemNum = FMath::Min(WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE, ImageReplicateInfo.UpdateElemNum);
	const SIZE_T CopySize = UsedElemNum * sizeof(FWendyReplicatedColor);
	FMemory::Memcpy(this->ImageData, ImageReplicateInfo.ImageData.GetData(), CopySize);

#if WD_VARIABLE_SIZE_IMAGE_PACKET
	// Tell the receiver only about the pixels that actually travelled. Without this, a wd.DesktopImageReplicateSize
	// raised above the packet's capacity would have the receiver read more pixels than were ever sent - it stays
	// within the struct so it can't crash, but it would apply whatever happened to be in the untouched tail.
	this->UpdateElemNum = UsedElemNum;

	// Send only as far as those pixels. Everything up to ImageData is fixed, so the length is that offset plus
	// the bytes copied above. STRUCT_OFFSET rather than (sizeof - sizeof(ImageData)): the struct has trailing
	// padding, so subtracting would overshoot.
	this->PacketSizeBytes = static_cast<uint32>(STRUCT_OFFSET(FWendyImageRepPacket_ImageData, ImageData) + CopySize);
#endif
}

void FWendyImageRepPacket_ImageData::ToReplicateInfo(FString& OutImageOwnerId, FWendyDesktopImageReplicateInfo& ImageReplicateInfo) const
{
	ImageReplicateInfo.UpdateBeginIndex = this->UpdateBeginIndex;
	ImageReplicateInfo.UpdateElemNum = this->UpdateElemNum;
	ImageReplicateInfo.ImageData.Empty(ImageReplicateInfo.UpdateElemNum);
	ImageReplicateInfo.ImageData.AddZeroed(ImageReplicateInfo.UpdateElemNum);
	OutImageOwnerId = FString(this->ImageOwnerId);

	const SIZE_T CopySize = ImageReplicateInfo.UpdateElemNum * sizeof(FWendyReplicatedColor);
	FMemory::Memcpy(ImageReplicateInfo.ImageData.GetData(), this->ImageData, CopySize);
}

uint32 FWendyImageRepPacket_RemoteInput::CalculatePacketSizeBytes() const
{
	const uint32 RetVal = static_cast<uint32>(sizeof(FWendyImageRepPacket_RemoteInput));
	return RetVal;
}

void FWendyImageRepPacket_RemoteInput::FromHitAndInputInfo(const FWendyMonitorHitAndInputInfo& InInfo)
{
	ensureMsgf(InInfo.TargetUserId.Len() < WD_USER_ID_MAX_LEN_PLUS_ONE, TEXT("This Id (%s) cannot be fully sent through network"), *InInfo.TargetUserId);
	FMemory::Memcpy(this->TargetUserId, InInfo.TargetUserId.GetCharArray().GetData(), WD_USER_ID_MAX_LEN_PLUS_ONE * sizeof(TCHAR));
	this->MonitorHitUV = InInfo.MonitorHitUV;
	this->InputKey = InInfo.InputKey;
	this->InputEvent = InInfo.InputEvent;
	this->bRelativeMouseMove = InInfo.bRelativeMouseMove;
	this->MouseDelta = InInfo.MouseDelta;
}
void FWendyImageRepPacket_RemoteInput::ToHitAndInputInfo(FWendyMonitorHitAndInputInfo& OutInfo)
{
	OutInfo.TargetUserId = FString(this->TargetUserId);
	OutInfo.MonitorHitUV = this->MonitorHitUV;
	OutInfo.InputKey = this->InputKey;
	OutInfo.InputEvent = this->InputEvent;
	OutInfo.bRelativeMouseMove = this->bRelativeMouseMove;
	OutInfo.MouseDelta = this->MouseDelta;
}