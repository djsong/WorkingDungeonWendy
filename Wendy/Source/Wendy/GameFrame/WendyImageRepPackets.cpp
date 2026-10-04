// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyImageRepPackets.h"
#include "Misc/Compression.h"
#include "WendyImageRepNetwork.h"

#if WD_COMPRESSED_IMAGE_PACKET
static TAutoConsoleVariable<int32> CVarWdDesktopImageCompression(
	TEXT("wd.DesktopImageCompression"),
	1,
	TEXT("Compress image bunches with LZ4 before sending (only available when WD_COMPRESSED_IMAGE_PACKET is on).")
	TEXT(" 0 sends raw. Safe to change at runtime and on one side only: every packet carries a flag saying")
	TEXT(" whether its own payload is compressed, so the receiver never needs the same setting."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdDesktopImageCompressionStats(
	TEXT("wd.DesktopImageCompressionStats"),
	0,
	TEXT("Log the achieved image compression ratio once a second, with bytes before/after and what share of")
	TEXT(" bunches were too incompressible to bother. This is the measurement that says whether compression is")
	TEXT(" paying off - wd.CountImageRepPerf counts pixels, which can't show a bytes-on-the-wire win."),
	ECVF_Default);
#endif

bool FWendyImageRepPacketBase::SerializeToSendBuffer(uint8* OutSendBuffer, uint32& InOutSendBufferPointer, uint32 InMaxUsableBytes)
{
	// If over, wait until some buffered data get sent.
	if (InOutSendBufferPointer + PacketSizeBytes < FMath::Min(InMaxUsableBytes, static_cast<uint32>(RECEIVE_SEND_BUFFER_SIZE)))
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

#if WD_COMPRESSED_IMAGE_PACKET
	// Try LZ4 straight into our payload (a different buffer from the source, so no aliasing). Note
	// CompressMemory reports success even when it EXPANDED the data, so the size comparison below is what
	// actually decides - without it an incompressible bunch would be sent slightly larger than raw.
	bool bPayloadCompressed = false;
	int64 CompressedSize = static_cast<int64>(sizeof(this->ImageData));

	if (CopySize > 0 && CVarWdDesktopImageCompression.GetValueOnAnyThread() > 0)
	{
		if (FCompression::CompressMemory(NAME_LZ4, this->ImageData, CompressedSize,
			ImageReplicateInfo.ImageData.GetData(), static_cast<int64>(CopySize)))
		{
			bPayloadCompressed = (CompressedSize < static_cast<int64>(CopySize));
		}
	}

	this->bCompressedPayload = bPayloadCompressed ? 1 : 0;

	// Raw fallback overwrites whatever the attempt left behind, so a rejected compression costs bytes on the
	// wire nothing - only the compress work we just threw away.
	if (false == bPayloadCompressed)
	{
		FMemory::Memcpy(this->ImageData, ImageReplicateInfo.ImageData.GetData(), CopySize);
	}

#else
	FMemory::Memcpy(this->ImageData, ImageReplicateInfo.ImageData.GetData(), CopySize);
#endif

#if WD_VARIABLE_SIZE_IMAGE_PACKET
	// Tell the receiver only about the pixels that actually travelled. Without this, a wd.DesktopImageReplicateSize
	// raised above the packet's capacity would have the receiver read more pixels than were ever sent - it stays
	// within the struct so it can't crash, but it would apply whatever happened to be in the untouched tail.
	this->UpdateElemNum = UsedElemNum;

	// Send only as far as the payload actually occupies (compressed or raw). Everything up to ImageData is
	// fixed, so the length is that offset plus those bytes. STRUCT_OFFSET rather than
	// (sizeof - sizeof(ImageData)): the struct has trailing padding, so subtracting would overshoot.
#if WD_COMPRESSED_IMAGE_PACKET
	const SIZE_T PayloadBytes = bPayloadCompressed ? static_cast<SIZE_T>(CompressedSize) : CopySize;
#else
	const SIZE_T PayloadBytes = CopySize;
#endif

	this->PacketSizeBytes = static_cast<uint32>(STRUCT_OFFSET(FWendyImageRepPacket_ImageData, ImageData) + PayloadBytes);

#if WD_COMPRESSED_IMAGE_PACKET
	if (CVarWdDesktopImageCompressionStats.GetValueOnAnyThread() > 0)
	{
		// Plain statics: this only ever runs on the image network's send path (one thread), and a slightly
		// stale debug figure costs nothing. This is the number that says whether compression is worth it -
		// the delivered-pixel counter can't tell you, since it measures pixels rather than bytes.
		static int64 AccumRawBytes = 0;
		static int64 AccumWireBytes = 0;
		static int64 AccumBunches = 0;
		static int64 AccumIncompressibleBunches = 0;
		static double LastStatsLogTime = 0.0;

		AccumRawBytes += static_cast<int64>(CopySize);
		AccumWireBytes += static_cast<int64>(PayloadBytes);
		++AccumBunches;
		if (false == bPayloadCompressed)
		{
			++AccumIncompressibleBunches;
		}

		const double CurrTime = FPlatformTime::Seconds();
		if (CurrTime - LastStatsLogTime >= 1.0)
		{
			if (AccumRawBytes > 0)
			{
				UE_LOG(LogWendy, Log, TEXT("Image compression: %.2fx (%lld KB -> %lld KB/s), %lld bunches, %lld%% sent raw"),
					static_cast<double>(AccumRawBytes) / static_cast<double>(AccumWireBytes),
					AccumRawBytes / 1024, AccumWireBytes / 1024,
					AccumBunches,
					(AccumIncompressibleBunches * 100) / AccumBunches);
			}

			AccumRawBytes = 0;
			AccumWireBytes = 0;
			AccumBunches = 0;
			AccumIncompressibleBunches = 0;
			LastStatsLogTime = CurrTime;
		}
	}
#endif
#endif
}

bool FWendyImageRepPacket_ImageData::ToReplicateInfo(FString& OutImageOwnerId, FWendyDesktopImageReplicateInfo& ImageReplicateInfo) const
{
	ImageReplicateInfo.UpdateBeginIndex = this->UpdateBeginIndex;
	ImageReplicateInfo.UpdateElemNum = this->UpdateElemNum;
	ImageReplicateInfo.ImageData.Empty(ImageReplicateInfo.UpdateElemNum);
	ImageReplicateInfo.ImageData.AddZeroed(ImageReplicateInfo.UpdateElemNum);
	OutImageOwnerId = FString(this->ImageOwnerId);

	const SIZE_T CopySize = ImageReplicateInfo.UpdateElemNum * sizeof(FWendyReplicatedColor);

#if WD_COMPRESSED_IMAGE_PACKET
	// Read the flag off the wire rather than our own compression setting: a sender with compression off (or an
	// incompressible bunch) sends raw, so the two sides don't have to agree on anything but the format.
	if (this->bCompressedPayload != 0)
	{
		// The uncompressed length is already known from UpdateElemNum, so it never had to go on the wire.
		const int64 CompressedSize = static_cast<int64>(this->PacketSizeBytes)
			- static_cast<int64>(STRUCT_OFFSET(FWendyImageRepPacket_ImageData, ImageData));

		if (CompressedSize <= 0 || CopySize == 0)
		{
			UE_LOG(LogWendy, Warning, TEXT("Compressed image packet has no payload (PacketSizeBytes %u, UpdateElemNum %d)."),
				this->PacketSizeBytes, this->UpdateElemNum);
			return false;
		}

		if (false == FCompression::UncompressMemory(NAME_LZ4, ImageReplicateInfo.ImageData.GetData(),
			static_cast<int64>(CopySize), this->ImageData, CompressedSize))
		{
			// Dropping the bunch is the right failure: this region simply keeps its previous pixels and gets
			// corrected by a later update, which is far better than writing garbage into the image.
			UE_LOG(LogWendy, Warning, TEXT("Failed to decompress image packet (%lld compressed -> %llu raw)."),
				CompressedSize, static_cast<uint64>(CopySize));
			return false;
		}

		return true;
	}
#endif

	FMemory::Memcpy(ImageReplicateInfo.ImageData.GetData(), this->ImageData, CopySize);
	return true;
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