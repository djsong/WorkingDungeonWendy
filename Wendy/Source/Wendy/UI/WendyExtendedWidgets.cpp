// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyExtendedWidgets.h"
#include "WendyCommon.h"

UWdTextBlock::UWdTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Much reaon for text blocks should block the input? It could be but guess not that much..
	Visibility = ESlateVisibility::HitTestInvisible;
}

//==================================================

UWdButton::UWdButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}
