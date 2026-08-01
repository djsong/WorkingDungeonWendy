// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "WdGameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Widgets/SWindow.h"
#include "WendyCharacter.h"
#include "WendyDataStore.h"
#include "WendyGameSettings.h"
#include "WendyGameInstance.h"
#include "SocketSubsystem.h"

/** Defined in WendyDesktopImageComponent.cpp. */
extern FIntPoint GetWendyDesktopImageSize();

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

/** Index of the first monitor that isn't the primary one, or INDEX_NONE with a single display.
 * Deliberately keys off bIsPrimary rather than assuming the primary is MonitorInfo[0]. */
static int32 FindNonPrimaryMonitorIndex(const FDisplayMetrics& InDisplayMetrics)
{
	for (int32 MonitorIdx = 0; MonitorIdx < InDisplayMetrics.MonitorInfo.Num(); ++MonitorIdx)
	{
		if (false == InDisplayMetrics.MonitorInfo[MonitorIdx].bIsPrimary)
		{
			return MonitorIdx;
		}
	}
	return INDEX_NONE;
}

void TryClampViewForWholeInclusiveDesktopCapture(UObject* WorldContextObject)
{
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);

	int32 ViewSizeX = 0, ViewSizeY = 0;
	APlayerController* LocalPC = UWdGameplayStatics::GetLocalPlayerController(WorldContextObject);
	UGameViewportClient* LocalViewportClient = nullptr;
	if (LocalPC != nullptr)
	{
		LocalPC->GetViewportSize(ViewSizeX, ViewSizeY);

		if (ULocalPlayer* LocPlayer = Cast<ULocalPlayer>(LocalPC->Player))
		{
			LocalViewportClient = LocPlayer->ViewportClient;
		}
	}

	// #WendyDealsWithPrimaryDisplayOnly
	if (ViewSizeX <= 0 || ViewSizeY <= 0 ||
		DisplayMetrics.PrimaryDisplayWidth <= 1 || DisplayMetrics.PrimaryDisplayHeight <= 1)
	{
		return;
	}

	// We capture the primary display, so getting Wendy off it entirely is much better than merely shrinking it.
	const int32 TargetMonitorIdx = FindNonPrimaryMonitorIndex(DisplayMetrics);
	const bool bMoveToOtherMonitor = DisplayMetrics.MonitorInfo.IsValidIndex(TargetMonitorIdx);

	int32 DesiredViewSizeX = 0;
	int32 DesiredViewSizeY = 0;

	const int32 TOOLBAR_HEIGHT = 30; // Just approximate.

	if (bMoveToOtherMonitor)
	{
		const FMonitorInfo& TargetMonitorInfo = DisplayMetrics.MonitorInfo[TargetMonitorIdx];
		// Work area rather than the raw resolution, so the window doesn't end up under the taskbar.
		const int32 TargetMonitorWidth = TargetMonitorInfo.WorkArea.Right - TargetMonitorInfo.WorkArea.Left;
		const int32 TargetMonitorHeight = TargetMonitorInfo.WorkArea.Bottom - TargetMonitorInfo.WorkArea.Top;

		// Once we're off the captured display there's no reason to stay small. Two ceilings: what the monitor
		// can actually show, and a little over the internal desktop image size (beyond that we would only be
		// upscaling a fixed-size image, so the extra pixels buy nothing).
		const FIntPoint InternalImageSize = GetWendyDesktopImageSize();
		DesiredViewSizeX = FMath::Min(TargetMonitorWidth, FMath::RoundToInt(static_cast<float>(InternalImageSize.X) * 1.2f));
		DesiredViewSizeY = FMath::Min(TargetMonitorHeight - TOOLBAR_HEIGHT, FMath::RoundToInt(static_cast<float>(InternalImageSize.Y) * 1.2f));
	}
	else
	{
		// Single display: we have to share the screen we're capturing, so stay small enough to leave the
		// rest of the desktop visible.
		DesiredViewSizeX = FMath::Min(ViewSizeX, FMath::RoundToInt(static_cast<float>(DisplayMetrics.PrimaryDisplayWidth) / 1.5f));
		DesiredViewSizeY = FMath::Min(ViewSizeY, FMath::RoundToInt(static_cast<float>(DisplayMetrics.PrimaryDisplayHeight) / 1.5f));
	}

	if (DesiredViewSizeX > 0 && DesiredViewSizeY > 0 &&
		(DesiredViewSizeX != ViewSizeX || DesiredViewSizeY != ViewSizeY))
	{
		// It must be windowed to see some other desktop area.
		FSystemResolution::RequestResolutionChange(DesiredViewSizeX, DesiredViewSizeY, EWindowMode::Windowed);
	}

	if (bMoveToOtherMonitor)
	{
		// This used to go through FViewport::MoveWindow, which is an empty no-op on FSceneViewport - that is
		// why it never did anything. What actually moves is the Slate window underneath.
		// It also has to run AFTER the resolution request above, which the engine defers to a later tick and
		// which repositions the window itself, so do it on a short timer rather than inline.
		const FMonitorInfo& TargetMonitorInfo = DisplayMetrics.MonitorInfo[TargetMonitorIdx];
		const FVector2D TargetWindowPos(TargetMonitorInfo.WorkArea.Left, TargetMonitorInfo.WorkArea.Top + TOOLBAR_HEIGHT);
		const FVector2D TargetWindowSize(DesiredViewSizeX, DesiredViewSizeY);

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World != nullptr && LocalViewportClient != nullptr)
		{
			const float DeferMoveSeconds = 0.1f;
			TWeakObjectPtr<UGameViewportClient> WeakViewportClient(LocalViewportClient);

			FTimerHandle MoveToOtherMonitorTimerHandle;
			World->GetTimerManager().SetTimer(MoveToOtherMonitorTimerHandle, FTimerDelegate::CreateLambda(
				[WeakViewportClient, TargetWindowPos, TargetWindowSize]()
				{
					if (UGameViewportClient* ViewportClient = WeakViewportClient.Get())
					{
						TSharedPtr<SWindow> GameWindow = ViewportClient->GetWindow();
						if (GameWindow.IsValid())
						{
							GameWindow->ReshapeWindow(TargetWindowPos, TargetWindowSize);
						}
					}
				}), DeferMoveSeconds, false);
		}
	}
}