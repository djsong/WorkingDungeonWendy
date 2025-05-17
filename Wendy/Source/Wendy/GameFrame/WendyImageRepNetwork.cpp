// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyImageRepNetwork.h"
#include "HAL/PlatformProcess.h" // For FPlatformProcess::Sleep()
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "WendyImageRepPackets.h"
#include "Misc/MessageDialog.h"

DEFINE_LOG_CATEGORY_STATIC(LogWendyImageRepNetwork, Log, All);

static TAutoConsoleVariable<int32> CVarWdImageRepNetworkClientRecvMaxLoop(
	TEXT("wd.ImageRepNetwork.ClientRecvMaxLoop"),
	10,
	TEXT("Client need some recv loop to get data of multiple clients, but it might better if each client knows the number of other clients in network thread."),
	ECVF_ReadOnly);

FWendyImageRepNetwork::FWendyImageRepNetwork(const FWendyWorldConnectingInfo& InConnectingInfo)
	: bIsServer(InConnectingInfo.bMyselfServer)
	, ConnectIpAddrIfClient(InConnectingInfo.ServerIp)
	, SelfIdentification(InConnectingInfo.UserId)
{

}

void FWendyImageRepNetwork::InitAndOpenConnection()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	// Is UDP going to be enough for image transfer? Even if so, shouldn't we then prepare the code better for more exceptions?
	ConnectionBase.SocketPtr = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("WendyImageRepNetwork Socket"), FNetworkProtocolTypes::IPv4);
	ensureMsgf(ConnectionBase.SocketPtr != nullptr, TEXT("Hey, we cannot really do anything without it."));
	if (ConnectionBase.SocketPtr != nullptr)
	{
		ConnectionBase.SocketPtr->SetNonBlocking(true);
		int32 ReceiveSizeSet = 0;
		ConnectionBase.SocketPtr->SetReceiveBufferSize(RECEIVE_SEND_BUFFER_SIZE, ReceiveSizeSet);
		int32 SendSizeSet = 0;
		ConnectionBase.SocketPtr->SetSendBufferSize(RECEIVE_SEND_BUFFER_SIZE, SendSizeSet);
		UE_LOG(LogWendyImageRepNetwork, Log, TEXT("Socket ReceiveBufferSize %d, SendBufferSize %d"), ReceiveSizeSet, SendSizeSet);

		if (bIsServer)
		{
			ConnectionBase.SocketPtr->SetReuseAddr();
			ConnectionBase.SocketPtr->SetNoDelay();

			TSharedRef<FInternetAddr> ListenAddr = SocketSubsystem->GetLocalBindAddr(*GLog);
			ListenAddr->SetPort(COMMON_PORT_NUMBER);
			ConnectionBase.SocketPtr->Bind(ListenAddr.Get());
						
			if (ConnectionBase.SocketPtr->Listen(8))
			{
				bInitSuccessful = true;
				UE_LOG(LogWendyImageRepNetwork, Log, TEXT("WendyImageRepNetwork Server socket listening.."));
			}
			else
			{
				UE_LOG(LogWendyImageRepNetwork, Log, TEXT("WendyImageRepNetwork Server socket listen fail."));
			}
		}
		else
		{
			TSharedRef<FInternetAddr> ConnectAddr = SocketSubsystem->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
			
			bool bIsIpValid = false;
			ConnectAddr->SetIp(*ConnectIpAddrIfClient, bIsIpValid);
			ConnectAddr->SetPort(COMMON_PORT_NUMBER);

			if (ConnectionBase.SocketPtr->Connect(ConnectAddr.Get()))
			{
				ConnectionBase.BoundAddr = ConnectAddr.ToSharedPtr();
				bInitSuccessful = true;

				UE_LOG(LogWendyImageRepNetwork, Log, TEXT("WendyImageRepNetwork Client socket connecting.. Sending User information to the Server.."));

				FWendyImageRepPacket_UserInfo UserInfoPacket;
				UserInfoPacket.FromUserIdStr(SelfIdentification);


				if (UserInfoPacket.SerializeToSendBuffer(ConnectionBase.SendBuffer, ConnectionBase.SendBufferPointer))
				{
					// It will be sent just once here, so make sure it is being sent.
					int32 SentTryLimit = 100;
					while (SentTryLimit-- > 0)
					{
						RawSendAction(ConnectionBase.SocketPtr, *ConnectionBase.BoundAddr.Get(), ConnectionBase.SendBuffer, ConnectionBase.SendBufferPointer);

						if (ConnectionBase.SendBufferPointer > 0)
						{
							FPlatformProcess::Sleep(0.001f);
						}
						else
						{
							break;
						}
					}
					
					if (ConnectionBase.SendBufferPointer > 0)
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to send UserInfoPacket from the first place.. Try again.")));
						checkf(false, TEXT("Failed to send UserInfoPacket from the first place.. Try again."));						
					}
				}
			}
			else
			{
				UE_LOG(LogWendyImageRepNetwork, Log, TEXT("WendyImageRepNetwork Client socket connecting fail."));
			}
		}
	}
}

