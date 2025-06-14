// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDungeonPlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "WendyDataStore.h"
#include "WendyDesktopImageComponent.h"
#include "WendyDungeonSeat.h"
#include "WendyGameInstance.h"
#include "WendyUIDungeonSeatSelection.h"
#if PLATFORM_WINDOWS
#include <Windows.h>
#endif

static TAutoConsoleVariable<float> CVarWdDungeonSeatPickingDist(
	TEXT("wd.DungeonSeatPickingDist"),
	1500.0f,
	TEXT("A seat beyond this distance from view won't be picked."),
	ECVF_Default);

AWendyDungeonPlayerController::AWendyDungeonPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AWendyDungeonPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Not setting the input mode at here BeginPlay
	// WendyDungeonGameMode(or state) has its own plan for this.
	//SetInputMode(FInputModeGameAndUI());

}

void AWendyDungeonPlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);

	AWendyCharacter* AsWendyChar = Cast<AWendyCharacter>(aPawn);

	if (IsLocalController() && AsWendyChar != nullptr)
	{
		// Assumes that local DungeonPlayerController is created after logging in from lobby UI and entering the wendy world.
		// This is the first step that AccountInfo goes for replication.. to the server (RPC) and to other client (as remote)
		FWendyDataStore& WendyDataStore = GetGlobalWendyDataStore();
		AsWendyChar->SetConnectedUserAccountInfo(WendyDataStore.GetUserAccountInfo());
	}
}

void AWendyDungeonPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsInFocusingMode())
	{
		UpdateFocusingDisplayHitUV();
	}
	else
	{
		// It doesn't have to be like every tick, so if performance matters consider calling seat picking with some interval.
		UpdateDungeonSeatPicking();
	}


	// Pick up input event from InputKey here
	if (IsInFocusingMode())
	{
		if (InputKeyInputEvent == EWendyRemoteInputEvents::Pressed || InputKeyInputEvent == EWendyRemoteInputEvents::Released)
		{
			FocusingModeMonitorHitInputInfo.InputEvent = InputKeyInputEvent;
			FocusingModeMonitorHitInputInfo.InputKey = InputKeyInputKey;
		}
		else
		{
			// Not sending else.. probably yet?
			FocusingModeMonitorHitInputInfo.InputKey = EWendyRemoteInputKeys::None;
			FocusingModeMonitorHitInputInfo.InputEvent = EWendyRemoteInputEvents::None;
		}
		
		// As FocusingModeMonitorHitInputInfo.InputEvent will last until next PlayerTick this can be at any other place,
		// but why not here?
		UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
		if (IsValid(WdGameInst))
		{
			//if (FocusingModeMonitorHitInputInfo.HasValidInfo()) <- Should better?
			{
				WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
			}
		}
	}
	else
	{ 
		// Reseting none here is not probably needed, but it is like double check.
		FocusingModeMonitorHitInputInfo.InputKey = EWendyRemoteInputKeys::None;
		FocusingModeMonitorHitInputInfo.InputEvent = EWendyRemoteInputEvents::None;
	}

	// Should reset after copy the value as InputKey is not being called only when there is an input.
	InputKeyInputKey = EWendyRemoteInputKeys::None;
	InputKeyInputEvent = EWendyRemoteInputEvents::None;


	// What if SimulateRemoteInput invokes InputKey? It can try send input that has been made remote and endless cycle. Better prevent conflict..
	SimulateRemoteInput();
}

bool AWendyDungeonPlayerController::InputKey(const FInputKeyParams& Params)
{
	// Might refer to ClickEventKeys and UPrimitiveComponent::DispatchOnClicked
	// but I want to put things here.

	if (Params.Event == EInputEvent::IE_Pressed)
	{
		InputKeyInputEvent = EWendyRemoteInputEvents::Pressed;
	}
	else if (Params.Event == EInputEvent::IE_Released)
	{
		InputKeyInputEvent = EWendyRemoteInputEvents::Released;
	}

	if (Params.Key == EKeys::LeftMouseButton)
	{
		InputKeyInputKey = EWendyRemoteInputKeys::MLB;

		// Too much misclick if entering focus mode on mouse left button.
		/*if (Params.Event == EInputEvent::IE_Released)
		{
			TryEnterFocusMode();
		}*/
	}
	else if (Params.Key == EKeys::RightMouseButton)
	{
		InputKeyInputKey = EWendyRemoteInputKeys::MRB;
	}
	else
	{
		const EWendyRemoteInputKeys FKeyConverted = FromFKeyToWendyRemoteKey(Params.Key);
		if (FKeyConverted != EWendyRemoteInputKeys::None)
		{
			InputKeyInputKey = FKeyConverted;
		}

		if (Params.Key == EKeys::F && Params.Event == EInputEvent::IE_Released)
		{
			TryEnterFocusMode();
		}
	}

	IConsoleVariable* StopProcessingWmSysCommandCVarPtr = IConsoleManager::Get().FindConsoleVariable(TEXT("w.StopProcessingWmSysCommand"));
	if (StopProcessingWmSysCommandCVarPtr != nullptr)
	{ // It is being set to zero at released signal in SimulateRemoteInput, but sometimes it is missing, so give some extra chance to reset.
		StopProcessingWmSysCommandCVarPtr->SetWithCurrentPriority(0);
	}

	return Super::InputKey(Params);
}

