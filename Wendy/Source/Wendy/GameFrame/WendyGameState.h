// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "WendyGameState.generated.h"

/**
 * GameState exists both server and client so we need our own if some mode is in network.
 */
UCLASS()
class AWendyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AWendyGameState(const FObjectInitializer& ObjectInitializer);
};



