// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyUiWidget.h"
#include "WendyCommon.h"

UWendyUiWidget::UWendyUiWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UWendyUiWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	StaticWidgetPreparations();
}