void AWendyDungeonPlayerController::SetInputModeExploring()
{
	// Isn't it replicated..?
	SetInputMode(FInputModeGameAndUI());
	SetShowMouseCursor(false);

	OnExploringInputModeEvent.Broadcast();
}

void AWendyDungeonPlayerController::SetInputModeUIFocusing(bool bEnableGameInputToo)
{
	if (bEnableGameInputToo)
	{
		SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		SetInputMode(FInputModeUIOnly());
	}
	SetShowMouseCursor(true);

	OnUIFocusingInputModeEvent.Broadcast();
}

void AWendyDungeonPlayerController::SetInputModeForDesktopFocusMode()
{
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// For desktop focus mode, we need a bit special handling like mostly need the characteristic of UIOnly mode,
	// but still need to get input down to here InputKey for remote control.

	UGameViewportClient* GameViewportClient = GetWorld()->GetGameViewport();
	if (IsValid(GameViewportClient))
	{
		GameViewportClient->SetIgnoreInput(false);

		// Couldn't find a way by trying VC's mouse ~~ mode, so we changed engine code a bit at FSceneViewport::OnMouseButtonDown
		// to remove several condition for calling ViewportClient->InputKey.
		//GameViewportClient->SetMouseCaptureMode();
		//GameViewportClient->SetMouseLockMode();
	}

	OnDesktopFocusingInputModeEvent.Broadcast();
}

void AWendyDungeonPlayerController::SimulateRemoteInput()
{
	UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (IsValid(WdGameInst))
	{
		TArray<FWendyMonitorHitAndInputInfo> RemoteInputInfo;
		WdGameInst->ConsumeRemoteInputInfo(RemoteInputInfo);
#if PLATFORM_WINDOWS
		int32 PrimDisplayWidth = 0;
		int32 PrimDisplayHeight = 0;
		// Here it has to be logical resolution.
		UWendyDesktopImageComponent::GetPrimaryMonitorResolution(PrimDisplayWidth, PrimDisplayHeight, false);

		for (const FWendyMonitorHitAndInputInfo& InputInfo : RemoteInputInfo)
		{
			if (InputInfo.HasValidInfo())
			{
				const FIntVector2 CursorPos(
					static_cast<int32>(static_cast<float>(PrimDisplayWidth) * InputInfo.MonitorHitUV.X),
					static_cast<int32>(static_cast<float>(PrimDisplayHeight) * InputInfo.MonitorHitUV.Y)
				);
				
				::SetCursorPos(CursorPos.X, CursorPos.Y);

				if (InputInfo.InputKey == EWendyRemoteInputKeys::MLB || InputInfo.InputKey == EWendyRemoteInputKeys::MRB)
				{
					IConsoleVariable* StopProcessingWmSysCommandCVarPtr = IConsoleManager::Get().FindConsoleVariable(TEXT("w.StopProcessingWmSysCommand"));
					
					const bool bConsideredMLB = (InputInfo.InputKey == EWendyRemoteInputKeys::MLB);

					if (InputInfo.InputEvent == EWendyRemoteInputEvents::Pressed)
					{
						// If hold remote Wendy title bar by remote input, there's no way escaping unless remote side do something,
						// so we just prevent such thing.
						if (StopProcessingWmSysCommandCVarPtr != nullptr)
						{
							StopProcessingWmSysCommandCVarPtr->SetWithCurrentPriority(1);
						}

						INPUT input = {};

						input.type = INPUT_MOUSE;
						input.mi.dwFlags = bConsideredMLB  ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
												
						// If it doesn't work well for clicking event, we can send 2 inputs at the same time to simulate clicking event precisely.
						::SendInput(1, &input, sizeof(INPUT));

					}
					// Simulating click by released itself, which make things more stable..?.
					else if (InputInfo.InputEvent == EWendyRemoteInputEvents::Released)
					{
						if (StopProcessingWmSysCommandCVarPtr != nullptr)
						{
							StopProcessingWmSysCommandCVarPtr->SetWithCurrentPriority(0);
						}

						INPUT inputs[2] = {};

						inputs[0].type = INPUT_MOUSE;
						inputs[0].mi.dwFlags = bConsideredMLB ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;

						inputs[1].type = INPUT_MOUSE;
						inputs[1].mi.dwFlags = bConsideredMLB ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;

						// If it doesn't work well for clicking event, we can send 2 inputs at the same time to simulate clicking event precisely.
						::SendInput(2, inputs, sizeof(INPUT));
					}
				}
				else // Keyborad input
				{
					// Like mouse input, simulating press and release altogether for stability
					if (InputInfo.InputEvent == EWendyRemoteInputEvents::Released)
					{
						const uint8 VKConverted = FromWendyRemoteKeyToWinVK(InputInfo.InputKey);
						if (VKConverted != 0)
						{
							INPUT inputs[2] = {};

							// Key down
							inputs[0].type = INPUT_KEYBOARD;
							inputs[0].ki.wVk = VKConverted;
							// Key up
							inputs[1].type = INPUT_KEYBOARD;
							inputs[1].ki.wVk = VKConverted;
							inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

							// Send both inputs (press and release)
							::SendInput(2, inputs, sizeof(INPUT));
						}
					}
				}
			}
		}
#endif
	}
}

