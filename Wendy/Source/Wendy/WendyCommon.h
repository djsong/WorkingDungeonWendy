// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.generated.h"

/**
 ==========================================================
 WendyCommon.h

 Putting some commonly referred symbols for Wendy Project.
 ==========================================================
 */


DECLARE_LOG_CATEGORY_EXTERN(LogWendy, Log, All);


 /**
  * As we don't have some UI layering and managing system.. Let's go simple
  */
const int32 WD_MAIN_UI_ZORDER = 0;

/**
 * It can be sent through network with fixed length buffer, so need limitation. (+1 for null termination)
 * I don't know why, just having trouble with declaring it as const int32..
 */
#define WD_USER_ID_MAX_LEN 12
#define WD_USER_ID_MAX_LEN_PLUS_ONE (WD_USER_ID_MAX_LEN + 1)
FORCEINLINE const FString GetClampedWendyUserIdString(const FString& InRawStr)
{
	if (InRawStr.Len() >= WD_USER_ID_MAX_LEN_PLUS_ONE)
	{
		return InRawStr.LeftChop(InRawStr.Len() - WD_USER_ID_MAX_LEN_PLUS_ONE + 1);
	}
	return InRawStr;
}

/** Collections of things necessary to enter the main world,
 * mostly for server connection? */
struct FWendyWorldConnectingInfo
{
	FWendyWorldConnectingInfo()
	{}

	FString UserId;
	FString ServerIp;

	/** You won't need ServerIp if this is true */
	bool bMyselfServer = false;

	FORCEINLINE bool IsValid() const { return UserId.Len() > 0 && (bMyselfServer || ServerIp.Len() > 0); }
};

/** Color format that is inteded to be used for replication only 
 * Instead of using FColor, use smaller format to minimize network transfer. */
USTRUCT()
struct FWendyReplicatedColor
{
	GENERATED_BODY()

	FWendyReplicatedColor()
		: R(0), G(0), B(0)
	{}

	UPROPERTY(Transient)
	uint8 R;
	UPROPERTY(Transient)
	uint8 G;
	UPROPERTY(Transient)
	uint8 B;
};

/** More structed info that is used for replicating the data stream of FWendyReplicatedColor */
USTRUCT()
struct FWendyDesktopImageReplicateInfo
{
	GENERATED_BODY()

	FWendyDesktopImageReplicateInfo()
		: UpdateBeginIndex(0), UpdateElemNum(0)
	{}

	/** The index position of the final buffer that the UpdateColors will be copied */
	UPROPERTY(Transient)
	int32 UpdateBeginIndex;

	/** Typically size of ImageData array, but it could be smaller in some circumstances */
	UPROPERTY(Transient)
	int32 UpdateElemNum;

	/** Part of whole image data that is transferred and to be updated this time. */
	UPROPERTY(Transient)
	TArray<FWendyReplicatedColor> ImageData;
};

/** The user account info like user name. */
USTRUCT()
struct FWendyAccountInfo
{
	GENERATED_BODY()

	FWendyAccountInfo()
	{}

	UPROPERTY(Transient)
	FString UserId; // Id? Nickname? Whatever..
};