void FWendyImageRepNetwork::UpdateTick(float InDeltaSecond)
{
	if (bInitSuccessful && nullptr != ConnectionBase.SocketPtr)
	{
		if (bIsServer)
		{
			UpdateTickServer(InDeltaSecond);
		}
		else
		{
			UpdateTickClient(InDeltaSecond);
		}
	}
}

void FWendyImageRepNetwork::UpdateTickServer(float InDeltaSecond)
{
	FSocket* NewClient = ConnectionBase.SocketPtr->Accept(TEXT("New Image Rep Client"));
	if (NewClient != nullptr)
	{
		FWendyBoundSocketAndRelevantInfo NewInfo;
		NewInfo.SocketPtr = NewClient;
		NewInfo.BoundAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		NewClient->GetAddress(*NewInfo.BoundAddr);
		{
			FScopeLock ClientLock(&ClientInfoAccessMutex);
			ConnectedClients.Add(NewInfo);
		}
	}

	////////////////////
	//// The client should've sent some right after connection, so whatever need to recv first.
	for (int32 ClientIdx = 0; ClientIdx < ConnectedClients.Num(); ++ClientIdx)
	{
		FWendyBoundSocketAndRelevantInfo& ClientInfo = ConnectedClients[ClientIdx];

		if (RawRecvAction(ClientInfo.SocketPtr, *ClientInfo.BoundAddr.Get(), ClientInfo.RecvBuffer, ClientInfo.RecvBufferPointer))
		{
			// It might have accumulated to serialize more than once, so try flushing as much as possible.
			int32 SerializeLoopLimit = 10;
			while (SerializeLoopLimit-- > 0)
			{
				FWendyImageRepPacketBase AsBasePacket(EWendyImageRepPacketType::WIRP_END, 0);
				if (AsBasePacket.SerializeFromRecvBuffer_HeaderOnly(ClientInfo.RecvBuffer, ClientInfo.RecvBufferPointer))
				{
					if (AsBasePacket.PacketID == EWendyImageRepPacketType::WIRP_USERINFO)
					{
						FWendyImageRepPacket_UserInfo UserInfoPacket;
						// It will be serialized and return true only when received enough for this packet, or recv again and serialize later.
						if (UserInfoPacket.SerializeFromRecvBuffer(ClientInfo.RecvBuffer, ClientInfo.RecvBufferPointer))
						{
							ClientInfo.UserIdentification = UserInfoPacket.ToUserIdStr();

							UE_LOG(LogWendyImageRepNetwork, Log, TEXT("Received User identification from client %s (%s)"), *ClientInfo.UserIdentification, *ClientInfo.BoundAddr->ToString(false));
						}
					}
					else if (AsBasePacket.PacketID == EWendyImageRepPacketType::WIRP_IMAGEDATA)
					{
						FWendyImageRepPacket_ImageData ImagePacket;
						// It will be serialized and return true only when received enough for this packet, or recv again and serialize later.
						if (ImagePacket.SerializeFromRecvBuffer(ClientInfo.RecvBuffer, ClientInfo.RecvBufferPointer))
						{
							FScopeLock ImageLock(&ImageDataAccessMutex);

							FString ImageOwnerId;
							FWendyDesktopImageReplicateInfo ImageReplicateInfo;
							ImagePacket.ToReplicateInfo(ImageOwnerId, ImageReplicateInfo);
							ensureMsgf(ImageOwnerId == ClientInfo.UserIdentification, TEXT("Is there any case that some client (%s) sent some other (%s) client's image data?"), *ClientInfo.UserIdentification, *ImageOwnerId);

							TArray<FWendyDesktopImageReplicateInfo>& ReplicateInfoArrayRef = RecvStagingReplicateInfo.FindOrAdd(ImageOwnerId);
							ReplicateInfoArrayRef.Add(ImageReplicateInfo);
						}
						else
						{
							// Not enough data to serialize anymore until recv again.
							break;
						}
					}
					/*else if (PacketID == EWendyImageRepPacketType::...)
					{

					}*/
					else
					{
						ensureMsgf(0, TEXT("Well, there's no more packet, or we should think about the way to interpret recv data."));
					}
				}

				if (ClientInfo.RecvBufferPointer <= 0)
				{ // No more to serialze.
					break;
				}
			}
		}
	}

	//// Before proceed, remove marked clients
	{
		FScopeLock ClientsToRemoveLock(&ClientRemoveMutex);
		for (int32 ClientIdx = 0; ClientIdx < ConnectedClients.Num(); ++ClientIdx)
		{
			FWendyBoundSocketAndRelevantInfo& ClientInfo = ConnectedClients[ClientIdx];
			if (ClientsToRemove.Contains(ClientInfo.UserIdentification))
			{
				UE_LOG(LogWendyImageRepNetwork, Log, TEXT("Closing and destroying client %s as it is shown to be disconnected."), *ClientInfo.UserIdentification);

				ClientInfo.SocketPtr->Close();
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientInfo.SocketPtr);
			}
		}
		// Should also remove from array not to iterate anymore.
		ConnectedClients.RemoveAll([this](const FWendyBoundSocketAndRelevantInfo& ClientInfo)
			{
				return ClientsToRemove.Contains(ClientInfo.UserIdentification);
			});
		ClientsToRemove.Empty();
	}

	////////////////////
	//// To send 
	{
		FScopeLock ImageLock(&ImageDataAccessMutex);

		for (int32 ClientIdx = 0; ClientIdx < ConnectedClients.Num(); ++ClientIdx)
		{
			FWendyBoundSocketAndRelevantInfo& ClientInfo = ConnectedClients[ClientIdx];
		
			for (const auto& ItStagingRepInfo : SendStagingReplicateInfo)
			{
				// Omitting this check is not a big problem, then just sending its own data back again, won't be that pretty.
				if (ItStagingRepInfo.Key == ClientInfo.UserIdentification)
				{
					continue;
				}

				const TArray<FWendyDesktopImageReplicateInfo>& ImageReplicateInfoForClient = ItStagingRepInfo.Value;
				if (ImageReplicateInfoForClient.Num() > 0)
				{
					for (int32 RepIdx = 0; RepIdx < ImageReplicateInfoForClient.Num(); ++RepIdx)
					{
						const FWendyDesktopImageReplicateInfo& SendImageReplicateInfo = ImageReplicateInfoForClient[RepIdx];
						FWendyImageRepPacket_ImageData ImagePacket;
						ImagePacket.FromReplicateInfo(ItStagingRepInfo.Key, SendImageReplicateInfo);

						if (ImagePacket.SerializeToSendBuffer(ClientInfo.SendBuffer, ClientInfo.SendBufferPointer))
						{
							if (RawSendAction(ClientInfo.SocketPtr, *ClientInfo.BoundAddr.Get(), ClientInfo.SendBuffer, ClientInfo.SendBufferPointer))
							{
								
							}
							else
							{
								// Well we gonna try next chance.
							}
						}
						else
						{
							// If serialize failed, send staging data will be loss.
							UE_LOG(LogWendyImageRepNetwork, Warning, TEXT("SerializeToSendBuffer failed... Does SendBufferPointer overflow? %u"), ClientInfo.SendBufferPointer);
						}
					}
				}
			}

		}

		// Sent then empty.
		for (auto& ItStagingRepInfo : SendStagingReplicateInfo)
		{
			ItStagingRepInfo.Value.Empty();
		}
	}

	DisposeTooMuchStagingData();
}