void AWendyDungeonPlayerController::UpdateDungeonSeatPicking()
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		for (FActorIterator ItActor(World); ItActor; ++ItActor)
		{
			AWendyDungeonSeat* AsDungeonSeat = Cast<AWendyDungeonSeat>(*ItActor);
			if (IsValid(AsDungeonSeat))
			{
				AsDungeonSeat->SetFocusHovered(false);
			}
		}

		FVector PlayerViewLoc(FVector::ZeroVector);
		FVector PlayerViewDir(FVector::ZeroVector);
		{
			FRotator PlayerViewRot(FRotator::ZeroRotator);
			GetPlayerViewPoint(PlayerViewLoc, PlayerViewRot);
			PlayerViewDir = PlayerViewRot.Vector();
		}

		if (PlayerViewDir.IsNearlyZero() == false)
		{
			TArray<FHitResult> HitResults;

			// There could be characters blocking the way, so trace multi and pick the most closest seat..
			// but that is not enough so put myself ignored.

			AWendyCharacter* LocalWdChar = UWdGameplayStatics::GetLocalPlayerCharacter(this);
			FCollisionQueryParams CollisionQueryParams;
			if (IsValid(LocalWdChar))
			{
				CollisionQueryParams.AddIgnoredActor(LocalWdChar);
			}

			if (World->LineTraceMultiByChannel(HitResults, 
					PlayerViewLoc, 
					PlayerViewLoc + PlayerViewDir * CVarWdDungeonSeatPickingDist.GetValueOnGameThread(), 
					ECC_WorldStatic,
					CollisionQueryParams))
			{
				AWendyDungeonSeat* ClosestSeat = nullptr;
				float ClosestSeatDist = FLT_MAX;
				for (const FHitResult& HitRes : HitResults)
				{
					AWendyDungeonSeat* AsDungeonSeat = Cast<AWendyDungeonSeat>(HitRes.GetActor());
					if (IsValid(AsDungeonSeat))
					{
						const float ThisSeatDist = (AsDungeonSeat->GetActorLocation() - PlayerViewLoc).Length();
						if (ThisSeatDist < ClosestSeatDist)
						{
							ClosestSeatDist = ThisSeatDist;
							ClosestSeat = AsDungeonSeat;
						}
					}
				}
				
				if (IsValid(ClosestSeat))
				{
					ClosestSeat->SetFocusHovered(true);
				}
			}
		}
	}
}

