// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/ObjectMacros.h"
#include "WendyGameSettings.generated.h"


/**
 * Put some (ini) configurable settings, but not like dynamically changing
 */
UCLASS(config=Game, defaultconfig)
class UWendyGameSettings  : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The map file path for the main world.. 
	 * In more extended application supporting multiple types of world selection, it might be going into some sort of table. */
	UPROPERTY(config, EditAnywhere, Category = Wendy)
	FString MainWorldPath;
	
	/** The IP address being displayed as a hint text at Lobby UI, and being used if user put nothing to the Ip address field. */
	UPROPERTY(config, EditAnywhere, Category = Wendy)
	FString DefaultServerIpAddress;

	/** The size of cloned display, to be used for rendering and also for sent through the network,
	 * while raw captured image might be bigger than this.
	 * It cannot simply being set in big enough value, mostly limited by network transferring performance. */
	UPROPERTY(config, EditAnywhere, Category = Wendy)
	FIntPoint InternalDesktopImageSize;

public:
	UWendyGameSettings(const FObjectInitializer& ObjectInitializer);

};
