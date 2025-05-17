// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"

/**
 * Some bulk place, that might better goes GameInstance extended class?
 */
class FWendyDataStore
{
	FWendyAccountInfo UserAccountInfo;

public:
	FWendyDataStore();
	~FWendyDataStore();

	void SetUserAccountInfo(const FWendyAccountInfo& InAccountInfo);
	const FWendyAccountInfo& GetUserAccountInfo() const { return UserAccountInfo; }
};

FWendyDataStore& GetGlobalWendyDataStore();