void FWendyImageRepNetwork::UpdateTickClient(float InDeltaSecond)
{
	uint8 ReadHeaderBuffer[MAX_PACKET_SIZE] = { 0 };
	uint8 ReadDataBuffer[MAX_PACKET_SIZE] = { 0 };

	int32 RecvLoopLimit = FMath::Max(CVarWdImageRepNetworkClientRecvMaxLoop.GetValueOnAnyThread(), 1);
	while (RecvLoopLimit-- > 0)
	{
		if (RawRecvAction(ConnectionBase.SocketPtr, *ConnectionBase.BoundAddr.Get(), ConnectionBase.RecvBuffer, ConnectionBase.RecvBufferPointer))
		{
			// It might have accumulated to serialize more than once, so try flushing as much as possible.
			int32 SerializeLoopLimit = 10;
			while (SerializeLoopLimit-- > 0)
			{
				FWendyImageRepPacketBase AsBasePacket(EWendyImageRepPacketType::WIRP_END, 0);
				if (AsBasePacket.SerializeFromRecvBuffer_HeaderOnly(ConnectionBase.RecvBuffer, ConnectionBase.RecvBufferPointer))
				{
					if (AsBasePacket.PacketID == EWendyImageRepPacketType::WIRP_IMAGEDATA)
					{
						FWendyImageRepPacket_ImageData ImagePacket;
						// It will be serialized and return true only when received enough for this packet, or recv again and serialize later.
						if (ImagePacket.SerializeFromRecvBuffer(ConnectionBase.RecvBuffer, ConnectionBase.RecvBufferPointer))
						{
							FScopeLock ImageLock(&ImageDataAccessMutex);

							FString ImageOwnerId;
							FWendyDesktopImageReplicateInfo ImageReplicateInfo;
							ImagePacket.ToReplicateInfo(ImageOwnerId, ImageReplicateInfo);

							TArray<FWendyDesktopImageReplicateInfo>& ReplicateInfoArrayRef = RecvStagingReplicateInfo.FindOrAdd(ImageOwnerId);
							ReplicateInfoArrayRef.Add(ImageReplicateInfo);
						}
						else
						{
							// Not enough data to serialize anymore until recv again.
							break;
						}
					}
					/*else if (PacketID == EWendyImageRepPacketType::...)
					{

					}*/
					else
					{
						ensureMsgf(0, TEXT("Well, there's no more packet, or we should think about the way to interpret recv data."));
					}
				}
				if (ConnectionBase.RecvBufferPointer <= 0)
				{ // No more to serialize.
					break;
				}
			}
		}
		else
		{
			break;
		}
	}

	//// To send 
	{
		FScopeLock ImageLock(&ImageDataAccessMutex);
		ensureMsgf(SendStagingReplicateInfo.Num() <= 1, TEXT("Why client has more than its own data to send? %d"), SendStagingReplicateInfo.Num());
		TArray<FWendyDesktopImageReplicateInfo>* MyImageDataToSend = SendStagingReplicateInfo.Find(SelfIdentification);
		if(MyImageDataToSend != nullptr && MyImageDataToSend->Num() > 0)
		{
			for (int32 RepIdx = 0; RepIdx < MyImageDataToSend->Num(); ++RepIdx)
			{
				const FWendyDesktopImageReplicateInfo& SendImageReplicateInfo = (*MyImageDataToSend)[RepIdx];
				FWendyImageRepPacket_ImageData ImagePacket;
				ImagePacket.FromReplicateInfo(SelfIdentification, SendImageReplicateInfo);

				if (ImagePacket.SerializeToSendBuffer(ConnectionBase.SendBuffer, ConnectionBase.SendBufferPointer))
				{
					if (RawSendAction(ConnectionBase.SocketPtr, *ConnectionBase.BoundAddr.Get(), ConnectionBase.SendBuffer, ConnectionBase.SendBufferPointer))
					{

					}
					else
					{
						// Well we gonna try next chance.
					}
				}
				else
				{
					// If serialize failed, send staging data will be loss.
					UE_LOG(LogWendyImageRepNetwork, Warning, TEXT("SerializeToSendBuffer failed... Does SendBufferPointer overflow? %u"), ConnectionBase.SendBufferPointer);
				}
			}
		}

		// Sent then empty.
		for (auto& ItStagingRepInfo : SendStagingReplicateInfo)
		{
			ItStagingRepInfo.Value.Empty();
		}
	}

	DisposeTooMuchStagingData();
}

