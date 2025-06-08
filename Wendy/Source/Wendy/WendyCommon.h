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

	FORCEINLINE bool operator==(const FWendyReplicatedColor& Other) const
	{
		return (this->R == Other.R) && (this->G == Other.G) && (this->B == Other.B);
	}
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

	FORCEINLINE bool operator==(const FWendyDesktopImageReplicateInfo& Other) const
	{
		return (this->UpdateBeginIndex == Other.UpdateBeginIndex)
			&& (this->UpdateElemNum == Other.UpdateElemNum)
			&& (this->ImageData == Other.ImageData);
	}
	FORCEINLINE bool operator!=(const FWendyDesktopImageReplicateInfo& Other) const
	{
		return !(*this == Other);
	}
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

enum class EWendyRemoteInputKeys : uint8
{
	None,

	MLB,
	MRB,

	Key_BackSpace,
	Key_Tab,
	Key_Enter,
	Key_Escape,
	Key_SpaceBar,
	Key_PageUp,
	Key_PageDown,
	Key_Left,
	Key_Up,
	Key_Right,
	Key_Down,
	Key_Delete,
	Key_Slash,
	Key_Backslash,
	Key_Tilde,

	Key_Zero,
	Key_One,
	Key_Two,
	Key_Three,
	Key_Four,
	Key_Five,
	Key_Six,
	Key_Seven,
	Key_Eight,
	Key_Nine,

	Key_F1,
	Key_F2,
	Key_F3,
	Key_F4,
	Key_F5,
	Key_F6,
	Key_F7,
	Key_F8,
	Key_F9,
	Key_F10,
	Key_F11,
	Key_F12,

	Key_A,
	Key_B,
	Key_C,
	Key_D,
	Key_E,
	Key_F,
	Key_G,
	Key_H,
	Key_I,
	Key_J,
	Key_K,
	Key_L,
	Key_M,
	Key_N,
	Key_O,
	Key_P,
	Key_Q,
	Key_R,
	Key_S,
	Key_T,
	Key_U,
	Key_V,
	Key_W,
	Key_X,
	Key_Y,
	Key_Z,

	/*
	 !!!!!!!!!!  

	 You must modify its conversion util (FromFKeyToWendyRemoteKey, FromWendyRemoteKeyToWinVK)
	 for new entry.	
	
	!!!!!!!!!!
	*/

	End
};
EWendyRemoteInputKeys FromFKeyToWendyRemoteKey(const FKey InFKey);
uint8 FromWendyRemoteKeyToWinVK(EWendyRemoteInputKeys InRemoteInputKey);

enum class EWendyRemoteInputEvents : uint8
{
	None,

	Pressed,
	Released,

	End
};

/** For monitor input tracking and faking. */
struct FWendyMonitorHitAndInputInfo
{
	FString TargetUserId; // The owner of the picked monitor, not the current user.
	FVector2D MonitorHitUV = FVector2D(-1.0f, -1.0f);
	EWendyRemoteInputKeys InputKey = EWendyRemoteInputKeys::None;
	EWendyRemoteInputEvents InputEvent = EWendyRemoteInputEvents::None;

	FORCEINLINE bool HasValidInfo() const {
		return TargetUserId.Len() > 0 &&
			MonitorHitUV.X >= 0.0f && MonitorHitUV.X <= 1.0f &&
			MonitorHitUV.Y >= 0.0f && MonitorHitUV.Y <= 1.0f;
	}
	FORCEINLINE void SetInvalid()
	{
		MonitorHitUV.X = MonitorHitUV.Y = -1.0f;
	}
};