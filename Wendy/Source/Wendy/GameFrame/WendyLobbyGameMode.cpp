// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyLobbyGameMode.h"
#include "GameFramework/SpectatorPawn.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyLobbyGameState.h"
#include "WendyLobbyPlayerController.h"
#include "WendyLobbyUI.h"

AWendyLobbyGameMode::AWendyLobbyGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = AWendyLobbyPlayerController::StaticClass();
	GameStateClass = AWendyLobbyGameState::StaticClass();
}

void AWendyLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	//CreateLobbyUIWidget();
}

void AWendyLobbyGameMode::CreateLobbyUIWidget()
{
	UWorld* OwnerWorld = GetWorld();
	if (LobbyUIClass != nullptr && IsValid(OwnerWorld))
	{
		LobbyUIWidget = CreateWidget<UWendyLobbyUI>(OwnerWorld, LobbyUIClass);
		if (LobbyUIWidget != nullptr)
		{
			LobbyUIWidget->AddToViewport(WD_MAIN_UI_ZORDER);
		}
	}
}