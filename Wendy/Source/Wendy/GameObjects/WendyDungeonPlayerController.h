// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"
#include "WendyPlayerController.h"
#include "WendyDungeonPlayerController.generated.h"

class AWendyDungeonSeat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExploringInputModeEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIFocusingInputModeEvent);

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
	virtual bool InputKey(const FInputKeyParams& Params) override;

	/** Simple helper for input mode setting */
	void SetInputModeExploring();
	void SetInputModeUIFocusing(bool bEnableGameInputToo);

	FOnExploringInputModeEvent OnExploringInputModeEvent;
	FOnUIFocusingInputModeEvent OnUIFocusingInputModeEvent;
private:

	void SimulateRemoteInput();

	/** For non focusing mode picking */
	void UpdateDungeonSeatPicking();
	/** For focusing mode input faking */
	void UpdateFocusingDisplayHitUV();

	/** To be valid only when monitor mesh is under mouse cursor in focusing mode. */
	FWendyMonitorHitAndInputInfo FocusingModeMonitorHitInputInfo;
	/** FWendyMonitorHitAndInputInfo also has this member, but extra variable is needed for tracking between frame to frame. */
	EWendyRemoteInputKeys InputKeyInputKey = EWendyRemoteInputKeys::None;
	EWendyRemoteInputEvents InputKeyInputEvent = EWendyRemoteInputEvents::None;

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
