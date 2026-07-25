// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"
#include "WendyPlayerController.h"
#include "WendyDungeonPlayerController.generated.h"

class AWendyDungeonSeat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWDPCInputModeSwitchEvent);

/**
 * The main Wendy world playing controller.
 */
UCLASS(config = Game)
class AWendyDungeonPlayerController : public AWendyPlayerController
{
	GENERATED_BODY()

protected:

public:
	AWendyDungeonPlayerController(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* aPawn) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** Simple helper for input mode setting */
	void SetInputModeExploring();
	void SetInputModeUIFocusing(bool bEnableGameInputToo);
	void SetInputModeForDesktopFocusMode(); // A bit special..

	FOnWDPCInputModeSwitchEvent OnExploringInputModeEvent;
	FOnWDPCInputModeSwitchEvent OnUIFocusingInputModeEvent;
	FOnWDPCInputModeSwitchEvent OnDesktopFocusingInputModeEvent;
private:

	void SimulateRemoteInput();

	/** For non focusing mode picking */
	void UpdateDungeonSeatPicking();
	/** For focusing mode input faking */
	void UpdateFocusingDisplayHitUV();

	/** To be valid only when monitor mesh is under mouse cursor in focusing mode. */
	FWendyMonitorHitAndInputInfo FocusingModeMonitorHitInputInfo;
	/** All key/mouse events captured during the current tick, in order, each sent as its own remote-input info.
	 * Replaces the old single-key/tick field so concurrent keys (modifier + key) and true press/release work. */
	TArray<FWendyRemoteInputKeyEvent> PendingRemoteInputKeyEvents;
	/** Keys we've sent a Pressed for but not yet a Released. Used to auto-release them on leaving focus mode,
	 * so a missed release can't leave a key (especially a modifier) stuck DOWN on the remote machine.
	 * A small TArray (only a few keys are ever held at once) — avoids needing GetTypeHash for the enum. */
	TArray<EWendyRemoteInputKeys> HeldRemoteInputKeys;

	/** Relative-mouse (drag) mode, entered once a held mouse button moves past the drag threshold in focus
	 * mode. Absolute cursor positions stop being sent and raw movement deltas go instead, which is the only
	 * thing capture-based UIs (e.g. an Unreal editor viewport doing fly/orbit navigation) can navigate with.
	 * The cursor stays VISIBLE throughout: the streamed desktop refreshes only a couple of times a second,
	 * so the local cursor is the user's only real-time feedback and hiding it makes a drag unsteerable. */
	bool bInRelativeMouseMode = false;
	/** Viewport centre, used as the target of the edge-guard warp. */
	FVector2D RelativeMouseAnchorPos = FVector2D::ZeroVector;
	/** Previous tick's cursor position; deltas are measured frame to frame against it. */
	FVector2D LastRelativeSamplePos = FVector2D::ZeroVector;
	/** Where the current mouse-button hold started. Relative mode only kicks in once the cursor has moved
	 * further than the drag threshold from here, so a plain click never turns into a drag. */
	FVector2D MouseHeldStartPos = FVector2D::ZeroVector;
	/** Movement sampled this tick while dragging, consumed by PlayerTick. */
	FVector2D PendingRelativeMouseDelta = FVector2D::ZeroVector;

	void EnterRelativeMouseMode();
	void ExitRelativeMouseMode();
	/** Per-tick check that promotes a held mouse button to a real drag once it moves past the threshold. */
	void UpdateRelativeMouseModeTransition();
	/** Samples this tick's movement and warps the cursor back to the anchor. Relative mode only. */
	void UpdateRelativeMouseDelta();
	bool HasAnyRemoteMouseButtonHeld() const;

	/** Static cam that focuses on a selected seat in near distance */
	void TryEnterFocusMode();
	void LeaveFocusMode();
	bool bInFocusingMode = false;
	bool bHasAnySeatFocusHovered = false;

	/** Valid only when there is focus hovered */
	TWeakObjectPtr<AWendyDungeonSeat> FocusHoveredSeat = nullptr;

public:
	void ConditionalLeaveFocusMode();

	/** Returns null if not focusing at all. */
	AWendyDungeonSeat* GetCurrFocusingSeat() const;
	/** Being called both for set and unset. */
	void OnDungeonSeatFocusHovered(AWendyDungeonSeat* TargetSeat, bool bFocusHovered);

	bool IsInFocusingMode() const { return bInFocusingMode; }
	bool HasAnyFocusHoveredSeat() const { return FocusHoveredSeat.IsValid(); }

	bool HasValidFocusingMonitorHitInputInfo() const { return FocusingModeMonitorHitInputInfo.HasValidInfo(); }
	const FWendyMonitorHitAndInputInfo& GetFocusingMonitorHitInputInfo() const { return FocusingModeMonitorHitInputInfo; }
};
