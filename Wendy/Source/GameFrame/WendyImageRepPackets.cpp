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
	if (HasReceivedEnoughForPacketSerialize(AvailableBytes))
	{
		FMemory::Memcpy(this, InRecvBuffer + InOutRecvBufferReadOffset, PacketSizeBytes);

		// Advance the read cursor only; no memory shift here. The caller compacts once per drain.
		InOutRecvBufferReadOffset += PacketSizeBytes;
		return true;
	}
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

	const SIZE_T CopySize = FMath::Min(WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE, ImageReplicateInfo.UpdateElemNum) * sizeof(FWendyReplicatedColor);
	FMemory::Memcpy(this->ImageData, ImageReplicateInfo.ImageData.GetData(), CopySize);
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