void FWendyImageRepNetwork::SetSendImageInfo(const FString& ImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend)
{
	{
		FScopeLock ImageLock(&ImageDataAccessMutex);
		ensureMsgf(bIsServer || ImageOwnerId == SelfIdentification, TEXT("Any case that client (%s) send other (%s) client's data?"), *SelfIdentification, *ImageOwnerId);
		TArray<FWendyDesktopImageReplicateInfo>& ReplicateInfoArrayRef = SendStagingReplicateInfo.FindOrAdd(ImageOwnerId);
		ReplicateInfoArrayRef.Add(ImageReplicateInfoToSend);
	}
}

void FWendyImageRepNetwork::ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo)
{
	{
		FScopeLock ImageLock(&ImageDataAccessMutex);
		TArray<FWendyDesktopImageReplicateInfo>* StagingImageInfo = RecvStagingReplicateInfo.Find(ImageOwnerId);
		if (StagingImageInfo != nullptr && StagingImageInfo->Num() > 0)
		{
			OutImageInfo.Append(*StagingImageInfo);
			StagingImageInfo->Empty();
		}
	}
}

void FWendyImageRepNetwork::MarkClientRemove(const FString& InClientId)
{
	{
		FScopeLock ClientsToRemoveLock(&ClientRemoveMutex);
		ClientsToRemove.Add(InClientId);
	}
}

