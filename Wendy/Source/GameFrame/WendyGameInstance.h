// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "Engine/GameInstance.h"
#include "WendyImageRepNetwork.h"
#include "WendyVoiceChat.h"
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

	TUniquePtr<FWendyVoiceChatThreadWorker> VoiceChatThreadWorker = nullptr;
	FRunnableThread* VoiceChatThread = nullptr;

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
	// For VoiceChat

	/** Init called from outside alongside InitImageRepNetwork; Term is handled inside. */
	void InitVoiceChat(const FWendyWorldConnectingInfo& InConnectingInfo);
	void TermVoiceChat();

	/** Null when voice chat isn't running. Game thread use only (the audio thread must go through the
	 * mixer state instead, which is shared by value for exactly that reason). */
	FWendyVoiceChat* GetVoiceChat() const;
	FWendyVoiceMixerStatePtr GetVoiceMixerState() const;


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


