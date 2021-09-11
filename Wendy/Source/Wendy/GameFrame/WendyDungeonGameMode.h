// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyGameMode.h"
#include "WendyDungeonGameMode.generated.h"


/**
 * Not so much here for GameMode, which exist only in server..?
 * There's not much "rule"s expected..
 */
UCLASS()
class AWendyDungeonGameMode : public AWendyGameMode
{
	GENERATED_BODY()

public:
	AWendyDungeonGameMode(const FObjectInitializer& ObjectInitializer);

};

