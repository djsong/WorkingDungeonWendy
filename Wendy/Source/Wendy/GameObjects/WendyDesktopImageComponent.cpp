// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDesktopImageComponent.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "WdGameplayStatics.h"
#include "Wendy.h"
#include "WendyCharacter.h"
#include "WendyGameSettings.h"
#include "TextureResource.h"

#if PLATFORM_WINDOWS
// For screen captures, directly using windows API 
// It would be the best if being implemented per extended FGenericPlatformApplicationMisc classes, 
// but I won't do that far here.
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"
#include <Windows.h>
#include <wingdi.h>
#endif // PLATFORM_WINDOWS

DECLARE_CYCLE_STAT(TEXT("UpdateDesktopCapture"), STAT_UpdateLocalDesktopCaptureStaging, STATGROUP_WendyGame);
DECLARE_CYCLE_STAT(TEXT("UpdateOuputTexture"), STAT_UpdateOuputTexture, STATGROUP_WendyGame);
DECLARE_CYCLE_STAT(TEXT("ExtractReplicateInfo"), STAT_ExtractReplicateInfo, STATGROUP_WendyGame);

FIntPoint GetWendyDesktopImageSize()
{
	const UWendyGameSettings* WdGameSettings = GetDefault<UWendyGameSettings>(UWendyGameSettings::StaticClass());
	if (WdGameSettings != nullptr && WdGameSettings->InternalDesktopImageSize.X > 0 && WdGameSettings->InternalDesktopImageSize.Y > 0)
	{
		return WdGameSettings->InternalDesktopImageSize;
	}
	return FIntPoint(1280, 720); // Just some mild fallback.
}

