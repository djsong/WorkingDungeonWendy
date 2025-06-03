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

void UWendyGameInstance::SetSendImageInfo(const FString& ImageOwnerId, const FWendyDesktopImageReplicateInfo& ImageReplicateInfoToSend)
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