bool FWendyImageRepNetwork::RawRecvAction(FSocket* InSocket, FInternetAddr& InAddr, uint8* RecvBuffer, uint32& RecvBufferPointer)
{
	constexpr int32 INTERNAL_BUFFER_SIZE = MAX_PACKET_SIZE;

	uint8 RawRecvBuffer[INTERNAL_BUFFER_SIZE] = { 0 };
	int32 ActualBytesRead = 0;
	if (InSocket->RecvFrom(RawRecvBuffer, INTERNAL_BUFFER_SIZE, ActualBytesRead, InAddr))
	{
		if (ActualBytesRead > 0)
		{
			FMemory::Memcpy(RecvBuffer + RecvBufferPointer, RawRecvBuffer, ActualBytesRead);
			RecvBufferPointer += ActualBytesRead;
			ensureMsgf(RecvBufferPointer < RECEIVE_SEND_BUFFER_SIZE, TEXT("There's no guarantee but how come actually got this far?"));
			return true;
		}
	}
	return false;
}

bool FWendyImageRepNetwork::RawSendAction(FSocket* InSocket, FInternetAddr& InAddr, uint8* SendBuffer, uint32& SendBufferPointer)
{
	uint8 RawSendBuffer[MAX_PACKET_SIZE] = { 0 };
	const uint32 MaxTryToSend = FMath::Min(SendBufferPointer, static_cast<uint32>(MAX_PACKET_SIZE));
	FMemory::Memcpy(RawSendBuffer, SendBuffer, MaxTryToSend);

	int32 ActualBytesSent = 0;
	if (InSocket->SendTo(RawSendBuffer, MaxTryToSend, ActualBytesSent, InAddr))
	{
		if (ActualBytesSent > 0)
		{
			SendBufferPointer = FMath::Max<uint32>(SendBufferPointer - static_cast<uint32>(ActualBytesSent), 0);

			for (int32 SendBfIdx = 0; SendBfIdx < static_cast<int32>(SendBufferPointer); ++SendBfIdx)
			{
				SendBuffer[SendBfIdx] = SendBuffer[SendBfIdx + ActualBytesSent];
			}
			return true;
		}
	}
	return false;
}

