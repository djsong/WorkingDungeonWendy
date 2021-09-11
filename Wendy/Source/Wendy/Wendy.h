// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_STATS_GROUP(TEXT("WendyGame"), STATGROUP_WendyGame, STATCAT_Advanced);


/**
 * The public interface to this module
 */
class FWendyGameModule : public FDefaultGameModuleImpl
{
	
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
