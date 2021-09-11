// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyGameState.h"
#include "WendyLobbyGameState.generated.h"

class UWendyLobbyUI;

/**
 * GameState exists both server and client so we need our own if some mode is in network.
 */
UCLASS()
class AWendyLobbyGameState : public AWendyGameState
{
	GENERATED_BODY()

protected:
	/**
	 * @TODO Wendy UIManagement: Having UI widget reference in transient actor class is not perhaps a good idea for more complex application,
	 *		but here for quick development as well as not worrying about GC management.
	 */
	UPROPERTY(EditAnywhere, Category="WendyLobbyGameState")
	TSubclassOf<UWendyLobbyUI> LobbyUIClass;

	UPROPERTY(Transient)
	UWendyLobbyUI* LobbyUIWidget;

public:
	AWendyLobbyGameState(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	void CreateLobbyUIWidget();
};



