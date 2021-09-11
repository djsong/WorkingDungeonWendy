// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyGameMode.h"
#include "WendyLobbyGameMode.generated.h"

class UWendyLobbyUI;

/**
 * Lobby game mode that manages login and server selection stuff.
 */
UCLASS()
class AWendyLobbyGameMode : public AWendyGameMode
{
	GENERATED_BODY()

protected:
	/**
	 * @TODO Wendy UIManagement: Having UI widget reference in transient actor class is not perhaps a good idea for more complex application,
	 *		but here for quick development as well as not worrying about GC management.
	 */
	UPROPERTY(EditAnywhere, Category="WendyLobbyGameMode")
	TSubclassOf<UWendyLobbyUI> LobbyUIClass;

	UPROPERTY(Transient)
	UWendyLobbyUI* LobbyUIWidget;

public:
	AWendyLobbyGameMode(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	void CreateLobbyUIWidget();
};

