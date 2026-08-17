// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyVoicePackets.h"

int32 FWendyVoicePacket::Write(uint8* OutDatagram, int32 InMaxDatagramBytes,
	const FString& InSpeakerId, uint16 InSequenceNumber,
	const uint8* InCompressedData, int32 InCompressedSize)
{
	if (OutDatagram == nullptr || InCompressedSize < 0)
	{
		return 0;
	}
	if ((HeaderBytes + InCompressedSize) > InMaxDatagramBytes)
	{
		return 0;
	}

	ensureMsgf(InSpeakerId.Len() < WD_USER_ID_MAX_LEN_PLUS_ONE,
		TEXT("This Id (%s) cannot be fully sent through network"), *InSpeakerId);

	int32 WriteOffset = 0;

	// Fixed-width id field, zero filled, so the reader can always treat it as a null terminated string.
	FMemory::Memzero(OutDatagram, SpeakerIdBytes);
	const int32 CopyIdChars = FMath::Min(InSpeakerId.Len(), WD_USER_ID_MAX_LEN_PLUS_ONE - 1);
	if (CopyIdChars > 0)
	{
		FMemory::Memcpy(OutDatagram, InSpeakerId.GetCharArray().GetData(), CopyIdChars * sizeof(TCHAR));
	}
	WriteOffset += SpeakerIdBytes;

	FMemory::Memcpy(OutDatagram + WriteOffset, &InSequenceNumber, sizeof(uint16));
	WriteOffset += sizeof(uint16);

	const uint16 CompressedSize16 = static_cast<uint16>(InCompressedSize);
	FMemory::Memcpy(OutDatagram + WriteOffset, &CompressedSize16, sizeof(uint16));
	WriteOffset += sizeof(uint16);

	if (InCompressedSize > 0 && InCompressedData != nullptr)
	{
		FMemory::Memcpy(OutDatagram + WriteOffset, InCompressedData, InCompressedSize);
		WriteOffset += InCompressedSize;
	}

	return WriteOffset;
}

bool FWendyVoicePacket::Read(const uint8* InDatagram, int32 InDatagramSize)
{
	if (InDatagram == nullptr || InDatagramSize < HeaderBytes)
	{
		return false;
	}

	int32 ReadOffset = 0;

	// The id field is zero filled on write, but a corrupt/spoofed datagram might not be - bound the string
	// to the field so we can never run off the end of it.
	TCHAR SpeakerIdChars[WD_USER_ID_MAX_LEN_PLUS_ONE];
	FMemory::Memcpy(SpeakerIdChars, InDatagram, SpeakerIdBytes);
	SpeakerIdChars[WD_USER_ID_MAX_LEN_PLUS_ONE - 1] = TEXT('\0');
	SpeakerId = FString(SpeakerIdChars);
	ReadOffset += SpeakerIdBytes;

	FMemory::Memcpy(&SequenceNumber, InDatagram + ReadOffset, sizeof(uint16));
	ReadOffset += sizeof(uint16);

	uint16 CompressedSize16 = 0;
	FMemory::Memcpy(&CompressedSize16, InDatagram + ReadOffset, sizeof(uint16));
	ReadOffset += sizeof(uint16);

	CompressedSize = static_cast<int32>(CompressedSize16);
	// Guard against a claimed size that the datagram doesn't actually contain.
	if (CompressedSize > (InDatagramSize - ReadOffset))
	{
		CompressedData = nullptr;
		CompressedSize = 0;
		return false;
	}

	CompressedData = (CompressedSize > 0) ? (InDatagram + ReadOffset) : nullptr;
	return true;
}