static TAutoConsoleVariable<int32> CVarWdDesktopImageStagingPixelsInOneTick(
	TEXT("wd.DesktopImageStagingPixelsInOneTick"),
	500000, // It doesn't have to be in sync with CapturedImageReplicateSize
	TEXT("How many pixels are taken from the captured source in a single tick, for locally controlled owner.")
	TEXT("This is about transferring the raw captured source to a little refined staging source in a desired size,")
	TEXT("handling all pixels in a tick can slow down the game thread much if target resolution gets big.")
	TEXT("Having higher of this value might have effect only in a local machine if network transferring cannot take up to this speed"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdOutputTextureUpdatePeriod(
	TEXT("wd.OutputTextureUpdatePeriod"),
	0.1f,
	TEXT("Regardless of the update condition of source image data, final texture update period can be delayed by this setting.")
	TEXT("0 or negative value will make it updated whenever it is required."),
	ECVF_Scalability);
static TAutoConsoleVariable<float> CVarWdOutputTextureUpdatePeriod_RandFrac(
	TEXT("wd.OutputTextureUpdatePeriod_RandFrac"),
	0.0f,
	TEXT("A bit of random is applied every update period. This is the fraction of original OutputTextureUpdatePeriod setting."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarWdDesktopCaptureBasePeriod(
	TEXT("wd.DesktopCaptureBasePeriod"),
	0.1f,
	TEXT("A base interval to capture the desktop image, the first and fundamental step to acqure desktop image.")
	TEXT("Having this interval variable means that image capturing is not incremental."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarWdDesktopCaptureBasePeriod_RandFrac(
	TEXT("wd.DesktopCaptureBasePeriod_RandFrac"),
	0.0f,
	TEXT("A bit of random is applied every capture period. This is the fraction of original DesktopCaptureBasePeriod setting."),
	ECVF_Default);


UWendyDesktopImageComponent::UWendyDesktopImageComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	CachedDisplayMetrics.PrimaryDisplayWidth = 0;
	CachedDisplayMetrics.PrimaryDisplayHeight = 0;
	LastStagingDesktopPixelCoordV.X = 0;
	LastStagingDesktopPixelCoordV.Y = 0;

	const FIntPoint WendyDesktopImageSize = GetWendyDesktopImageSize();
	SourceImageData.AddZeroed(WendyDesktopImageSize.X * WendyDesktopImageSize.Y);

	OutputTexture = nullptr;

	LastReplicatedImageDataIndex = -1;

	bDirtyForReplication = false;
	bDirtyForTextureUpdate = false;
	LastTextureUpdatedTime = 0.0;
	TextureUpdatePeriodThisTime = 0.0f;
}

void UWendyDesktopImageComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ShouldCaptureLocalImage() 
		
		// Is checking bDirtyForReplication necessary? and even proper when the staging size is not ensured to be the same as replicating size?
		//&& bDirtyForReplication == false 
		
		)
	{
		// Won't do the direct capturing job here. It will do some incremental converting..
		UpdateLocalDesktopCaptureStaging();
	}

	// Non locally controlled owner also need output texture update.
	// @TODO Wendy UpdateOutputTexture : but why isn't it driven by timer? I might thought about incremental processing in every tick..
	UpdateOutputTexture();
}

void UWendyDesktopImageComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ShouldCaptureLocalImage())
	{
		UpdateCachedDisplayMetrics();

		// Some hacky workaround that might make you embarassing.
		TryClampViewForWholeInclusiveDesktopCapture(this);

		// As the first capturing timer.
		CaptureDesktopImageTimerFn();
	}

	CreateOutputTexture();
}

void UWendyDesktopImageComponent::CreateOutputTexture()
{
	// It doesn't check for current validity of OutputTexture
	// If newly created the old one will automatically GCd unless there is other tracked reference.

	const FIntPoint WendyDesktopImageSize = GetWendyDesktopImageSize();
	OutputTexture = UTexture2D::CreateTransient(WendyDesktopImageSize.X, WendyDesktopImageSize.Y,
		EPixelFormat::PF_B8G8R8A8, 
		*FString::Printf(TEXT("WendyDesktopImageOutputTexture_%s"), (GetOwner() != nullptr) ? *GetOwner()->GetName() : *GetName())
	);

	// A bit confusing whether captured image should be treated as sRGB.. Guess yes..?
	OutputTexture->SRGB = true; // #CapturedDesktopImageGamma

	// I am not sure.. perhaps there could be some timing issue?
	FlushRenderingCommands();

	// Refer to FTexturePlatformData::TryLoadMips ?
	//FTexture2DMipMap& MipRef = OutputTexture->PlatformData->Mips[0];
	//MipRef.BulkData.Lock(LOCK_READ_ONLY LOCK_READ_WRITE)
}

void UWendyDesktopImageComponent::UpdateCachedDisplayMetrics()
{
	// @TODO Wendy DesktopImageComponent
	// There surely is a way to get current metric too. I just don't feel to need for such thing before make it working in rough way.
	FSlateApplication::Get().GetInitialDisplayMetrics(CachedDisplayMetrics);

	// At this point CachedDisplayMetrics.PrimaryDisplayWidth/Height can be scaled value, but we want unscaled physical resolution.
	// but couldn't we just use GetPrimaryMonitorResolution from the first place? Is there any other possible use of CachedDisplayMetrics?

	GetPrimaryMonitorResolution(CachedDisplayMetrics.PrimaryDisplayWidth, CachedDisplayMetrics.PrimaryDisplayHeight, true);
}

#if PLATFORM_WINDOWS
namespace WendyWindowsDesktopCaptureImpl
{
	/** Returned BITMAPINFO is what you would need for actual bitmap data access. 
	 * It is allocated internally so you should release it after using it. */
	BITMAPINFO* GetBitmapInfoFromHBitmap(HBITMAP InHBit)
	{
		HDC WholeScreenDC = GetDC(nullptr);

		BITMAP BitHeaderDef;
		BITMAPINFOHEADER BitInfoHeader;
		
		// Don't understand why GetObject is not found.. That should be a simple wrapper to GetObjectW or GetObjectA according to UNICODE define.
		GetObjectW(InHBit, sizeof(BITMAP), &BitHeaderDef);

		//GetObject(InHBit, sizeof(BITMAP), &BitHeaderDef);
		BitInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
		BitInfoHeader.biWidth = BitHeaderDef.bmWidth;
		BitInfoHeader.biHeight = BitHeaderDef.bmHeight;
		BitInfoHeader.biPlanes = 1;
		BitInfoHeader.biBitCount = BitHeaderDef.bmPlanes * BitHeaderDef.bmBitsPixel;// Like bit nubmer per pixel?
		BitInfoHeader.biCompression = BI_RGB;
		BitInfoHeader.biSizeImage = 0;
		BitInfoHeader.biXPelsPerMeter = 0;
		BitInfoHeader.biYPelsPerMeter = 0;
		BitInfoHeader.biClrUsed = 0;
		BitInfoHeader.biClrImportant = 0;

		// Cannot allocated in a whole size from the first place.
		// As the first step, allocated as info header size and pallete.
		int32 PalSize = (BitInfoHeader.biBitCount > 8 ? 0 : 1 << BitInfoHeader.biBitCount) * sizeof(RGBQUAD);
		BITMAPINFO* RetBitInfo = (BITMAPINFO *)malloc(BitInfoHeader.biSize + PalSize);
		RetBitInfo->bmiHeader = BitInfoHeader;

		// Not get the size of bitmap to resize BITMAPINFO allocation
		GetDIBits(WholeScreenDC, InHBit, 0, BitHeaderDef.bmHeight, NULL, RetBitInfo, DIB_RGB_COLORS);
		BitInfoHeader = RetBitInfo->bmiHeader;

		// Just for a case if somethings wrong.
		if (BitInfoHeader.biSizeImage == 0) {
			BitInfoHeader.biSizeImage = ((((BitInfoHeader.biWidth * BitInfoHeader.biBitCount) + 31) & ~31) >> 3) * BitInfoHeader.biHeight;
		}

		// Re-allocation now we know that the final size.
		DWORD BitSizeByte = BitInfoHeader.biSize + PalSize + BitInfoHeader.biSizeImage;
		RetBitInfo = (BITMAPINFO *)realloc(RetBitInfo, BitSizeByte);

		// Everythings ready, get the data.
		GetDIBits(WholeScreenDC, InHBit, 0, BitHeaderDef.bmHeight, (PBYTE)RetBitInfo + BitInfoHeader.biSize + PalSize, RetBitInfo, DIB_RGB_COLORS);
		
		ReleaseDC(nullptr, WholeScreenDC);
		
		return RetBitInfo;
	}

	/** The main function that capture the whole desktop image of primary display,
	 * OutCapturedImageData is supposed to be in the size of primary display or it will be reallocated anyway. 
	 * #WendyDealsWithPrimaryDisplayOnly */
	void GetPrimaryDisplayCapturedData(TArray<FColor>& OutCapturedImageData)
	{
		int32 PrimDisplayWidth = 0;
		int32 PrimDisplayHeight = 0;
		// It should be physical resolution not being affected by display scale.
		UWendyDesktopImageComponent::GetPrimaryMonitorResolution(PrimDisplayWidth, PrimDisplayHeight, true);

		// I just copied the rest of code from some place..
		HDC hScrDC = ::CreateDC(TEXT("DISPLAY"), nullptr, nullptr, nullptr); // Does that specified name do some?
		HDC hMemDC = CreateCompatibleDC(hScrDC);
		HBITMAP hBitmap = CreateCompatibleBitmap(hScrDC, PrimDisplayWidth, PrimDisplayHeight);
		SelectObject(hMemDC, hBitmap);

		BitBlt(hMemDC, 0, 0, PrimDisplayWidth, PrimDisplayHeight, hScrDC, 0, 0, SRCCOPY);

		BITMAPINFO* BitInfoData = GetBitmapInfoFromHBitmap(hBitmap);
		if (BitInfoData != nullptr)
		{
			const int32 OutArraySize = PrimDisplayWidth * PrimDisplayHeight;
			if (OutCapturedImageData.Num() != OutArraySize)
			{
				OutCapturedImageData.Empty();
				OutCapturedImageData.AddZeroed(OutArraySize);
			}

			// It assumes that RGBA is aligned in the same order.
			FMemory::Memcpy(OutCapturedImageData.GetData(), BitInfoData->bmiColors, OutArraySize * sizeof(FColor));
			// RGBA order matches indeed, but it is reversed in element order,
			// Even simple reversing cannot help, it is only upside down.
			// Algo::Reverse(OutCapturedImageData);

			// This is allocated from inside. Should manually free it.
			// @TODO Wendy DesktopCapture : It might not have to be allocated and released everytime?
			free(BitInfoData);
			BitInfoData = nullptr;
		}

		DeleteDC(hMemDC);
		DeleteDC(hScrDC);
	}
}
using namespace WendyWindowsDesktopCaptureImpl;
#else
static_assert(0, "Not supported platform.. not much plan. You also need to check other places having platform define mark.");
#endif

void UWendyDesktopImageComponent::UpdateLocalDesktopCaptureStaging()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateLocalDesktopCaptureStaging);

	checkf(ShouldCaptureLocalImage(), TEXT("Trying to do the staging captured local image while owner is not appeared to be a local controlled character."));

	/////////////////////////////////////////////
	//
	// @TODO Wendy DesktopImageCapture
	//		For now, it will be done in the main thread.
	//		Capturing and staging the desktop image might not be the bottleneck when there are other things to optimize,
	//		but sometime it will probably be needed.
	//
	/////////////////////////////////////////////

	FIntPoint WendyDesktopImageSize = GetWendyDesktopImageSize();

	const FIntPoint SrcDesktopResolution = GetCachedDesktopResolution();
	const FVector2D ImageStagingIndexStride = GetDesktopImageStagingStride();
	const int32 StagingItorMaxNum = CVarWdDesktopImageStagingPixelsInOneTick.GetValueOnGameThread();
	for (int32 CountIndex = 0; CountIndex < StagingItorMaxNum; ++CountIndex)
	{
		// Not capturing all the pixels if desktop size is bigger than SourceImageData size, yes SourceImageData could be bigger, not much expected though.

		FVector2D CapturePointInDesktopV(LastStagingDesktopPixelCoordV.X + ImageStagingIndexStride.X,
			LastStagingDesktopPixelCoordV.Y);

		/*FIntPoint CapturePointInDesktop(
			FMath::RoundToInt(LastStagingDesktopPixelCoordV.X + ImageCaptureIndexStride.X),
			FMath::RoundToInt(LastStagingDesktopPixelCoordV.Y)
		);*/

		// Here compare it with ceiled int, to remove chance for out of bound index due to some precision issue.
		if (FMath::CeilToInt(CapturePointInDesktopV.X) >= SrcDesktopResolution.X)
		{
			// Next row.
			CapturePointInDesktopV.X = 0.0f;
			CapturePointInDesktopV.Y = LastStagingDesktopPixelCoordV.Y + ImageStagingIndexStride.Y;

			if (FMath::CeilToInt(CapturePointInDesktopV.Y) >= SrcDesktopResolution.Y)
			{
				// We ran the whole the way down. Do it from the starting point again.
				CapturePointInDesktopV.Y = 0.0f;
			}
		}

		LastStagingDesktopPixelCoordV = CapturePointInDesktopV;

		// I would like to make vector version prior to intpoint version, but row changing operation right above will be safer in int format.
		//FVector2D CapturePointInDesktopV(CapturePointInDesktop);

		FIntPoint CapturePointInDesktop(
			FMath::RoundToInt(LastStagingDesktopPixelCoordV.X),
			FMath::RoundToInt(LastStagingDesktopPixelCoordV.Y)
		);

		FColor CurrCaptureColor(FColor::Blue);

#if 1
	#if PLATFORM_WINDOWS
		// Captured raw data is upside down.
		int32 CapturedDataIndex = (SrcDesktopResolution.Y - CapturePointInDesktop.Y - 1) * SrcDesktopResolution.X + CapturePointInDesktop.X;
	#else
		// Don't know if other platforms have such order. Anyway there's not much plan for that.
		int32 CapturedDataIndex = CapturePointInDesktop.Y * SrcDesktopResolution.X + CapturePointInDesktop.X;
	#endif
		if (RawCapturedSourceImageData.IsValidIndex(CapturedDataIndex))
		{
			CurrCaptureColor = RawCapturedSourceImageData[CapturedDataIndex];
		}
#else
		// Some noise fallback, which was used before actual capturing was not implemented yet.
		const uint8 RandNoise = FMath::RandRange(0, 100);
		// Nothing important.. Just want to put some vertical coloring gradation.
		const float FracV = CapturePointInDesktopV.Y / (float)SrcDesktopResolution.Y;
		CurrCaptureColor = FColor(RandNoise, 
			(uint8)(FracV * 255.0f), (uint8)((1.0f - FracV) * 255.0f)
		);
#endif

		// Save it to SourceImageData, so it can goes to texture and being replicated too..
		FVector2D CapturePointInImageDataV = CapturePointInDesktopV / ImageStagingIndexStride;

		const int32 ImageDataIndex = (int32)CapturePointInImageDataV.Y * WendyDesktopImageSize.X + (int32)CapturePointInImageDataV.X;
		// Might be a chance of out of array range while float int conversion?
		if (SourceImageData.IsValidIndex(ImageDataIndex))
		{
			SourceImageData[ImageDataIndex] = CurrCaptureColor;
		}
	}


	// For now there's no actual condition.
	bDirtyForReplication = true;
	bDirtyForTextureUpdate = true;
}

