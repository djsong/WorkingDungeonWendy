// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "WendyExtendedWidgets.generated.h"

/**

	 Not all widgets are extended.

	 Some very commonly used and expect some different behavior easily.

*/

UCLASS()
class UWdTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	UWdTextBlock(const FObjectInitializer& ObjectInitializer);
};

//==================================================

/**
 * Even you don't have nothing to modify for Button behavior, 
 * it is quite freqent to have some.. why not?
 */
UCLASS()
class UWdButton : public UButton
{
	GENERATED_BODY()

public:
	UWdButton(const FObjectInitializer& ObjectInitializer);
};