// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "WendyCommon.h"
#include "WendyImageRepPackets.h"

class FSocket; 

/** Need to be bigger than the max packet size. */
const int32 RECEIVE_SEND_BUFFER_SIZE = MAX_PACKET_SIZE * 1000;

/** Just a bunch */
struct FWendyBoundSocketAndRelevantInfo
{
	FSocket* SocketPtr = nullptr;
	TSharedPtr<FInternetAddr> BoundAddr = nullptr;
	FString UserIdentification;


	uint8 RecvBuffer[RECEIVE_SEND_BUFFER_SIZE] = { 0 };
	uint32 RecvBufferPointer = 0; // This being bigger than zero means there's something received and to be processed
	uint8 SendBuffer[RECEIVE_SEND_BUFFER_SIZE] = { 0 };
	uint32 SendBufferPointer = 0; // This being bigger than zero means there's something to send.
};

/** Image data is too big to send effectively by Unreal replication, so make its own networking running in thread */
class FWendyImageRepNetwork 
{
	// Basic information given at beginning.
	bool bIsServer;
	FString ConnectIpAddrIfClient;
	FString SelfIdentification; // Like UserId.

	/** Mostly used for client, but in the case of the server however, only socket is used for initial connection and accept. */
	FWendyBoundSocketAndRelevantInfo ConnectionBase;

	/** For server side */
	TArray<FWendyBoundSocketAndRelevantInfo> ConnectedClients;
	/** When it has element(s), matching element(s) of ConnectedClients should be removed */
	TArray<FString> ClientsToRemove;

	static const int32 COMMON_PORT_NUMBER = 9918;
	
	bool bInitSuccessful = false;

	/** Much data come from/goes to the game thread, so need lock between thread. */
	FCriticalSection ClientInfoAccessMutex;
	FCriticalSection ImageDataAccessMutex;
	FCriticalSection ClientRemoveMutex;
	FCriticalSection RemoteInoputInfoMutex;
	
	/** An element per each SetSendImageInfo call, then removed on socket send. 
	 * Accessed from both game and this network thread. 
	 * Key is connected client's identifier (most likely UserId). */
	TMap<FString, TArray<FWendyDesktopImageReplicateInfo>> SendStagingReplicateInfo;

	/** Just the opposite of SendStagingReplicateInfo. 
	 * Key is the identifier of original image data owner. */
	TMap<FString, TArray<FWendyDesktopImageReplicateInfo>> RecvStagingReplicateInfo;
	
	/** If any Send/RecvStagingReplicateInfo bound to an account becomes larger than this value, it will be abandoned. */
	const int32 STAGING_DATA_SIZE_REGARDED_TOO_MUCH = 100000;

	TArray<FWendyMonitorHitAndInputInfo> SendStagingRemoteInoputInfo;
	TArray<FWendyMonitorHitAndInputInfo> RecvStagingRemoteInoputInfo;
	/** To control the frequency of sending mouse cursor movement. */
	double LastTimeRemoteInputStagingForSend = 0.0;
	/** Not to send cursor movement while cursor is not moving. */
	FVector2D LastTimeRemoteInputStagingUV = FVector2D::ZeroVector;

public:
	FWendyImageRepNetwork(const FWendyWorldConnectingInfo& InConnectingInfo);

	void InitAndOpenConnection();

	void UpdateTick(float InDeltaSecond);
		void UpdateTickServer(float InDeltaSecond);
		void UpdateTickClient(float InDeltaSecond);
		 
	/** Feeding/Consuming from main thread */
	void SetSendImageInfo(const FString& ImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend);
	void ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo);
	void MarkClientRemove(const FString& InClientId);
	/** Not exactly about "Image" replication, but whatever.. */
	void SetRemoteInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ConsumeRemoteInputInfo(TArray<FWendyMonitorHitAndInputInfo>& OutInfo);
private:

	static bool RawRecvAction(FSocket* InSocket, FInternetAddr& InAddr, uint8* RecvBuffer, uint32& RecvBufferPointer);
	static bool RawSendAction(FSocket* InSocket, FInternetAddr& InAddr, uint8* SendBuffer, uint32& SendBufferPointer);

	/** Just putting repetitive common part together. */
	static void WrappedSendAction(FWendyImageRepPacketBase* SendPacket, FSocket* InSocket, FInternetAddr& InAddr, uint8* SendBuffer, uint32& SendBufferPointer);
	bool WrappedRecvAction_ImageData(uint8* RecvBuffer, uint32& RecvBufferPointer);

	/** Doing something if Send/RecvStaging data gets too big (by any unexpected reason) */
	void DisposeTooMuchStagingData();

};

//////////////////////////

class FWendyImageRepNetworkThreadWorker : public FRunnable
{
public:
	/** InConnectIpAddrIfClient would be ignored for server */
	FWendyImageRepNetworkThreadWorker(const FWendyWorldConnectingInfo& InConnectingInfo);
	virtual ~FWendyImageRepNetworkThreadWorker();

	// Begin FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
	// End FRunnable interface

	void SetSendImageInfo(const FString& ImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend);
	void ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo);
	void MarkClientRemove(const FString& InClientId);
	void SetRemoteInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ConsumeRemoteInputInfo(TArray<FWendyMonitorHitAndInputInfo>& OutInfo);
private:
	FThreadSafeCounter StopTaskCounter;

	FWendyImageRepNetwork ImageRepNetwork;
};