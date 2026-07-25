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

static TAutoConsoleVariable<int32> CVarWdRemoteInputRelativeDragMode(
	TEXT("wd.RemoteInput.RelativeDragMode"),
	1,
	TEXT("Once a held mouse button moves past wd.RemoteInput.DragThresholdPixels in focus mode, send raw movement deltas ")
	TEXT("instead of absolute cursor positions. This is the only way to navigate capture-based UIs (e.g. an Unreal editor ")
	TEXT("viewport doing fly/orbit navigation). Clicks and sub-threshold movement stay on the absolute path and the cursor ")
	TEXT("stays visible throughout, so ordinary pointing and window dragging are unaffected. Set 0 to force absolute-only."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdRemoteInputDragThresholdPixels(
	TEXT("wd.RemoteInput.DragThresholdPixels"),
	5.0f,
	TEXT("How far (in local viewport pixels) the cursor must move while a mouse button is held before the hold is treated as a drag and relative mode kicks in. Keeps a click a click. Only used when wd.RemoteInput.RelativeDragMode is 1."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdRemoteInputDragEdgeMarginPixels(
	TEXT("wd.RemoteInput.DragEdgeMarginPixels"),
	100.0f,
	TEXT("During a relative drag the cursor is left visible and free, and is only warped back to the middle of the viewport once it comes within this many pixels of an edge, so a long drag doesn't run out of room. 0 disables the warp entirely."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdRemoteInputRelativeMouseSensitivity(
	TEXT("wd.RemoteInput.RelativeMouseSensitivity"),
	1.0f,
	TEXT("Scale applied to relative mouse deltas sent while dragging in focus mode. 1.0 maps one local viewport pixel to one remote pixel."),
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
		// A held button only becomes a drag once it has actually moved far enough; until then everything
		// stays on the absolute path so plain clicks behave normally.
		UpdateRelativeMouseModeTransition();

		if (bInRelativeMouseMode)
		{
			// Mid-drag: deliberately skip the hit-UV update so MonitorHitUV stays frozen at where the drag
			// began. The remote cursor is being moved by deltas now, and a frozen-but-valid UV also keeps
			// HasValidInfo() true so these infos still pass the send/apply gates.
			UpdateRelativeMouseDelta();
		}
		else
		{
			UpdateFocusingDisplayHitUV();
		}
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
			for (const FWendyRemoteInputKeyEvent& KeyEvent : PendingRemoteInputKeyEvents)
			{
				FocusingModeMonitorHitInputInfo.InputKey = KeyEvent.Key;
				FocusingModeMonitorHitInputInfo.InputEvent = KeyEvent.Event;
				// Events captured mid-drag (notably the button release ending it) must not reposition the
				// remote cursor, or the drag would snap back to where it started right before releasing.
				FocusingModeMonitorHitInputInfo.bRelativeMouseMove = KeyEvent.bRelativeMouse;
				FocusingModeMonitorHitInputInfo.MouseDelta = FVector2D::ZeroVector;
				WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
			}

			FocusingModeMonitorHitInputInfo.InputKey = EWendyRemoteInputKeys::None;
			FocusingModeMonitorHitInputInfo.InputEvent = EWendyRemoteInputEvents::None;

			if (bInRelativeMouseMode)
			{
				// Dragging: send this tick's raw movement (unthrottled) and no absolute position at all.
				if (!PendingRelativeMouseDelta.IsNearlyZero())
				{
					FocusingModeMonitorHitInputInfo.bRelativeMouseMove = true;
					FocusingModeMonitorHitInputInfo.MouseDelta = PendingRelativeMouseDelta * CVarWdRemoteInputRelativeMouseSensitivity.GetValueOnGameThread();
					WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
				}
			}
			else if (PendingRemoteInputKeyEvents.Num() == 0)
			{
				// Idle: still send a "no key" info so the remote cursor keeps following the hover.
				FocusingModeMonitorHitInputInfo.bRelativeMouseMove = false;
				FocusingModeMonitorHitInputInfo.MouseDelta = FVector2D::ZeroVector;
				WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
			}

			// Don't leave relative state on the persistent member for the next tick / other senders.
			FocusingModeMonitorHitInputInfo.bRelativeMouseMove = false;
			FocusingModeMonitorHitInputInfo.MouseDelta = FVector2D::ZeroVector;
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
	PendingRelativeMouseDelta = FVector2D::ZeroVector;


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
		// Stamp the CURRENT relative state, before the transition below. That gives exactly the right
		// behaviour at both ends of a drag: the opening button press is still absolute (it positions the
		// remote cursor precisely where the user clicked), while the closing release is relative
		// (no repositioning, so the drag ends wherever it actually ended).
		PendingRemoteInputKeyEvents.Add(FWendyRemoteInputKeyEvent{ CapturedKey, CapturedEvent, bInRelativeMouseMode });

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

	// Relative-mouse (drag) mode is deliberately NOT entered here on button-down: a plain click has to stay a
	// click. Promotion to a drag waits until the cursor actually moves past a threshold, which is checked per
	// tick in UpdateRelativeMouseModeTransition. All we do here is note where a hold began, and end the mode
	// when the last button comes up.
	if (IsInFocusingMode())
	{
		if (HasAnyRemoteMouseButtonHeld())
		{
			const bool bIsMouseButtonKey = (CapturedKey == EWendyRemoteInputKeys::MLB
				|| CapturedKey == EWendyRemoteInputKeys::MRB
				|| CapturedKey == EWendyRemoteInputKeys::MMB);
			if (bIsMouseButtonKey && CapturedEvent == EWendyRemoteInputEvents::Pressed && false == bInRelativeMouseMode)
			{
				// Origin the drag threshold is measured from.
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
				if (IsValid(LocalPlayer) && IsValid(LocalPlayer->ViewportClient))
				{
					LocalPlayer->ViewportClient->GetMousePosition(MouseHeldStartPos);
				}
			}
		}
		else if (bInRelativeMouseMode)
		{
			ExitRelativeMouseMode();
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
				
				if (InputInfo.bRelativeMouseMove)
				{
					// Part of a drag: deliberately no SetCursorPos. Absolute placement here would yank the
					// drag back to where it began, and capture-based UIs (editor viewport fly/orbit nav)
					// ignore cursor position anyway — they read raw movement, which is what we inject.
					const int32 DeltaX = FMath::RoundToInt(InputInfo.MouseDelta.X);
					const int32 DeltaY = FMath::RoundToInt(InputInfo.MouseDelta.Y);
					if (DeltaX != 0 || DeltaY != 0)
					{
						INPUT input = {};
						input.type = INPUT_MOUSE;
						// MOUSEEVENTF_MOVE without MOUSEEVENTF_ABSOLUTE == relative motion.
						input.mi.dwFlags = MOUSEEVENTF_MOVE;
						input.mi.dx = DeltaX;
						input.mi.dy = DeltaY;
						::SendInput(1, &input, sizeof(INPUT));
					}
				}
				else
				{
					::SetCursorPos(CursorPos.X, CursorPos.Y);
				}

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

bool AWendyDungeonPlayerController::HasAnyRemoteMouseButtonHeld() const
{
	return HeldRemoteInputKeys.Contains(EWendyRemoteInputKeys::MLB)
		|| HeldRemoteInputKeys.Contains(EWendyRemoteInputKeys::MRB)
		|| HeldRemoteInputKeys.Contains(EWendyRemoteInputKeys::MMB);
}

void AWendyDungeonPlayerController::EnterRelativeMouseMode()
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	if (false == IsValid(LocalPlayer) || false == IsValid(LocalPlayer->ViewportClient))
	{
		return;
	}

	FVector2D ViewportSize(FVector2D::ZeroVector);
	LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);
	if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		return;
	}

	FVector2D MousePosition(FVector2D::ZeroVector);
	LocalPlayer->ViewportClient->GetMousePosition(MousePosition);

	// From here on movement is measured frame to frame against this.
	LastRelativeSamplePos = MousePosition;
	RelativeMouseAnchorPos = ViewportSize * 0.5f;

	bInRelativeMouseMode = true;
	PendingRelativeMouseDelta = FVector2D::ZeroVector;

	// The cursor is deliberately left visible and free to roam. The streamed desktop image only refreshes a
	// couple of times a second, so the local cursor is the user's only real-time feedback about where they
	// are pointing; hiding it and pinning it to the centre makes a drag impossible to steer.
}

void AWendyDungeonPlayerController::ExitRelativeMouseMode()
{
	bInRelativeMouseMode = false;
	PendingRelativeMouseDelta = FVector2D::ZeroVector;

	// Put the cursor back where the hold began, so the user finishes still over the monitor and ready to
	// carry on, rather than wherever the drag happened to wander off to.
	SetMouseLocation(FMath::RoundToInt(MouseHeldStartPos.X), FMath::RoundToInt(MouseHeldStartPos.Y));
}

void AWendyDungeonPlayerController::UpdateRelativeMouseModeTransition()
{
	const bool bModeEnabled = (CVarWdRemoteInputRelativeDragMode.GetValueOnGameThread() > 0);

	if (bInRelativeMouseMode)
	{
		// Turning the mode off mid-drag shouldn't strand the cursor hidden and warped to the anchor.
		if (false == bModeEnabled)
		{
			ExitRelativeMouseMode();
		}
		return;
	}

	if (false == bModeEnabled || false == HasAnyRemoteMouseButtonHeld())
	{
		return;
	}

	// Still absolute so far: a click, or a drag small enough that absolute positions handle it fine (which is
	// how ordinary window drags / marquee selects keep working). Only a genuine, larger movement promotes.
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	if (IsValid(LocalPlayer) && IsValid(LocalPlayer->ViewportClient))
	{
		FVector2D MousePosition(FVector2D::ZeroVector);
		if (LocalPlayer->ViewportClient->GetMousePosition(MousePosition))
		{
			const float ThresholdPixels = FMath::Max(CVarWdRemoteInputDragThresholdPixels.GetValueOnGameThread(), 0.0f);
			if ((MousePosition - MouseHeldStartPos).SizeSquared() >= static_cast<double>(ThresholdPixels) * ThresholdPixels)
			{
				EnterRelativeMouseMode();
			}
		}
	}
}

void AWendyDungeonPlayerController::UpdateRelativeMouseDelta()
{
	PendingRelativeMouseDelta = FVector2D::ZeroVector;

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	if (false == IsValid(LocalPlayer) || false == IsValid(LocalPlayer->ViewportClient))
	{
		return;
	}

	FVector2D MousePosition(FVector2D::ZeroVector);
	if (false == LocalPlayer->ViewportClient->GetMousePosition(MousePosition))
	{
		return;
	}

	// Movement since the previous sample. The cursor roams normally, so this stays 1:1 with the motion the
	// user can actually see themselves making.
	PendingRelativeMouseDelta = MousePosition - LastRelativeSamplePos;
	LastRelativeSamplePos = MousePosition;

	// Edge guard: only once the cursor is about to run out of viewport do we recentre it, so a long drag
	// isn't cut short. Re-baselining the sample means the warp itself contributes no movement.
	const float EdgeMargin = FMath::Max(CVarWdRemoteInputDragEdgeMarginPixels.GetValueOnGameThread(), 0.0f);
	if (EdgeMargin > 0.0f)
	{
		FVector2D ViewportSize(FVector2D::ZeroVector);
		LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);
		if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
		{
			const bool bNearEdge =
				MousePosition.X <= EdgeMargin || MousePosition.X >= (ViewportSize.X - EdgeMargin) ||
				MousePosition.Y <= EdgeMargin || MousePosition.Y >= (ViewportSize.Y - EdgeMargin);
			if (bNearEdge)
			{
				RelativeMouseAnchorPos = ViewportSize * 0.5f;
				SetMouseLocation(FMath::RoundToInt(RelativeMouseAnchorPos.X), FMath::RoundToInt(RelativeMouseAnchorPos.Y));
				LastRelativeSamplePos = RelativeMouseAnchorPos;
			}
		}
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
				// Input now belongs to the remote desktop, so stop driving our own character with it.
				// These are refcounted, hence paired strictly with the release in LeaveFocusMode.
				// (Covers WASD via AddMovementInput and mouse-look via AddControllerYaw/PitchInput; bindings
				// that don't consult these flags are gated in AWendyCharacter::IsLocalGameplayInputSuppressed.)
				SetIgnoreMoveInput(true);
				SetIgnoreLookInput(true);

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
				// Leaving mid-drag: keep suppressing absolute placement so the release lands where the
				// drag actually is, rather than snapping back to the frozen start UV first.
				FocusingModeMonitorHitInputInfo.bRelativeMouseMove = bInRelativeMouseMode;
				FocusingModeMonitorHitInputInfo.MouseDelta = FVector2D::ZeroVector;
				WdGameInst->SetRemoteInputInfo(FocusingModeMonitorHitInputInfo);
			}
			FocusingModeMonitorHitInputInfo.bRelativeMouseMove = false;
		}
		HeldRemoteInputKeys.Reset();
	}

	// A drag can be interrupted by leaving focus mode; restore the local cursor rather than leaving it hidden.
	if (bInRelativeMouseMode)
	{
		ExitRelativeMouseMode();
	}

	// Is it better to iterate all and call SetFocused(false)? not probably because SetFocused call invokes view target change.
	AWendyDungeonSeat* FocusingSeat = GetCurrFocusingSeat();
	if (IsValid(FocusingSeat))
	{
		FocusingSeat->SetFocused(false);
	}
	SetInputModeExploring();
	bInFocusingMode = false;

	// Hand input back to our own character. Paired with the pair taken in TryEnterFocusMode (refcounted).
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
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