void AWendyDungeonPlayerController::UpdateFocusingDisplayHitUV()
{
	FHitResult UnderCursorHitRes;

	bool bSetHitUV = false;

	// Instead of directly using GetHitResultUnderCursor, brought GetHitResultUnderCursor body here, to use it with a bit different query param.
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	if (IsValid(LocalPlayer) && IsValid(LocalPlayer->ViewportClient))
	{
		FVector2D MousePosition;
		if (LocalPlayer->ViewportClient->GetMousePosition(MousePosition))
		{
			FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(ClickableTrace), true);
			CollisionQueryParams.bReturnFaceIndex = true; //<- Needed for HitUV result.
			if (GetHitResultAtScreenPosition(MousePosition, ECC_WorldStatic, CollisionQueryParams, UnderCursorHitRes))
			{
				AWendyDungeonSeat* HitActorAsDungeonSeat = Cast<AWendyDungeonSeat>(UnderCursorHitRes.GetActor());
				UPrimitiveComponent* HitComp = UnderCursorHitRes.GetComponent();

				if (IsValid(HitActorAsDungeonSeat) && IsValid(HitComp) && HitComp == HitActorAsDungeonSeat->GetMonitorMeshComp())
				{
					FVector2D HitUV(FVector2D::ZeroVector);
					// Need bSupportUVFromHitResults being true for this.
					if (UGameplayStatics::FindCollisionUV(UnderCursorHitRes, 0, HitUV))
					{
						//UE_LOG(LogWendy, Display, TEXT("Hit under cursor monitor %.2f %.2f"), HitUv.X, HitUv.Y);

						FocusingModeMonitorHitInputInfo.TargetUserId = HitActorAsDungeonSeat->GetOwnerCharacterId();
						FocusingModeMonitorHitInputInfo.MonitorHitUV = HitUV;
						bSetHitUV = true;
					}
				}
			}
		}
	}

	if (false == bSetHitUV)
	{
		FocusingModeMonitorHitInputInfo.SetInvalid();
	}
}

void AWendyDungeonPlayerController::TryEnterFocusMode()
{
	// If it was already in focus mode, there's no focus hovered so cannot get in focus mode again, 
	// so not leaving, just not doing anything in such case.
	/*if (bInFocusingMode)
	{
		UE_LOG(LogWendy, Warning, TEXT("Unexpected re-entrance to EnterFocusMode while in focusing mode already"));
		LeaveFocusMode();
		ensureMsgf(GetCurrFocusingSeat() == nullptr, TEXT("Is there any focusing seat after leaving? %s"), *GetCurrFocusingSeat()->GetName());
	}*/

	UWorld* World = GetWorld();
	if (IsValid(World) && false == bInFocusingMode)
	{
		for (FActorIterator ItActor(World); ItActor; ++ItActor)
		{
			AWendyDungeonSeat* AsDungeonSeat = Cast<AWendyDungeonSeat>(*ItActor);
			// It will succeed only when there is a seat currently focus hovered.
			if (IsValid(AsDungeonSeat) && AsDungeonSeat->IsFocusHovered())
			{
				bInFocusingMode = true;
				AsDungeonSeat->SetFocusHovered(false);
				AsDungeonSeat->SetFocused(true);
				SetInputModeForDesktopFocusMode();

				break; // Only one can be focused, no need to see more.
			}
		}
	}
}

void AWendyDungeonPlayerController::LeaveFocusMode()
{
	// Is it better to iterate all and call SetFocused(false)? not probably because SetFocused call invokes view target change.
	AWendyDungeonSeat* FocusingSeat = GetCurrFocusingSeat();
	if (IsValid(FocusingSeat))
	{
		FocusingSeat->SetFocused(false);
	}
	SetInputModeExploring();
	bInFocusingMode = false;
}

void AWendyDungeonPlayerController::ConditionalLeaveFocusMode()
{
	if (bInFocusingMode)
	{
		LeaveFocusMode();
	}
}

AWendyDungeonSeat* AWendyDungeonPlayerController::GetCurrFocusingSeat() const
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		for (FActorIterator ItActor(World); ItActor; ++ItActor)
		{
			AWendyDungeonSeat* AsDungeonSeat = Cast<AWendyDungeonSeat>(*ItActor);
			if (IsValid(AsDungeonSeat) && AsDungeonSeat->IsFocused())
			{
				return AsDungeonSeat;
			}
		}
	}
	return nullptr;
}

void AWendyDungeonPlayerController::OnDungeonSeatFocusHovered(AWendyDungeonSeat* TargetSeat, bool bFocusHovered)
{
	if (IsValid(TargetSeat))
	{
		if (bFocusHovered)
		{
			FocusHoveredSeat = TargetSeat;
		}
		else
		{
			if (FocusHoveredSeat.IsValid() && FocusHoveredSeat.Get() == TargetSeat)
			{
				FocusHoveredSeat = nullptr;
			}
		}
	}
}