void UWendyDesktopImageComponent::CaptureDesktopImageTimerFn()
{
	UWorld* OwnerWorld = GetWorld();
	if (OwnerWorld != nullptr)
	{
		OwnerWorld->GetTimerManager().ClearTimer(CaptureDesktopImageTH);

		// For the next term. setting the timer again with randomly calculated period.
		const float BaseCapturePeriod = CVarWdDesktopCaptureBasePeriod.GetValueOnGameThread();
		const float RandFrac = FMath::Clamp(CVarWdDesktopCaptureBasePeriod_RandFrac.GetValueOnGameThread(), 0.0f, 1.0f);
		const float RandRadius = BaseCapturePeriod * RandFrac;
		// Should keep the timer running, so don't let it become zero or less.
		const float ThisTimePeriod = FMath::Max(FMath::RandRange(BaseCapturePeriod - RandRadius, BaseCapturePeriod + RandRadius), KINDA_SMALL_NUMBER);

		OwnerWorld->GetTimerManager().SetTimer(CaptureDesktopImageTH, this, &UWendyDesktopImageComponent::CaptureDesktopImageTimerFn, ThisTimePeriod);
	}

	// Guess it shouldn't come here from the first place, but do the double check.
	if (ShouldCaptureLocalImage())
	{
		CaptureDesktopImage();
	}
}

