// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WendyImageRepNetwork.h"
#include "WendyGameMode.generated.h"

/**
 * Initially created from ThirdPersion template.
 */
UCLASS(minimalapi)
class AWendyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWendyGameMode(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

};


