// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WendyUiWidget.generated.h"

/**
 * Common native base for all UI class of Wendy project.
 */
UCLASS()
class UWendyUiWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWendyUiWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;

	/** This is the place where you put some binding like set text and bind delegate, etc..
	 * for statically placed widgets. */
	virtual void StaticWidgetPreparations() {/* Don't put anything here so sub classes don't have to call super's */}
};

