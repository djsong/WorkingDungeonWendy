// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyPlayerController.h"
#include "WendyDungeonPlayerController.generated.h"


/**
 * The main Wendy world playing controller.
 */
UCLASS(config = Game)
class AWendyDungeonPlayerController : public AWendyPlayerController
{
	GENERATED_BODY()

protected:

public:
	AWendyDungeonPlayerController(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* aPawn) override;
};