void UWendyDesktopImageComponent::CaptureDesktopImage()
{
#if PLATFORM_WINDOWS
	GetPrimaryDisplayCapturedData(RawCapturedSourceImageData);
#else

	// For other platforms?? Probably not.

#endif
}

void UWendyDesktopImageComponent::UpdateOutputTexture()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateOuputTexture);

	const double CurrentRealTime = FPlatformTime::Seconds();

	// Locally controlled owner might not need time check anymore if image capturing itself is done periodically..
	const bool bTimeToUpdate = (TextureUpdatePeriodThisTime <= 0.0f || CurrentRealTime >= LastTextureUpdatedTime + TextureUpdatePeriodThisTime);

	if (bDirtyForTextureUpdate 
		&& bTimeToUpdate
		&& IsValid(OutputTexture) && OutputTexture->GetPlatformData() != nullptr && OutputTexture->GetPlatformData()->Mips.Num() > 0)
	{
		///////////////////////////////////
		// Update itself

		FTexture2DMipMap& MipRef = OutputTexture->GetPlatformData()->Mips[0];
		{  
			FColor* MipData = reinterpret_cast<FColor*>(MipRef.BulkData.Lock(LOCK_READ_WRITE));

			const FIntPoint WendyDesktopImageSize = GetWendyDesktopImageSize();
			// If we are going to update progressively, some loop is needed ,
			// but the format is just the same then why not simply does memcpy?
			if (MipRef.SizeX == WendyDesktopImageSize.X && MipRef.SizeY == WendyDesktopImageSize.Y)
			{
				FMemory::Memcpy(MipData, SourceImageData.GetData(), MipRef.SizeX * MipRef.SizeY * sizeof(FColor));
			}
			else
			{
				// If this path is really needed, 
				// better not iterate all at once.
				// Do some progressive job with some mark like LastReplicatedImageDataIndex
				for (int32 IdxX = 0; IdxX < MipRef.SizeX; ++IdxX)
				{
					for (int32 IdxY = 0; IdxY < MipRef.SizeY; ++IdxY)
					{
						const int32 OneDimIndex = (IdxX + (IdxY * MipRef.SizeX));
						if (SourceImageData.IsValidIndex(OneDimIndex))
						{
							MipData[OneDimIndex] = SourceImageData[OneDimIndex];
						}
					}
				}
			}
			MipRef.BulkData.Unlock();
			OutputTexture->UpdateResource();
		}
		///////////////////////////////////
		// For the next time..
		{
			bDirtyForTextureUpdate = false;
			LastTextureUpdatedTime = CurrentRealTime;

			const float BaseUpdatePeriod = CVarWdOutputTextureUpdatePeriod.GetValueOnGameThread();
			const float RandFrac = FMath::Clamp(CVarWdOutputTextureUpdatePeriod_RandFrac.GetValueOnGameThread(), 0.0f, 1.0f);
			if (BaseUpdatePeriod > 0.0f)
			{
				const float RandRadius = BaseUpdatePeriod * RandFrac;

				TextureUpdatePeriodThisTime = BaseUpdatePeriod + FMath::RandRange(BaseUpdatePeriod - RandRadius, BaseUpdatePeriod + RandRadius);
			}
			else
			{
				TextureUpdatePeriodThisTime = 0.0f;
			}
		}
	}
}

