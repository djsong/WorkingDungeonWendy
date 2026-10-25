// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WendyCommon.h"
#include "WendyDesktopImageComponent.generated.h"

class AWendyCharacter;
class UTexture2D;

#define WD_IMAGE_REP_PERF_CHECK STATS

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

#if WD_IMAGE_REP_PERF_CHECK
	int32 ReplicatedImageDataCounter = 0;
	double LastReplicatedImageDataCounterCheckTime = 0.0;
#endif

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
	/** A copy of SourceImageData as it was last sent, used to detect which regions changed.
	 *
	 * Deliberately the SAME layout as SourceImageData (FColor, one entry per pixel) rather than the packed
	 * 3-byte replication format. That makes the diff a single Memcmp per region instead of three byte
	 * comparisons per pixel across mismatched strides, and the update a single Memcpy. It also replaces a
	 * TMap of ~2800 per-region structs, each of which owned its own heap array, with one allocation. */
	TArray<FColor> LastSentImageMirror;
	/** One bit per region: has it been sent at least once since the last reset? Needed because a never-sent
	 * region could otherwise happen to match the zero-initialised mirror and be skipped forever. */
	TBitArray<> SentRegionFlags;

	bool bDirtyForReplication;
	/** Regions examined without finding a difference since the last actual change, accumulated across passes.
	 * Once it reaches a whole image's worth, every region has provably been checked and bDirtyForReplication
	 * can drop. Reset whenever anything is sent or the image is marked dirty again. */
	int32 CleanReplicateWalkCount;
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
	
	/** The three steps a replication pass is built from, kept separate so a caller can decide whether a
	 * region is even worth packing BEFORE paying to pack it. In diff mode most regions are unchanged, so the
	 * old pack-then-ask order threw that work away for the common case.
	 *
	 * Advances the walk cursor and reports the next region. InMaxElemNum is the caller's buffer capacity: the
	 * cursor must not advance past what can actually be packed or regions would be skipped. Returns false when
	 * there is nothing to walk (no captured image yet), so the caller can stop instead of spinning. */
	bool AdvanceToNextReplicateRegion(int32 InMaxElemNum, int32& OutBeginIndex, int32& OutElemNum);
	/** True when this region has to go out: either it hasn't been sent since the last reset, or its live pixels
	 * differ from the mirror of what was last sent. Reads the source directly, so nothing has to be packed to
	 * find out, and the comparison is a single Memcmp because the mirror shares SourceImageData's layout. */
	bool DoesReplicateRegionNeedSend(int32 InBeginIndex, int32 InElemNum) const;
	/** Records that this region's current pixels have now been sent. */
	void MarkReplicateRegionSent(int32 InBeginIndex, int32 InElemNum);
	/** Forget what was sent so the whole image goes out again (e.g. a new participant appeared). Also marks
	 * dirty, since otherwise nothing would re-walk the image to act on it. */
	void ResetSentImageRecord();
	/** Fills OutReplicateInfo with the region's pixels, ready to send. */
	void PackReplicateRegion(int32 InBeginIndex, int32 InElemNum, FWendyDesktopImageReplicateInfo& OutReplicateInfo) const;
	/** 
	 * To be called when the owner is remote character
	 * Put image data being transferred through network to SourceImageData here. 
	 * It sets only part of SourceImageData, depend on the replicate info size 
	 */
	void SetFromReplicateInfo(const FWendyDesktopImageReplicateInfo& InReplicateInfo);

	/** False means every region of SourceImageData is already known to match what was last sent, so walking it
	 * again can only waste time. Set true whenever SourceImageData actually changes - which happens from TWO
	 * places: local capture, and SetFromReplicateInfo applying received data (the latter is what lets a server
	 * relay a client's desktop onward). */
	bool IsDirtyForReplication() const { return bDirtyForReplication; }
	/** Force a re-walk. Needed after anything that invalidates the sender's record of what it already sent.
	 * Also discards accumulated clean-walk evidence, since the image may now differ again. */
	void MarkDirtyForReplication() { bDirtyForReplication = true; CleanReplicateWalkCount = 0; }

	/** Report what a replication pass found, so the component can work out when the image is provably in sync
	 * and the flag can drop. Evidence accumulates ACROSS passes on purpose: a single pass is capped by
	 * wd.DesktopImageExtractItorNum, which may well be smaller than the number of regions, in which case
	 * "walked a whole image in one pass" would be impossible to satisfy. */
	void NotifyReplicationWalkResult(int32 InWalkedRegionNum, int32 InSentRegionNum);

	/** How many region walks make up one complete pass over the image. */
	int32 GetReplicateRegionCount() const;

private:
	/** Which SentRegionFlags bit a region's begin index corresponds to. */
	int32 GetReplicateRegionIndex(int32 InBeginIndex) const;

public:

#if WD_IMAGE_REP_PERF_CHECK
	void UpdateForImageRefPerfCheck(int32 InRepDataNum);
#endif

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

