// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "WendyCommon.h"
#include "WdGameplayStatics.generated.h"


/**
 * Extended gameplay utils for Wendy project
 * Not necessarily BlueprintCallables.
 */
UCLASS()
class UWdGameplayStatics : public UGameplayStatics
{
	GENERATED_BODY()
public:

	/** Returns player controller which is locally controlled
	 * If there's multiple then it will return the first encountered. */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class APlayerController* GetLocalPlayerController(const UObject* WorldContextObject);

	/** Returns player pawn which is locally controlled */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class APawn* GetLocalPlayerPawn(const UObject* WorldContextObject);

	/** Returns the player character (NULL if the player pawn doesn't exist OR is not a character) which is locally controlled */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class AWendyCharacter* GetLocalPlayerCharacter(const UObject* WorldContextObject);

	/** Enter the main world, assuming it is still standalone before coming here, so you give the basic information for connection
	 * both for server or client.
	 * If there comes another world then, would it requires conneting info too? */
	static void EnterWendyWorld(UObject* WorldContextObject, const FWendyWorldConnectingInfo& ConnectingInfo);
};

/** 
 * As far as Wendy captures desktop image as is, it will include the game window of Wendy itself, which we don't really want to show.
 * It is like the least workaround until we find some way to exclude Wendy window from captured image.
 */
void TryClampViewForWholeInclusiveDesktopCapture(UObject* WorldContextObject);