void UWendyDesktopImageComponent::ExtractReplicateInfo(FWendyDesktopImageReplicateInfo& OutReplicateInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_ExtractReplicateInfo);

	// It is assume that this function is called per replication interval,
	// but how often repliation being handled? like once in a tick?
	
	// Is checking bDirtyForReplication necessary? and even proper when the staging size is not ensured to be the same as replicating size?
	// if (bDirtyForReplication) 
	{
		OutReplicateInfo.UpdateBeginIndex = FMath::Max(LastReplicatedImageDataIndex, 0);
		if (OutReplicateInfo.UpdateBeginIndex >= SourceImageData.Num() - 1)
		{
			OutReplicateInfo.UpdateBeginIndex = 0;
		}

		const int32 ReplicateBunchElemSize = GetWdDesktopImageReplicateElemSize();

		// OutReplicateInfo.ImageData would probably be sized to ReplicateBunchElemSize.. Just double checking here.
		OutReplicateInfo.UpdateElemNum = FMath::Min(OutReplicateInfo.ImageData.Num(), ReplicateBunchElemSize);

		// Just clamp here. Don't go around LastReplicatedImageDataIndex.
		LastReplicatedImageDataIndex = OutReplicateInfo.UpdateBeginIndex + OutReplicateInfo.UpdateElemNum;

		const int32 RepImageIndexOver = LastReplicatedImageDataIndex - SourceImageData.Num() + 1;
		if (RepImageIndexOver > 0)
		{
			LastReplicatedImageDataIndex -= RepImageIndexOver;
			OutReplicateInfo.UpdateElemNum -= RepImageIndexOver;
		}

		for (int32 RDI = 0; RDI < OutReplicateInfo.UpdateElemNum; ++RDI)
		{
			const int32 SDI = OutReplicateInfo.UpdateBeginIndex + RDI;

			const FColor SrcColor = SourceImageData[SDI];
			FWendyReplicatedColor& RepColorRef = OutReplicateInfo.ImageData[RDI];

			RepColorRef.R = SrcColor.R;
			RepColorRef.G = SrcColor.G;
			RepColorRef.B = SrcColor.B;
		}

		//bDirtyForReplication = false;
	}
}

