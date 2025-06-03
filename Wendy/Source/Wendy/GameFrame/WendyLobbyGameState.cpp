// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyLobbyGameState.h"
#include "WendyCommon.h"
#include "WendyLobbyUI.h"

AWendyLobbyGameState::AWendyLobbyGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AWendyLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	CreateLobbyUIWidget();
}

void AWendyLobbyGameState::CreateLobbyUIWidget()
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