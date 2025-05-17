// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDataStore.h"

/** It might better goes inside some slightly wrapped place like GameInstance? */
FWendyDataStore GWendyDataStore;

FWendyDataStore::FWendyDataStore()
{
}

FWendyDataStore::~FWendyDataStore()
{
}

void FWendyDataStore::SetUserAccountInfo(const FWendyAccountInfo& InAccountInfo)
{
	UserAccountInfo = InAccountInfo;
	
	ensureMsgf(UserAccountInfo.UserId.Len() < WD_USER_ID_MAX_LEN_PLUS_ONE, TEXT("Id (%s) is too long, Isn't it clamped?"), *UserAccountInfo.UserId);

	// How should we do duplicated ID check while this whole data store is bound to just one user.
}

FWendyDataStore& GetGlobalWendyDataStore()
{
	return GWendyDataStore;
}