void FWendyImageRepNetwork::DisposeTooMuchStagingData()
{
	// If any staging info is not being processed this much and are being disposed here, it is likely to be wrong at some place,
	// so logging as error.
	{
		FScopeLock ImageLock(&ImageDataAccessMutex);

		for (auto& ItSendItStagingRepInfo : SendStagingReplicateInfo)
		{
			if (ItSendItStagingRepInfo.Value.Num() > STAGING_DATA_SIZE_REGARDED_TOO_MUCH)
			{
				UE_LOG(LogWendyImageRepNetwork, Error, TEXT("Throwing away too much ignored send staging replication info for %s"), *ItSendItStagingRepInfo.Key);
				ItSendItStagingRepInfo.Value.Empty();
			}
		}
		for (auto& ItRecvItStagingRepInfo : RecvStagingReplicateInfo)
		{
			if (ItRecvItStagingRepInfo.Value.Num() > STAGING_DATA_SIZE_REGARDED_TOO_MUCH)
			{
				UE_LOG(LogWendyImageRepNetwork, Error, TEXT("Throwing away too much ignored recv staging replication info for %s"), *ItRecvItStagingRepInfo.Key);
				ItRecvItStagingRepInfo.Value.Empty();
			}
		}
	}
}

//////////////////////////
FWendyImageRepNetworkThreadWorker::FWendyImageRepNetworkThreadWorker(const FWendyWorldConnectingInfo& InConnectingInfo)
	: ImageRepNetwork(InConnectingInfo)
{
}

FWendyImageRepNetworkThreadWorker::~FWendyImageRepNetworkThreadWorker()
{
}

bool FWendyImageRepNetworkThreadWorker::Init()
{
	UE_LOG(LogWendyImageRepNetwork, Log, TEXT("FWendyImageRepNetworkThreadWorker Init"));

	ImageRepNetwork.InitAndOpenConnection();

	return true;
}

uint32 FWendyImageRepNetworkThreadWorker::Run()
{
	// Initial wait before starting
	FPlatformProcess::Sleep(0.03f);

	UE_LOG(LogWendyImageRepNetwork, Log, TEXT("FWendyImageRepNetworkThreadWorker Running"));

	double LastLoopTime = FPlatformTime::Seconds();
	// Main thread loop
	while (StopTaskCounter.GetValue() == 0)
	{
		const double LoopBeginTime = FPlatformTime::Seconds();

		ImageRepNetwork.UpdateTick(static_cast<float>(LoopBeginTime - LastLoopTime));
		LastLoopTime = LoopBeginTime;

		const double LoopEndTime = FPlatformTime::Seconds();

		const double ThreadTickMinTime = 0.001;
		if (LoopEndTime - LoopBeginTime < ThreadTickMinTime)
		{
			FPlatformProcess::Sleep(ThreadTickMinTime - (LoopEndTime - LoopBeginTime));
		}
	}

	UE_LOG(LogWendyImageRepNetwork, Log, TEXT("FWendyImageRepNetworkThreadWorker Stopping"));

	return 0;
}

void FWendyImageRepNetworkThreadWorker::Stop()
{
	StopTaskCounter.Increment();
}

void FWendyImageRepNetworkThreadWorker::Exit()
{
	// Cleanup when thread exits
	UE_LOG(LogWendyImageRepNetwork, Log, TEXT("FWendyImageRepNetworkThreadWorker Exit"));
}


void FWendyImageRepNetworkThreadWorker::SetSendImageInfo(const FString& ImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend)
{
	ImageRepNetwork.SetSendImageInfo(ImageOwnerId, ImageReplicateInfoToSend);
}

void FWendyImageRepNetworkThreadWorker::ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo)
{
	ImageRepNetwork.ConsumeImageInfo(ImageOwnerId, OutImageInfo);
}

void FWendyImageRepNetworkThreadWorker::MarkClientRemove(const FString& InClientId)
{
	ImageRepNetwork.MarkClientRemove(InClientId);
}