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

/** Whether to send one by one (0) or altogether as array (1)
 * Set to 1 can give a chance to optimization, but seems unstable. */
#define WENDY_IMAGE_SEND_STAGING_BUNCH 0

/** Send-path locking strategy for the image staging (Step 2 perf work).
 * 0 = original: hold ImageDataAccessMutex across the whole send incl. blocking socket I/O (known-working, incl. real network).
 *		It had been working fine in general, by unintentionally hiding weakness of networking by long time game thread blocking.
 * 1 = decoupled: snapshot/swap staging under the lock, then do socket I/O outside it
 * Keep at 0 by default so normal builds work over the network; flip to 1 to test/debug the decoupled path. */
#define WD_DECOUPLED_IMAGE_SEND 1

/** Just a bunch */
struct FWendyBoundSocketAndRelevantInfo
{
	FSocket* SocketPtr = nullptr;
	TSharedPtr<FInternetAddr> BoundAddr = nullptr;
	FString UserIdentification;


	uint8 RecvBuffer[RECEIVE_SEND_BUFFER_SIZE] = { 0 };
	uint32 RecvBufferPointer = 0; // Write head: total valid bytes in RecvBuffer. Bigger than zero means there's something received and to be processed.
	uint32 RecvBufferReadOffset = 0; // Read head into RecvBuffer. Unread region is [RecvBufferReadOffset, RecvBufferPointer). Advanced while draining, compacted back to 0 once per drain.
	uint8 SendBuffer[RECEIVE_SEND_BUFFER_SIZE] = { 0 };
	uint32 SendBufferPointer = 0; // This being bigger than zero means there's something to send.

	/** Fast-path (WD_DECOUPLED_IMAGE_SEND) send-side backlog: image regions still waiting to go out on this
	 * connection, coalesced to the newest per owner + per UpdateBeginIndex (region). A newer capture of the same
	 * region overwrites the pending-but-unsent older one, so the backlog is bounded to one frame's worth of regions
	 * and what goes out is always the freshest. Fed into SendBuffer only as fast as it drains. Network-thread-only. */
	TMap<FString, TMap<int32, FWendyDesktopImageReplicateInfo>> PendingSendByOwnerRegion;
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
	FCriticalSection RemoteInputInfoMutex;
	
	/** An element per each SetSendImageInfo call, then removed on socket send. 
	 * Accessed from both game and this network thread. 
	 * Key is connected client's identifier (most likely UserId). */
	TMap<FString, TArray<FWendyDesktopImageReplicateInfo>> SendStagingReplicateInfo;
	
	/** Just the opposite of SendStagingReplicateInfo. 
	 * Key is the identifier of original image data owner. */
	TMap<FString, TArray<FWendyDesktopImageReplicateInfo>> RecvStagingReplicateInfo;
	
	/** If any Send/RecvStagingReplicateInfo bound to an account becomes larger than this value, it will be abandoned. */
	const int32 STAGING_DATA_SIZE_REGARDED_TOO_MUCH = 100000;

	TArray<FWendyMonitorHitAndInputInfo> SendStagingRemoteInputInfo;
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
	void SetSendImageInfo(const FString& ImageOwnerId, 
#if WENDY_IMAGE_SEND_STAGING_BUNCH
		const TArray<FWendyDesktopImageReplicateInfo>& ImageReplicateInfoToSend
#else
		const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend
#endif
	);
	void ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo);
	void MarkClientRemove(const FString& InClientId);
	/** Not exactly about "Image" replication, but whatever.. */
	void SetRemoteInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ConsumeRemoteInputInfo(TArray<FWendyMonitorHitAndInputInfo>& OutInfo);
private:

	static bool RawRecvAction(FSocket* InSocket, FInternetAddr& InAddr, uint8* RecvBuffer, uint32& RecvBufferPointer);
	static bool RawSendAction(FSocket* InSocket, FInternetAddr& InAddr, uint8* SendBuffer, uint32& SendBufferPointer);

#if WD_DECOUPLED_IMAGE_SEND
	/** Push as much of SendBuffer to the socket as it will accept right now, without sleeping/blocking.
	 * Stops as soon as the socket would block (its OS send buffer is full). Called every tick so a full
	 * SendBuffer always gets a chance to drain, independent of whether new packets can be appended. */
	static void DrainSendBufferNonBlocking(FSocket* InSocket, FInternetAddr& InAddr, uint8* SendBuffer, uint32& SendBufferPointer);
#endif

	/** Just putting repetitive common part together.
	 * Returns false when the packet could not even be queued because SendBuffer is full (caller should stop/requeue),
	 * true otherwise (packet was appended to SendBuffer and a flush was attempted). */
	static bool WrappedSendAction(FWendyImageRepPacketBase* SendPacket, FSocket* InSocket, FInternetAddr& InAddr, uint8* SendBuffer, uint32& SendBufferPointer);
	bool WrappedRecvAction_ImageData(uint8* RecvBuffer, uint32& RecvBufferReadOffset, uint32 RecvBufferPointer);

#if WD_DECOUPLED_IMAGE_SEND
	/** Coalesce a staging snapshot into Conn's per-region pending backlog (newest per owner+region wins), then feed
	 * that backlog into Conn.SendBuffer in ascending region order until SendBuffer is full, leaving the remainder
	 * pending for the next tick (requeue, never drop). Paces the send to the socket's drain rate. Network-thread-only. */
	void FeedConnectionFromStaging(FWendyBoundSocketAndRelevantInfo& Conn, const TMap<FString, TArray<FWendyDesktopImageReplicateInfo>>& StagingSnapshot);
#endif

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

	void SetSendImageInfo(const FString& ImageOwnerId, 
#if WENDY_IMAGE_SEND_STAGING_BUNCH
		const TArray<FWendyDesktopImageReplicateInfo>& ImageReplicateInfoToSend
#else
		const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend
#endif	
	);
	void ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo);
	void MarkClientRemove(const FString& InClientId);
	void SetRemoteInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ConsumeRemoteInputInfo(TArray<FWendyMonitorHitAndInputInfo>& OutInfo);
private:
	FThreadSafeCounter StopTaskCounter;

	FWendyImageRepNetwork ImageRepNetwork;
};