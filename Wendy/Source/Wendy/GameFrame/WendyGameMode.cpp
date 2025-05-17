// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyGameMode.h"
#include "GameFramework/SpectatorPawn.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "WendyGameState.h"
#include "WendyPlayerController.h"
#include "UObject/ConstructorHelpers.h"

AWendyGameMode::AWendyGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// set default pawn class to our Blueprinted character
	// Once was like below, but not much reason to have hard coded resourct path when we have blueprint extension.
	/*static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}*/

	DefaultPawnClass = ASpectatorPawn::StaticClass();

	PlayerControllerClass = AWendyPlayerController::StaticClass();

	GameStateClass = AWendyGameState::StaticClass();
}

void AWendyGameMode::BeginPlay()
{
	Super::BeginPlay();

}

void AWendyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	Super::EndPlay(EndPlayReason);
}
