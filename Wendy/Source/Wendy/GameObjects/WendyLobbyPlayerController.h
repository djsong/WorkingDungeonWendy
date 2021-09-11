// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyPlayerController.h"
#include "WendyLobbyPlayerController.generated.h"

UCLASS(config = Game)
class AWendyLobbyPlayerController : public AWendyPlayerController
{
	GENERATED_BODY()

public:
	AWendyLobbyPlayerController(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
};

