// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyExtendedWidgets.h"
#include "WendyCommon.h"

UWdTextBlock::UWdTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Much reason for text blocks should block the input? It could be but guess not that much..
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

//==================================================

UWdButton::UWdButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}
