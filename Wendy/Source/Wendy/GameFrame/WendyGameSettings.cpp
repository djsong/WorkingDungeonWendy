// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyGameSettings.h"

UWendyGameSettings::UWendyGameSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Wendy");

	InternalDesktopImageSize = FIntPoint(1280, 720);
}

