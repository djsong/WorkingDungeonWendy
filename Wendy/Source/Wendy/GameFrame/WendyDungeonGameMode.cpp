// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDungeonGameMode.h"
#include "GameFramework/SpectatorPawn.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "WendyDungeonGameState.h"
#include "WendyDungeonPlayerController.h"
#include "WendyUIDungeonSeatSelection.h"

AWendyDungeonGameMode::AWendyDungeonGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// set default pawn class to our Blueprinted character
	// Once was like below, but not much reason to have hard coded resourct path when we have blueprint extension.
	/*static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}*/

	PlayerControllerClass = AWendyDungeonPlayerController::StaticClass();
	GameStateClass = AWendyDungeonGameState::StaticClass();
}
