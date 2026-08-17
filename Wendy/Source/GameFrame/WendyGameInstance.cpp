// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyGameInstance.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "Kismet/GameplayStatics.h"

UWendyGameInstance::UWendyGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UWendyGameInstance::Shutdown()
{
	Super::Shutdown();

	TermImageRepNetwork();
	TermVoiceChat();
}

void UWendyGameInstance::InitImageRepNetwork(const FWendyWorldConnectingInfo& InConnectingInfo)
{
	ImageRepNetworkThreadWorker = MakeUnique<FWendyImageRepNetworkThreadWorker>(InConnectingInfo);
	ImageRepNetworkThread = FRunnableThread::Create(ImageRepNetworkThreadWorker.Get(), TEXT("WendyImageRepNetworkThread"));

	bIsServer = InConnectingInfo.bMyselfServer;
}

void UWendyGameInstance::TermImageRepNetwork()
{
	if (ImageRepNetworkThreadWorker.IsValid())
	{
		ImageRepNetworkThreadWorker->Stop();
	}

	if (ImageRepNetworkThread)
	{
		ImageRepNetworkThread->WaitForCompletion();
		ImageRepNetworkThread->Kill();
		ImageRepNetworkThread = nullptr;
	}

	if (ImageRepNetworkThreadWorker.IsValid())
	{
		ImageRepNetworkThreadWorker.Reset();
	}
}

void UWendyGameInstance::InitVoiceChat(const FWendyWorldConnectingInfo& InConnectingInfo)
{
	if (VoiceChatThreadWorker.IsValid())
	{
		return;
	}

	VoiceChatThreadWorker = MakeUnique<FWendyVoiceChatThreadWorker>(InConnectingInfo);

	// Deliberately initialized here on the game thread rather than inside FRunnable::Init(): that runs on
	// the worker thread and would race with characters' BeginPlay, which grab the mixer state. Doing it
	// synchronously first makes the ordering deterministic - it always exists before anyone asks.
	VoiceChatThreadWorker->GetVoiceChat().InitVoice(InConnectingInfo);

	VoiceChatThread = FRunnableThread::Create(VoiceChatThreadWorker.Get(), TEXT("WendyVoiceChatThread"));
}

void UWendyGameInstance::TermVoiceChat()
{
	if (VoiceChatThreadWorker.IsValid())
	{
		VoiceChatThreadWorker->Stop();
	}

	if (VoiceChatThread)
	{
		VoiceChatThread->WaitForCompletion();
		VoiceChatThread->Kill();
		VoiceChatThread = nullptr;
	}

	if (VoiceChatThreadWorker.IsValid())
	{
		VoiceChatThreadWorker.Reset();
	}
}

FWendyVoiceChat* UWendyGameInstance::GetVoiceChat() const
{
	return VoiceChatThreadWorker.IsValid() ? &VoiceChatThreadWorker->GetVoiceChat() : nullptr;
}

FWendyVoiceMixerStatePtr UWendyGameInstance::GetVoiceMixerState() const
{
	FWendyVoiceChat* VoiceChat = GetVoiceChat();
	return VoiceChat != nullptr ? VoiceChat->GetMixerState() : nullptr;
}

void UWendyGameInstance::SetSendImageInfo(const FString& ImageOwnerId,
#if WENDY_IMAGE_SEND_STAGING_BUNCH
	const TArray<FWendyDesktopImageReplicateInfo>& ImageReplicateInfoToSend
#else
	const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend
#endif
)
{
	if (ImageRepNetworkThreadWorker.IsValid())
	{
		ImageRepNetworkThreadWorker->SetSendImageInfo(ImageOwnerId, ImageReplicateInfoToSend);
	}
}

void UWendyGameInstance::ConsumeImageInfo(const FString& ImageOwnerId, TArray<FWendyDesktopImageReplicateInfo>& OutImageInfo)
{
	if (ImageRepNetworkThreadWorker.IsValid())
	{
		ImageRepNetworkThreadWorker->ConsumeImageInfo(ImageOwnerId, OutImageInfo);
	}
}

void UWendyGameInstance::MarkClientRemoveServerOnly(const FString& InClientId)
{
	if (bIsServer)
	{
		if (ImageRepNetworkThreadWorker.IsValid())
		{
			ImageRepNetworkThreadWorker->MarkClientRemove(InClientId);
		}
	}
}

void UWendyGameInstance::SetRemoteInputInfo(const FWendyMonitorHitAndInputInfo& InInfo)
{
	if (ImageRepNetworkThreadWorker.IsValid())
	{
		ImageRepNetworkThreadWorker->SetRemoteInputInfo(InInfo);
	}
}

void UWendyGameInstance::ConsumeRemoteInputInfo(TArray<FWendyMonitorHitAndInputInfo>& OutInfo)
{
	if (ImageRepNetworkThreadWorker.IsValid())
	{
		ImageRepNetworkThreadWorker->ConsumeRemoteInputInfo(OutInfo);
	}
}