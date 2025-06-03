// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "WendyCommon.h"
#include "WendyDungeonSeat.generated.h"

class AWendyCharacter;
class UStaticMeshComponent;
class UTexture2D;
class UWendyDgSeatOriginPosComp;
class UCameraComponent;

/** 
 * It represents the seat (table and chair) that a character works in Wendy world,
 * like a home spot for a WendyCharacter
 */
UCLASS(config=Game)
class AWendyDungeonSeat : public AActor
{
	GENERATED_BODY()

protected:
	/** It doesn't have to be table only.. put thing that won't have special interaction with the character (except blocking the movement) */
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TableMeshComp;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ChairMeshComp;

	/** The mesh that will show the captured image. */
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MonitorMeshComp;

	/** To be visible when focusing. */
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HighlightOutlineMeshComp;

	/** It is simply to provide a practical safe postion of this seat. Could be a FVector(and FRotator) variable, but put a component for some convenience. */
	UPROPERTY(VisibleAnywhere)
	UWendyDgSeatOriginPosComp* RelativeOriginComp;
	
	/** To be the viewtarget when myself is get focused. */
	UPROPERTY(VisibleAnywhere)
	UCameraComponent* FocusingCamera;

	/** A resource texture showing default off state for the monitor. */
	UPROPERTY(EditAnywhere, Category="WendyDungeonSeat")
	UTexture2D* OffStateTexture;

	/** This should be the OutputTexture of UWendyDesktopImageComponent of a character having this seat as its home. 
	 * Its lifetime is not managed here, so tracking it with weak ptr. */
	TWeakObjectPtr<UTexture2D> DesktopImageTexture;

	/** The character who occupied this seat. If invalid there's no one. */
	TWeakObjectPtr<AWendyCharacter> OwnerCharacter;

private:
	bool bFocusHovered; // Not focused yet, but like being candidate
	bool bFocused;

public:
	/** Name of the material slot that will be applied captured desktop image.
	 * It should be setup to the static mesh resource representing the monitor. */
	const static FName MonitorMatSlotName;
	/** The material assigned to the slot of MonitorMatSlotName 
	 * should has its main texture parameter of this name. */
	const static FName DesktopImageTextureParamName;

public:
	AWendyDungeonSeat(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	/** DesktopImageTexture should be given from outside, from the character having this as its home. */
	void SetDesktopImageTexture(UTexture2D* InTexture);
	/** Something that should be done when the character having this as home left,
	 * or as the first state when occupied by nobody. */
	void FallbackToOffState();
	/** Goes to off state only when not occupied. Safe measure */
	void ConditionalFallbackToOffState();

	void SetFocusHovered(bool bInFocusHovered);
	bool IsFocusHovered() const { return bFocusHovered; }

	void SetFocused(bool bInFocused);
	bool IsFocused() const { return bFocused; }

private:
	void SetMonitorTextureInternal(UTexture2D* InTexture);

public:
	/** You use this instead of GetActorLocation to get the safe position of this seat. */
	FVector GetOriginPos() const;
	FRotator GetOriginRot() const;

	void SetOwnerCharacter(AWendyCharacter* InCharacter);
	bool IsOccupied() const;

	/** Return empty string if not occupied. */
	FString GetOwnerCharacterId() const;

	FORCEINLINE const UStaticMeshComponent* GetMonitorMeshComp() const { return MonitorMeshComp; }
};

/** It could be just a FVector (and optionally FRotator), but for a little convenience.. */
UCLASS()
class UWendyDgSeatOriginPosComp : public USceneComponent
{
	GENERATED_BODY()

	/*
	
		Not really much thing expected here
		but we might add some editor only icon..

	*/

public:
	UWendyDgSeatOriginPosComp(const FObjectInitializer& ObjectInitializer);

};