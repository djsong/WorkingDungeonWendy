// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDungeonGameState.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "WendyDungeonPlayerController.h"
#include "WendyUIDungeonSeatSelection.h"

AWendyDungeonGameState::AWendyDungeonGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SeatSelectionUIClass = nullptr;
	SeatSelectionUIWidget = nullptr;
	CurrentPhase = EWendyDungeonPhase::WDP_SeatSelection;
}

void AWendyDungeonGameState::BeginPlay()
{
	Super::BeginPlay();

	// Begin from SeatSelection.
	GoToSeatSelection();
}

void AWendyDungeonGameState::CreateSeatSelectionUI()
{
	UWorld* OwnerWorld = GetWorld();
	if (SeatSelectionUIClass != nullptr && IsValid(OwnerWorld))
	{
		SeatSelectionUIWidget = CreateWidget<UWendyUIDungeonSeatSelection>(OwnerWorld, SeatSelectionUIClass);
		if (SeatSelectionUIWidget != nullptr)
		{
			SeatSelectionUIWidget->AddToViewport(WD_MAIN_UI_ZORDER);
		}
	}
}

void AWendyDungeonGameState::DestroySeatSelectionUI()
{
	// @TODO Wendy UIManagement: When there is some unified UI management, there would other kind of UI destruction handling
	//		This is too specific.

	if (SeatSelectionUIWidget != nullptr)
	{
		SeatSelectionUIWidget->RemoveFromViewport();
		SeatSelectionUIWidget->ConditionalBeginDestroy();
		SeatSelectionUIWidget = nullptr;
	}
}

void AWendyDungeonGameState::GoToSeatSelection()
{
	CurrentPhase = EWendyDungeonPhase::WDP_SeatSelection;

	CreateSeatSelectionUI();

	// For seat selection, need to concentrate on UI.
	APlayerController* LocalPC = UWdGameplayStatics::GetLocalPlayerController(this);
	if (LocalPC != nullptr)
	{
		// Isn't it replicated..?
		LocalPC->SetInputMode(FInputModeUIOnly());
		LocalPC->SetShowMouseCursor(true);
	}
}

void AWendyDungeonGameState::AdvancePhase()
{
	switch (CurrentPhase)
	{
	case EWendyDungeonPhase::WDP_SeatSelection:

		// Other handlings regarding seat selecation should be done at this point.
		DestroySeatSelectionUI();

		CurrentPhase = EWendyDungeonPhase::WDP_Working;

		// Input mode for the main working phase. Now going to 3D world, but guess need UI too.
		APlayerController* LocalPC = UWdGameplayStatics::GetLocalPlayerController(this);
		if (LocalPC != nullptr)
		{
			LocalPC->SetInputMode(FInputModeGameAndUI());
			LocalPC->SetShowMouseCursor(false);
		}

		// If there's any UI for the next phase..

		break;


		// Whatever if added.
	}
}