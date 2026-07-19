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
