// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WendyCommon.h"
#include "WendyDesktopImageComponent.generated.h"

class AWendyCharacter;
class UTexture2D;

/** Management of acquring desktop image data 
 * either by actually capturing it from local desktop or by replicated data,
 * then also updating dynamically generated texture */
UCLASS()
class UWendyDesktopImageComponent : public UActorComponent
{
	GENERATED_BODY()

protected:

	/** We are dealing with desktop image, so need to have display metrics. 
	 * It would be required only for locally controlled owner. Otherwise there won't be a chance..? */
	FDisplayMetrics CachedDisplayMetrics;

	/** As captured image staging is not done in a single step, cache how much we gone so far..
	 * Be careful that cached indices are for desktop resolution, not the SourceImageData here. They could be different. 
	 * It is declared in vector (float) format, instead of FIntPoint because capturing stride can be in float */
	FVector2D LastStagingDesktopPixelCoordV;

	/**
	 * The source data to update the output texture.
	 * It is either from local desktop (in the case of locally controlled owner), 
	 * or from replicated data (in the case of other remote owners)
	 * In any case, this data is assumed to be dynamic, so the output texture should be updated periodically.
	 * For the locally controlled owner it is from the captured image source, but not that itself, a little refined from it.
	 */
	TArray<FColor> SourceImageData;
	/**
	 * Even before the SourceImageData..
	 * SourceImageData is based on this data, by its own resolution.
	 * Should be valid only when the owner is locally controlled.
	 */
	TArray<FColor> RawCapturedSourceImageData;

	/**
	 * The main output of this component.
	 * Dynamically generated and updated. Not a resource object that can be seen in content browser.
	 */
	UPROPERTY(Transient, VisibleAnywhere)
	UTexture2D* OutputTexture;

	/** 
	 * Tracking the index of SourceImageData array that has been copied for replication.
	 * so that it will begin from the next index.
	 * Only for the local character owner.
	 */
	int32 LastReplicatedImageDataIndex;

	/** Avallable for locally controlled owner. To be set when desktop image is just captured.
	 * It is supposed to be set like every tick in normally expected circumstances.
	 * but there might be some exceptional cases that updating data can be considered as waste. */
	bool bDirtyForReplication;
	/** Basically the same story as bDirtyForReplication,
	 * but it can be set for non locally controlled owner too. */
	bool bDirtyForTextureUpdate;

	/** To give some room to control texture update period.. */
	double LastTextureUpdatedTime;
		/** It could be changing within certain range.. */
		float TextureUpdatePeriodThisTime;

	/** A timer handle to control the period of desktop image capture, assuming the capturing is done in the main thread 
	 * It will be controlled in a different way if it is done at separated thread. */
	FTimerHandle CaptureDesktopImageTH;

public:
	UWendyDesktopImageComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

	/** Somthing that should be done at the beginning. */
	void CreateOutputTexture();

	/** Is it worth or fine to call frequently or just once at beginning? */
	void UpdateCachedDisplayMetrics();

	/** Only for locally controlled owner, do the job from the raw captured image to staing array */
	void UpdateLocalDesktopCaptureStaging();

	/** Simply to call CaptureDesktopImage.. by timer, as far as that is not done in the separated thread. */
	void CaptureDesktopImageTimerFn();
	/** Doing the most fundamental job of acquring the desktop image for locally controlled owner */
	void CaptureDesktopImage();

	/** Apply SourceImageData (either from local capture or replication whatsoever) to the finally used texture object. */
	void UpdateOutputTexture();
public:
	
	/**
	 * To be called when the owner is locally controlled character, 
	 * so to get image data transferred to server and other clients.
	 * It fills up OutReplicateInfo step by step each time you call this, managing the index that being set till last time.
	 */
	void ExtractReplicateInfo(FWendyDesktopImageReplicateInfo& OutReplicateInfo);
	/** 
	 * To be called when the owner is remote character
	 * Put image data being transferred through network to SourceImageData here. 
	 * It sets only part of SourceImageData, depend on the replicate info size 
	 */
	void SetFromReplicateInfo(const FWendyDesktopImageReplicateInfo& InReplicateInfo);

	/** If true, the image should be taken locally, otherwise from replicated data. 
	 * It is almost whether the owner is locally controlled character. */
	bool ShouldCaptureLocalImage() const;

	/** Returns resolution (not affected by display scale) of the first monitor 
	 * @param bPhysical : If true, returned value is not affected by display scale. */
	static void GetPrimaryMonitorResolution(int32& OutWidth, int32& OutHeight, bool bPhysical = true);

	/** Assumes CachedDisplayMetrics is updated. */
	FIntPoint GetCachedDesktopResolution() const;

	/** To supprt the captured image data size being different from the actual desktop resolution.
	 * While the resolutions are represented in int format, stride is in float format, 
	 * because that won't be clearly divided in most cases, and floating point stride will do the job for the purpose here. */
	FVector2D GetDesktopImageStagingStride() const;

	FORCEINLINE UTexture2D* GetOutputTexture() const { return OutputTexture; }
};

