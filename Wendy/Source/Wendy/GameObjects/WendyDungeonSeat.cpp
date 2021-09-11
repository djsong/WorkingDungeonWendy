// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDungeonSeat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WendyCharacter.h"

const FName AWendyDungeonSeat::MonitorMatSlotName(TEXT("WdCapturedDesktop"));
const FName AWendyDungeonSeat::DesktopImageTextureParamName(TEXT("WdCapturedDesktopImage"));

AWendyDungeonSeat::AWendyDungeonSeat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//USceneComponent* DummySceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DummySceneRoot"));
	//RootComponent = DummySceneRoot;

	TableMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMeshComp"));
	//TableMeshComp->SetupAttachment(RootComponent);
	RootComponent = TableMeshComp;
	TableMeshComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TableMeshComp->Mobility = EComponentMobility::Static;
	TableMeshComp->SetGenerateOverlapEvents(false);
	TableMeshComp->bUseDefaultCollision = true;
	

	MonitorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMeshComp"));
	MonitorMeshComp->SetupAttachment(RootComponent);
	MonitorMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	MonitorMeshComp->Mobility = EComponentMobility::Movable;
	//MonitorMeshComp->SetGenerateOverlapEvents(true); // Might someday need overlap check for this?
	
	ChairMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMeshComp"));
	ChairMeshComp->SetupAttachment(RootComponent);
	ChairMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ChairMeshComp->Mobility = EComponentMobility::Movable;

	RelativeOriginComp = CreateDefaultSubobject<UWendyDgSeatOriginPosComp>(TEXT("RelativeOriginComp"));
	RelativeOriginComp->SetupAttachment(RootComponent);

	OffStateTexture = nullptr;
	DesktopImageTexture = nullptr;

}

void AWendyDungeonSeat::BeginPlay()
{
	Super::BeginPlay();

	ConditionalFallbackToOffState(); //Is it so necessary?
}

void AWendyDungeonSeat::SetDesktopImageTexture(UTexture2D* InTexture)
{
	DesktopImageTexture = InTexture;

	if (DesktopImageTexture.IsValid())
	{
		SetMonitorTextureInternal(DesktopImageTexture.Get());
	}
}

void AWendyDungeonSeat::FallbackToOffState()
{
	// @TODO Wendy DungeonSeat
	// If This has connection to the wendy character, need to null that out too.
	// This is intended to be called when the character left the world.

	DesktopImageTexture.Reset();

	if (OffStateTexture != nullptr)
	{
		SetMonitorTextureInternal(OffStateTexture);
	}
}

void AWendyDungeonSeat::ConditionalFallbackToOffState()
{
	// If This has connection to the wendy character, check that out too.

	if (DesktopImageTexture.IsValid() == false)
	{
		FallbackToOffState();
	}
}

void AWendyDungeonSeat::SetMonitorTextureInternal(UTexture2D* InTexture)
{
	if (MonitorMeshComp != nullptr && InTexture != nullptr)
	{
		int32 MatSlotIndexOfDesktopImage = MonitorMeshComp->GetMaterialIndex(MonitorMatSlotName);
		if (MatSlotIndexOfDesktopImage == INDEX_NONE)
		{
			// Might not have a chance to set the mat slot name.
			// so better have fallback index.. but could be all slots in this case..
			MatSlotIndexOfDesktopImage = 0; 
		}

		UMaterialInstanceDynamic* MID = MonitorMeshComp->CreateAndSetMaterialInstanceDynamic(MatSlotIndexOfDesktopImage);
		if (MID != nullptr)
		{
			MID->SetTextureParameterValue(DesktopImageTextureParamName, InTexture);
		}
	}
}

FVector AWendyDungeonSeat::GetOriginPos() const
{
	// This is what RelativeOriginComp exists for.
	return (RelativeOriginComp != nullptr) ? RelativeOriginComp->GetComponentLocation() : GetActorLocation();
}

FRotator AWendyDungeonSeat::GetOriginRot() const
{
	// This is what RelativeOriginComp exists for.
	return (RelativeOriginComp != nullptr) ? RelativeOriginComp->GetComponentRotation() : GetActorRotation();
}

//bool AWendyDungeonSeat::IsOccupied() const
//{
//
//}

/////////////////////////////////////////////////////////////////

UWendyDgSeatOriginPosComp::UWendyDgSeatOriginPosComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}