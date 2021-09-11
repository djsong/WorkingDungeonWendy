// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyLobbyPlayerController.h"


AWendyLobbyPlayerController::AWendyLobbyPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowMouseCursor = true;
}

void AWendyLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Lobby is like static scene, UIs only
	SetInputMode(FInputModeUIOnly());
}