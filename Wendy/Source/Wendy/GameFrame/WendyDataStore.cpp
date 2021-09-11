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

FWendyDataStore& GetGlobalWendyDataStore()
{
	return GWendyDataStore;
}