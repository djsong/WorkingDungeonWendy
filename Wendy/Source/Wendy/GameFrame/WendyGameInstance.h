// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "Engine/GameInstance.h"
#include "WendyImageRepNetwork.h"
#include "WendyCommon.h"
#include "WendyGameInstance.generated.h"

/**
 * Initially created from ThirdPersion template.
 */
UCLASS(minimalapi)
class UWendyGameInstance : public UGameInstance
{
	GENERATED_BODY()

	TUniquePtr<FWendyImageRepNetworkThreadWorker> ImageRepNetworkThreadWorker = nullptr;
	FRunnableThread* ImageRepNetworkThread = nullptr;

	/** Being set while InitImageRepNetwork */
	bool bIsServer = false;

public:
	UWendyGameInstance(const FObjectInitializer& ObjectInitializer);
	
	virtual void Shutdown() override;

	/** Init called from outside, but Term is handled inside.
	 * @param InConnectIpAddrIfClient : Ignored if server */
	void InitImageRepNetwork(const FWendyWorldConnectingInfo& InConnectingInfo);
	void TermImageRepNetwork();


	////////////////////////////////////////////////////////////////////////
	// For ImageRepNetwork

	/** Theoretically you call it as much as you want still ImageRepNetwork should transfer.
	 * but in reality do not go that far.. */
	void SetSendImageInfo(const FString& ImageOwnerId, 
#if WENDY_IMAGE_SEND_STAGING_BUNCH
		const TArray<FWendyDesktopImageReplicateInfo>& ImageReplicateInfoToSend
#else
		const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend
#endif
	);
	void ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo);
	void MarkClientRemoveServerOnly(const FString& InClientId);
	void SetRemoteInputInfo(const FWendyMonitorHitAndInputInfo& InInfo);
	void ConsumeRemoteInputInfo(TArray<FWendyMonitorHitAndInputInfo>& OutInfo);
};


