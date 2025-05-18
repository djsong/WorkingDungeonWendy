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

bool FWendyImageRepPacketBase::SerializeFromRecvBuffer(uint8* InOutRecvBuffer, uint32& InOutRecvBufferPointer)
{
	if (HasReceivedEnoughForPacketSerialize(InOutRecvBufferPointer))
	{
		FMemory::Memcpy(this, InOutRecvBuffer, PacketSizeBytes);

		InOutRecvBufferPointer -= PacketSizeBytes;

		/*if (InOutRecvBufferPointer > 0)
		{
			UE_LOG(LogWendy, Warning, TEXT("Network Checking #1, Accumulated recv %u"), InOutRecvBufferPointer);
		}*/

		for (int32 RecvBfIdx = 0; RecvBfIdx < static_cast<int32>(InOutRecvBufferPointer); ++RecvBfIdx)
		{
			InOutRecvBuffer[RecvBfIdx] = InOutRecvBuffer[RecvBfIdx + PacketSizeBytes];
		}
		return true;
	}
	/*else
	{
		UE_LOG(LogWendy, Warning, TEXT("Network Checking #2, haven't recv enough %u"), InOutRecvBufferPointer);
	}*/

	return false;
}

bool FWendyImageRepPacketBase::SerializeFromRecvBuffer_HeaderOnly(uint8* InRecvBuffer, uint32 InRecvBufferPointer)
{
	if (static_cast<int32>(InRecvBufferPointer) >= GetPacketHeaderSize())
	{
		FMemory::Memcpy(this, InRecvBuffer, GetPacketHeaderSize());
		return true;
	}
	return false;
}

bool FWendyImageRepPacketBase::HasReceivedEnoughForPacketSerialize(uint32 InRecvBufferPointer) const
{
	ensureMsgf(static_cast<int32>(PacketSizeBytes) > GetPacketHeaderSize(), TEXT("Are you calling it as base struct?"));
	return (InRecvBufferPointer >= PacketSizeBytes);
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
