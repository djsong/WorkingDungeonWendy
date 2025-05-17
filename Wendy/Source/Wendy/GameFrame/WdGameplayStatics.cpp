// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "WdGameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "WendyCharacter.h"
#include "WendyDataStore.h"
#include "WendyGameSettings.h"
#include "WendyGameInstance.h"
#include "SocketSubsystem.h"

APlayerController* UWdGameplayStatics::GetLocalPlayerController(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController->IsLocalController())
			{
				return PlayerController;
			}
		}
	}
	return nullptr;
}

APawn* UWdGameplayStatics::GetLocalPlayerPawn(const UObject* WorldContextObject)
{
	APlayerController* PC = GetLocalPlayerController(WorldContextObject);
	return PC ? PC->GetPawnOrSpectator() : nullptr;
}

AWendyCharacter* UWdGameplayStatics::GetLocalPlayerCharacter(const UObject* WorldContextObject)
{
	APlayerController* PC = GetLocalPlayerController(WorldContextObject);
	return PC ? Cast<AWendyCharacter>(PC->GetPawn()) : nullptr;
}

void UWdGameplayStatics::EnterWendyWorld(UObject* WorldContextObject, const FWendyWorldConnectingInfo& ConnectingInfo)
{
	FString LevelNameString;
	FString OptionsString;

	FWendyWorldConnectingInfo CheckedConnectingInfo = ConnectingInfo;	
	if (CheckedConnectingInfo.UserId.Len() >= WD_USER_ID_MAX_LEN_PLUS_ONE)
	{
		CheckedConnectingInfo.UserId = GetClampedWendyUserIdString(ConnectingInfo.UserId);
		UE_LOG(LogWendy, Warning, TEXT("User Account %s clamped to %s"), *CheckedConnectingInfo.UserId, *CheckedConnectingInfo.UserId);
	}

	if (CheckedConnectingInfo.bMyselfServer)
	{
		const UWendyGameSettings* WdGameSettings = GetDefault<UWendyGameSettings>(UWendyGameSettings::StaticClass());
		if (WdGameSettings != nullptr)
		{
			LevelNameString = WdGameSettings->MainWorldPath;
		}
		OptionsString += TEXT("?Listen");
	}
	else
	{
		LevelNameString = CheckedConnectingInfo.ServerIp;
	}

	FWendyDataStore& WendyDataStore = GetGlobalWendyDataStore();
	FWendyAccountInfo UserAccountInfo;
	UserAccountInfo.UserId = CheckedConnectingInfo.UserId;
	WendyDataStore.SetUserAccountInfo(UserAccountInfo);
	
	OpenLevel(WorldContextObject, *LevelNameString, true, OptionsString);

	// Here is the place where image replication networking is getting on, instead of GameMode because GameMode doesn't exist at client side.
	UWendyGameInstance* WendyGameInst = Cast<UWendyGameInstance>(GetGameInstance(WorldContextObject));
	if (IsValid(WendyGameInst))
	{
		WendyGameInst->InitImageRepNetwork(CheckedConnectingInfo);
	}
}

void TryClampViewForWholeInclusiveDesktopCapture(UObject* WorldContextObject)
{
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);

	int32 ViewSizeX = 0, ViewSizeY = 0;
	APlayerController* LocalPC = UWdGameplayStatics::GetLocalPlayerController(WorldContextObject);
	FViewport* LocalViewport = nullptr;
	if (LocalPC != nullptr)
	{
		LocalPC->GetViewportSize(ViewSizeX, ViewSizeY);

		ULocalPlayer* LocPlayer = Cast<ULocalPlayer>(LocalPC->Player);
		if (LocPlayer != nullptr && LocPlayer->ViewportClient != nullptr)
		{
			LocalViewport = LocPlayer->ViewportClient->Viewport;
		}
	}

	// #WendyDealsWithPrimaryDisplayOnly
	if (ViewSizeX > 0 && ViewSizeY > 0 && 
		DisplayMetrics.PrimaryDisplayWidth > 1 && DisplayMetrics.PrimaryDisplayHeight > 1)
	{
		int32 DesiredViewSizeX = FMath::Min(ViewSizeX, DisplayMetrics.PrimaryDisplayWidth / 2);
		int32 DesiredViewSizeY = FMath::Min(ViewSizeY, DisplayMetrics.PrimaryDisplayHeight / 2);
		if (DesiredViewSizeX != ViewSizeX || DesiredViewSizeY != ViewSizeY)
		{
			// It must be windowed to see some other desktop area.
			FSystemResolution::RequestResolutionChange(DesiredViewSizeX, DesiredViewSizeY, EWindowMode::Windowed);
		}
		
		// In fact, it would be the best if there's a way to move the window to the second display if exists.
		// But here I would like to get the current window position first.. like what you get from GetClientRect?
		if (DisplayMetrics.MonitorInfo.Num() >= 2
			&& LocalViewport != nullptr)
		{
			const FMonitorInfo& SecondMonitorInfo = DisplayMetrics.MonitorInfo[1];
			// Anyway, it doesn't work.
			LocalViewport->MoveWindow(SecondMonitorInfo.WorkArea.Left, SecondMonitorInfo.WorkArea.Top, DesiredViewSizeX, DesiredViewSizeY);
		}
	}
}