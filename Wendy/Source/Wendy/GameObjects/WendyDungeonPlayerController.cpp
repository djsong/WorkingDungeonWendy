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


	// Send the input captured this tick. Each queued key event goes out as its own remote-input info (in order),
	// so concurrent keys and true press/release survive. With no key this tick we still send a "no key" info so
	// the remote cursor keeps following the hover.
	if (IsInFocusingMode())
	{
		UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
		if (IsValid(WdGameInst))
		{
			if (PendingRemoteInputKeyEvents.Num() > 0)
			{
				for (const FWendyRemoteInputKeyEvent& KeyEvent : PendingRemoteInputKeyEvents)
				{
					FocusingModeMonitorHitInputInfo.InputKey = KeyEvent.Key;
					FocusingModeMonitorHitInputInfo.InputEvent = KeyEvent.Event;
					WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
				}
			}
			else
			{
				FocusingModeMonitorHitInputInfo.InputKey = EWendyRemoteInputKeys::None;
				FocusingModeMonitorHitInputInfo.InputEvent = EWendyRemoteInputEvents::None;
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

	// This tick's events have been sent; clear the queue for the next tick.
	PendingRemoteInputKeyEvents.Reset();


	// What if SimulateRemoteInput invokes InputKey? It can try send input that has been made remote and endless cycle. Better prevent conflict..
	SimulateRemoteInput();
}

bool AWendyDungeonPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Might refer to ClickEventKeys and UPrimitiveComponent::DispatchOnClicked
	// but I want to put things here.

	EWendyRemoteInputEvents CapturedEvent = EWendyRemoteInputEvents::None;
	if (Params.Event == EInputEvent::IE_Pressed)
	{
		CapturedEvent = EWendyRemoteInputEvents::Pressed;
	}
	else if (Params.Event == EInputEvent::IE_Released)
	{
		CapturedEvent = EWendyRemoteInputEvents::Released;
	}

	EWendyRemoteInputKeys CapturedKey = EWendyRemoteInputKeys::None;
	if (Params.Key == EKeys::LeftMouseButton)
	{
		CapturedKey = EWendyRemoteInputKeys::MLB;

		// Too much misclick if entering focus mode on mouse left button.
		/*if (Params.Event == EInputEvent::IE_Released)
		{
			TryEnterFocusMode();
		}*/
	}
	else if (Params.Key == EKeys::RightMouseButton)
	{
		CapturedKey = EWendyRemoteInputKeys::MRB;
	}
	else if (Params.Key == EKeys::MiddleMouseButton)
	{
		CapturedKey = EWendyRemoteInputKeys::MMB;
	}
	else if (Params.Key == EKeys::MouseScrollUp)
	{
		CapturedKey = EWendyRemoteInputKeys::MWheelUp;
	}
	else if (Params.Key == EKeys::MouseScrollDown)
	{
		CapturedKey = EWendyRemoteInputKeys::MWheelDown;
	}
	else
	{
		CapturedKey = FromFKeyToWendyRemoteKey(Params.Key);

		if (Params.Key == EKeys::F && Params.Event == EInputEvent::IE_Released)
		{
			TryEnterFocusMode();
		}
	}

	// Queue this event (in order) for PlayerTick to send. Unlike the old single-field model, multiple events per
	// tick are kept, so concurrent keys (e.g. modifier + key) and true press/release are both preserved.
	if (CapturedKey != EWendyRemoteInputKeys::None && CapturedEvent != EWendyRemoteInputEvents::None)
	{
		PendingRemoteInputKeyEvents.Add(FWendyRemoteInputKeyEvent{ CapturedKey, CapturedEvent });

		// Track held keys for the leave-focus safeguard. Wheel notches are momentary, so don't track them.
		if (CapturedKey != EWendyRemoteInputKeys::MWheelUp && CapturedKey != EWendyRemoteInputKeys::MWheelDown)
		{
			if (CapturedEvent == EWendyRemoteInputEvents::Pressed)
			{
				HeldRemoteInputKeys.AddUnique(CapturedKey);
			}
			else if (CapturedEvent == EWendyRemoteInputEvents::Released)
			{
				HeldRemoteInputKeys.Remove(CapturedKey);
			}
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

				if (InputInfo.InputKey == EWendyRemoteInputKeys::MLB || InputInfo.InputKey == EWendyRemoteInputKeys::MRB || InputInfo.InputKey == EWendyRemoteInputKeys::MMB)
				{
					IConsoleVariable* StopProcessingWmSysCommandCVarPtr = IConsoleManager::Get().FindConsoleVariable(TEXT("w.StopProcessingWmSysCommand"));
					
					// Pick the down/up flags for whichever mouse button this is.
					DWORD MouseDownFlag = MOUSEEVENTF_LEFTDOWN;
					DWORD MouseUpFlag = MOUSEEVENTF_LEFTUP;
					if (InputInfo.InputKey == EWendyRemoteInputKeys::MRB)
					{
						MouseDownFlag = MOUSEEVENTF_RIGHTDOWN;
						MouseUpFlag = MOUSEEVENTF_RIGHTUP;
					}
					else if (InputInfo.InputKey == EWendyRemoteInputKeys::MMB)
					{
						MouseDownFlag = MOUSEEVENTF_MIDDLEDOWN;
						MouseUpFlag = MOUSEEVENTF_MIDDLEUP;
					}

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
						input.mi.dwFlags = MouseDownFlag;
												
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

						// Release only (no fused down) so click-and-drag works: the button stays down between press and release.
						INPUT input = {};
						input.type = INPUT_MOUSE;
						input.mi.dwFlags = MouseUpFlag;
						::SendInput(1, &input, sizeof(INPUT));
					}
				}
				else if (InputInfo.InputKey == EWendyRemoteInputKeys::MWheelUp || InputInfo.InputKey == EWendyRemoteInputKeys::MWheelDown)
				{
					// Discrete wheel notch: emit one WHEEL event per captured scroll; direction comes from the key.
					INPUT input = {};
					input.type = INPUT_MOUSE;
					input.mi.dwFlags = MOUSEEVENTF_WHEEL;
					input.mi.mouseData = (InputInfo.InputKey == EWendyRemoteInputKeys::MWheelUp)
						? static_cast<DWORD>(WHEEL_DELTA)
						: static_cast<DWORD>(-WHEEL_DELTA);
					::SendInput(1, &input, sizeof(INPUT));
				}
				else // Keyborad input
				{
					// True press/release: down on Pressed, up on Released. This is what makes held modifiers,
					// key-repeat, and shortcuts (e.g. Ctrl+C) work instead of collapsing into a single tap.
					const uint8 VKConverted = FromWendyRemoteKeyToWinVK(InputInfo.InputKey);
					if (VKConverted != 0
						&& (InputInfo.InputEvent == EWendyRemoteInputEvents::Pressed || InputInfo.InputEvent == EWendyRemoteInputEvents::Released))
					{
						INPUT input = {};
						input.type = INPUT_KEYBOARD;
						input.ki.wVk = VKConverted;

						DWORD KeyFlags = 0;
						// Right Ctrl/Alt share their base scancode with the left ones; the extended bit marks them as right-side.
						if (InputInfo.InputKey == EWendyRemoteInputKeys::Key_RControl || InputInfo.InputKey == EWendyRemoteInputKeys::Key_RAlt)
						{
							KeyFlags |= KEYEVENTF_EXTENDEDKEY;
						}
						if (InputInfo.InputEvent == EWendyRemoteInputEvents::Released)
						{
							KeyFlags |= KEYEVENTF_KEYUP;
						}
						input.ki.dwFlags = KeyFlags;

						::SendInput(1, &input, sizeof(INPUT));
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
	// Release any keys still held down on the remote so a missed release can't leave them stuck (especially modifiers).
	// Do this while still in focus mode so FocusingModeMonitorHitInputInfo still carries the last valid hover UV
	// (the apply side gates on a valid UV). Then clear the held set.
	if (HeldRemoteInputKeys.Num() > 0)
	{
		UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
		if (IsValid(WdGameInst))
		{
			for (const EWendyRemoteInputKeys HeldKey : HeldRemoteInputKeys)
			{
				FocusingModeMonitorHitInputInfo.InputKey = HeldKey;
				FocusingModeMonitorHitInputInfo.InputEvent = EWendyRemoteInputEvents::Released;
				WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
			}
		}
		HeldRemoteInputKeys.Reset();
	}

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