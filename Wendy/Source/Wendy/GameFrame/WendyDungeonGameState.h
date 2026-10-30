// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyGameState.h"
#include "WendyDungeonGameState.generated.h"

class UWendyUIDungeonSeatSelection;
class UWendyHudUI;
class AWendyCharacter;

/** Some flow state for dungeon game.. Not really need to be replicated? */
UENUM()
enum class EWendyDungeonPhase : uint8
{
	WDP_SeatSelection,
	WDP_Working,

	WDP_End
};

/**
 * The main playing game state for Wendy.
 * Working in a Dungeon!
 * GameState exists both server and client,
 * we put some many here because pretty much are in client 
 */
UCLASS()
class AWendyDungeonGameState : public AWendyGameState
{
	GENERATED_BODY()

protected:
	/**
	 * @TODO Wendy UIManagement: Having UI widget reference in transient actor class is not perhaps a good idea for more complex application,
	 *		but here for quick development as well as not worrying about GC management.
	 */
	UPROPERTY(EditAnywhere, Category="WendyDungeonGameState")
	TSubclassOf<UWendyUIDungeonSeatSelection> SeatSelectionUIClass;

	/** It should be valid for a certain while only, when the user just entered. */
	UPROPERTY(Transient)
	UWendyUIDungeonSeatSelection* SeatSelectionUIWidget;

	UPROPERTY(EditAnywhere, Category = "WendyDungeonGameState")
	TSubclassOf<UWendyHudUI> HudUIClass;

	UPROPERTY(Transient)
	UWendyHudUI* HudUIWidget;

	/** Somewhat like MatchState of GameMode..? */
	UPROPERTY(Transient)
	EWendyDungeonPhase CurrentPhase;

	/** Being registered when a WendyCharacter spawned in a world.
	 * I don't mean to this being a GC holder, so declaring it as TWeakObjectPtr */
	TArray<TWeakObjectPtr<AWendyCharacter>> AllWdCharacterInWorld;

public:
	AWendyDungeonGameState(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	void AddWdCharacter(AWendyCharacter* InNewCharacter);
	void RemoveWdCharacter(AWendyCharacter* InNewCharacter);
	/** It might not be the simple copy of AllWdCharacterInWorld. Invalid (staled) object won't be included */
	void GetAllValidWdCharacterInWorld(TArray<AWendyCharacter*>& OutResult) const;

protected:
	void CreateSeatSelectionUI();
	/** SeatSelectionUI is not being ermanently shown */
	void DestroySeatSelectionUI();

	/** As the first phase, specially provide its own entering method, and let other phases use AdvancePhase? */
	void GoToSeatSelection();

	void CreateHudUIWidget();

public:
	/** It should include some specific handling for each phase transition. */
	void AdvancePhase();

	FORCEINLINE EWendyDungeonPhase GetCurrentPhase() const { return CurrentPhase; }
};