void UWendyDesktopImageComponent::SetFromReplicateInfo(const FWendyDesktopImageReplicateInfo& InReplicateInfo)
{
	const int32 FinalReplicatDataNum = FMath::Min(InReplicateInfo.ImageData.Num(), InReplicateInfo.UpdateElemNum);

	for (int32 RDI = 0; RDI < FinalReplicatDataNum; ++RDI)
	{
		const int32 SourceImageDataIdx = InReplicateInfo.UpdateBeginIndex + RDI;
		if (SourceImageData.IsValidIndex(SourceImageDataIdx))
		{
			const FWendyReplicatedColor RepColor = InReplicateInfo.ImageData[RDI];
			FColor& DataRef = SourceImageData[SourceImageDataIdx];
			DataRef.R = RepColor.R;
			DataRef.G = RepColor.G;
			DataRef.B = RepColor.B;
		}
	}
	if (FinalReplicatDataNum > 0)
	{
		bDirtyForTextureUpdate = true;
	}

	// Might call UpdateOutputTexture here? Let it be done at other unified place..
	//UpdateOutputTexture();
}

bool UWendyDesktopImageComponent::ShouldCaptureLocalImage() const
{
	AWendyCharacter* OwnerAsWC = Cast<AWendyCharacter>(GetOwner());
	if (OwnerAsWC != nullptr)
	{
		return OwnerAsWC->IsLocallyControlled();
	}

	// For any case that we should handle types other than AWendyCharacter,
	// Pawn can be judged in the same way as WendyCharacter.
	// Other than that it can be judged by Role of Actor. Only ROLE_AutonomousProxy return true for this.
	// In fact, we can simply check whether owner role is AutonomousProxy from the first place.

	return false;
}

