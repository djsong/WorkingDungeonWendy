// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"

/**
 * Some bulk place, that might better goes GameInstance extended class?
 */
class FWendyDataStore
{
public:
	FWendyDataStore();
	~FWendyDataStore();

	FWendyAccountInfo UserAccountInfo;

};

FWendyDataStore& GetGlobalWendyDataStore();
