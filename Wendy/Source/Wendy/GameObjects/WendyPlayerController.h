// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WendyPlayerController.generated.h"

/** It is supposed to be a simple base.. Just for a case */
UCLASS(config=Game)
class AWendyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWendyPlayerController(const FObjectInitializer& ObjectInitializer);
};