void UWendyDesktopImageComponent::GetPrimaryMonitorResolution(int32& OutWidth, int32& OutHeight, bool bPhysical)
{
	// GetSystemMetrics is still not the right answer when display is scaled.
	OutWidth = ::GetSystemMetrics(SM_CXSCREEN);
	OutHeight = ::GetSystemMetrics(SM_CYSCREEN);

	if (bPhysical)
	{
		// Below is needed for getting device resolution when display is scaled.
		DEVMODE devMode = {};
		devMode.dmSize = sizeof(DEVMODE);
		if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode))
		{
			OutWidth = devMode.dmPelsWidth;
			OutHeight = devMode.dmPelsHeight;
		}
	}
}

FIntPoint UWendyDesktopImageComponent::GetCachedDesktopResolution() const
{
	// #WendyDealsWithPrimaryDisplayOnly
	// Not considering multiple monitors here. Any though on handling such case? Probably very far..
	return FIntPoint(CachedDisplayMetrics.PrimaryDisplayWidth, CachedDisplayMetrics.PrimaryDisplayHeight);
}

FVector2D UWendyDesktopImageComponent::GetDesktopImageStagingStride() const
{
	// Here need values in floating point..
	FVector2D DesktopResolutionV(GetCachedDesktopResolution());
	FVector2D InternalImageResolutionV(GetWendyDesktopImageSize());

	return DesktopResolutionV / InternalImageResolutionV;
}