// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyCharacter.h"
//#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "WdGameplayStatics.h"
#include "WendyDataStore.h"
#include "WendyDesktopImageComponent.h"
#include "WendyDungeonSeat.h"
#include "WendyUINameTag.h"
#include "WendyImageRepPackets.h"
#include "WendyGameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogWendyCharacter, Log, All);

static TAutoConsoleVariable<int32> CVarWdDesktopImageReplicateSize(
	TEXT("wd.DesktopImageReplicateSize"),
	WENDY_IMAGE_PACKET_DATA_ARRAY_SIZE,
	TEXT("It could be bigger if we upgrade rep inmage networking, until then don't go over certain limit."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarWdDesktopImageReplicateBunchNum(
	TEXT("wd.DesktopImageReplicateBunchNum"),
	100,
	TEXT("It directly related to image replication performance. Instead of increasing wd.DesktopImageReplicateSize, you increase this number."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdDesktopImageRepMaxSizeForEveryTick(
	TEXT("wd.DesktopImageRepMaxSizeForEveryTick"),
	20000,
	TEXT("The maximum size that replication or RPC call can happen every tick.")
	TEXT("If it is too big then transferring every tick can make other replication job unstable"),
	ECVF_ReadOnly);

// DesktopImageRPCInterval and DesktopImageRepInterval are old terms, when image transferring relied on unreal networking,
// which is not anymore but the setting still works in whatever way.
static TAutoConsoleVariable<float> CVarWdDesktopImageRPCInterval(
	TEXT("wd.DesktopImageRPCInterval"),
	0.01f,
	TEXT("The basic time interval that capture image sending RPC call from clinet to server.")
	TEXT("Small (frequent) interval is fine if DesktopImageReplicateSize is small, but should give enough term if that becomes larger."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarWdDesktopImageRepInterval(
	TEXT("wd.DesktopImageRepInterval"),
	0.01f,
	TEXT("The basic time interval that capture image being replicate call from server to client. ")
	TEXT("Small (frequent) interval is fine if DesktopImageReplicateSize is small, but should give enough term if that becomes larger."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWdChatMessageMaxNum(
	TEXT("wd.ChatMessageMaxNum"),
	5,
	TEXT("Maximum line of chat messages, to be replicated and displayed.")
	TEXT("Increasing this number doesn't necessarly mean that whole messages get displayed because of UI."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarWdChatMessageCirculatePeriod(
	TEXT("wd.ChatMessageCirculatePeriod"),
	30.0f,
	TEXT("Message line circulation time in second. This time multiplies wd.ChatMessageMaxNum will be message life span (provided there's no new message).")
	TEXT(""),
	ECVF_Default);

/** Access these variables by these functions instead of referring variables directly. 
 * Probably considering a possibility of changing there declaration type like ini? */
int32 GetWdDesktopImageReplicateElemSize()
{
	return CVarWdDesktopImageReplicateSize.GetValueOnAnyThread();
}
int32 GetWdDesktopImageRepMaxSizeForEveryTick()
{
	return CVarWdDesktopImageRepMaxSizeForEveryTick.GetValueOnAnyThread();
}

float GetWdDesktopImageRPCInterval()
{
	return CVarWdDesktopImageRPCInterval.GetValueOnAnyThread();
}

float GetWdDesktopImageRepInterval()
{
	return CVarWdDesktopImageRepInterval.GetValueOnAnyThread();
}

int32 GetWdChatMessageMaxNum()
{
	return CVarWdChatMessageMaxNum.GetValueOnAnyThread();
}

float GetWdChatMessageCirculatePeriod()
{
	return CVarWdChatMessageCirculatePeriod.GetValueOnAnyThread();
}

//////////////////////////////////////////////////////////////////////////
// AWendyCharacter

const float AWendyCharacter::CameraBoomTargetArmLengthDefault = 300.0f;
AWendyCharacter::AWendyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(25.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f)); // Let it more like first person cam if zoomed in enough.
	CameraBoom->TargetArmLength = CameraBoomTargetArmLengthDefault; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	FollowCamera->AddRelativeLocation(FVector(0,0,80.0f));
	//FollowCamera->AddRelativeRotation(FRotator(-10.0f, 0, 0));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

//=============================================================================================
// Belows are added for Wendy
	NameTagComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameTagComp"));
//	NameTagComp->SetWidgetSpace(EWidgetSpace::Screen); // I have no idea what is wrong.. it just doesn't work well in server side.
	NameTagComp->SetupAttachment(RootComponent);
	NameTagComp->AddRelativeLocation(FVector(0, 0, 100.0f));
	NameTagComp->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
	NameTagComp->SetDrawSize(FVector2D(1000.0f, 200.0f));
	NameTagComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DesktopImageComponent = CreateDefaultSubobject<UWendyDesktopImageComponent>(TEXT("DesktopImageComponent"));

	HomeSeat = nullptr;
	PickedHomeSeatPosition = FVector2D::ZeroVector;

	LastDesktopImageRPCCallTime = 0.0;
	bDesktopImageRPCCallLastTick = false;
	LastDesktopImageRepDirtyTime = 0.0;
	bDesktopImageRepDirtyLastTick = false;
//=============================================================================================
}

//////////////////////////////////////////////////////////////////////////
// Input

void AWendyCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AWendyCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AWendyCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AWendyCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AWendyCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AWendyCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AWendyCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AWendyCharacter::OnResetVR);

//=============================================================================================
// Belows are added for Wendy
	PlayerInputComponent->BindAxis("ZoomCameraView", this, &AWendyCharacter::ZoomCameraView);
//=============================================================================================
}


void AWendyCharacter::OnResetVR()
{
	// If Wendy is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in Wendy.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	//UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AWendyCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AWendyCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AWendyCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AWendyCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AWendyCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AWendyCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

//=============================================================================================

void AWendyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateDesktopImageReplication();
}

void AWendyCharacter::BeginPlay()
{
	Super::BeginPlay();

	UWorld* OwnerWorld = GetWorld();
	if (IsValid(OwnerWorld))
	{
		OwnerWorld->GetTimerManager().SetTimer(DeferredBeginPlayHandlingTH, this, &AWendyCharacter::DeferredBeginPlayHandling, 0.5f);
	}
	SetChatMessagesCirculationTimer();
}

void AWendyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME(AWendyCharacter, ReplicatedDesktopImage);
	//FDoRepLifetimeParams ReplicatedImageParams{ COND_None, REPNOTIFY_Always, /*bIsPushBased=*/true };
	//DOREPLIFETIME_WITH_PARAMS(AWendyCharacter, ReplicatedDesktopImage, ReplicatedImageParams);

	DOREPLIFETIME(AWendyCharacter, PickedHomeSeatPosition);

	DOREPLIFETIME(AWendyCharacter, ConnectedUserAccountInfo);

	DOREPLIFETIME(AWendyCharacter, ChatMessages);
}

void AWendyCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Some like this..
	//DOREPLIFETIME_ACTIVE_OVERRIDE(AWendyCharacter, ReplicatedDesktopImage, DesktopImageReplicateCondition(0));
}

void AWendyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	RemoveSelfFromImageRepNetwork();
}

void AWendyCharacter::DeferredBeginPlayHandling()
{
	UWorld* OwnerWorld = GetWorld();
	if (OwnerWorld != nullptr)
	{
		OwnerWorld->GetTimerManager().ClearTimer(DeferredBeginPlayHandlingTH);
	}

	if (IsLocallyControlled())
	{
		// I have no idea what is wrong.. This is done at other place when WendyDungeonPlayerController possesses a pawn,
		// but what's wrong with client?
		FWendyDataStore& WendyDataStore = GetGlobalWendyDataStore();
		SetConnectedUserAccountInfo(WendyDataStore.GetUserAccountInfo());
	}
}

bool AWendyCharacter::FindAndCacheHomeSeat()
{
	bool bRetVal = false;

	// Should find according to the chosen index and coordinate..

	// but before that..

	UWorld* OwnerWorld = GetWorld();
	if (IsValid(OwnerWorld))
	{
		UTexture2D* DesktopImageTexture = (DesktopImageComponent != nullptr) ? DesktopImageComponent->GetOutputTexture() : nullptr;

		// @TODO Wendy HomeSeat : I know that this is quite hacky. Just wanted to make it quick.
		AWendyDungeonSeat* FoundClosest = nullptr;
		float ClosestDistSqSoFar = FLT_MAX;
		for (FActorIterator ItActor(OwnerWorld); ItActor; ++ItActor)
		{
			AWendyDungeonSeat* AsWDS = Cast<AWendyDungeonSeat>(*ItActor);
			if (IsValid(AsWDS) && AsWDS->IsOccupied() == false)
			{
				// Not the GetActorLocation. There's other location mark for this purpose.
				FVector SeatOriginPos = AsWDS->GetOriginPos();

				const float CurrDistSQ = FVector2D::DistSquared(PickedHomeSeatPosition, FVector2D(SeatOriginPos.X, SeatOriginPos.Y));
				if (CurrDistSQ < ClosestDistSqSoFar)
				{
					ClosestDistSqSoFar = CurrDistSQ;
					FoundClosest = AsWDS;
				}
			}
		}

		if (IsValid(FoundClosest))
		{
			HomeSeat = FoundClosest;
			HomeSeat->SetDesktopImageTexture(DesktopImageTexture);
			HomeSeat->SetOwnerCharacter(this);
			bRetVal = true;

			// I guess setting the location is only server job?
			// Or perhaps for locally controlled ?
			if (GetLocalRole() == ROLE_Authority
				|| IsLocallyControlled() // Because of camera rotation issue, just let the local autonomous proxy set it..
				)
			{
				const float FollowCamYawBefore = FollowCamera->GetComponentRotation().Yaw;

				this->SetActorLocation(HomeSeat->GetOriginPos());
				// To prevent embarassing view.. but we actually need to set the camera location/rotation for this..
				FRotator OriginRot = HomeSeat->GetOriginRot();
				this->SetActorRotation(OriginRot);

				// This is a hack that assuming controller yaw input goes camera rotation.
				const float CamYawDiff = OriginRot.Yaw - FollowCamYawBefore;
				AddControllerYawInput(CamYawDiff * 2.0f); // I just found out that it has to be two multiple, probably internal business.
			}
		}
	}
	return bRetVal;
}

void AWendyCharacter::DeferredFindAndCacheHomeSeat()
{
	UWorld* OwnerWorld = GetWorld();
	if (OwnerWorld != nullptr)
	{
		OwnerWorld->GetTimerManager().ClearTimer(DeferredFindAndCacheHomeSeatTH);
	}

	FindAndCacheHomeSeat();
}

void AWendyCharacter::SetAccountInfoUI()
{
	if (NameTagComp != nullptr)
	{
		UWendyUINameTag* InternalWidget = Cast<UWendyUINameTag>(NameTagComp->GetWidget());
		if (InternalWidget != nullptr)
		{
			InternalWidget->SetUserId(ConnectedUserAccountInfo.UserId);
		}
	}	
}

void AWendyCharacter::DeferredSetAccountInfoUI()
{
	UWorld* OwnerWorld = GetWorld();
	if (OwnerWorld != nullptr)
	{
		OwnerWorld->GetTimerManager().ClearTimer(DeferredSetAccountInfoUITH);
	}

	SetAccountInfoUI();
}

void AWendyCharacter::SetupDeferredSetAccountInfoUITimer()
{
	UWorld* OwnerWorld = GetWorld();
	if (IsValid(OwnerWorld))
	{
		OwnerWorld->GetTimerManager().ClearTimer(DeferredSetAccountInfoUITH);
		OwnerWorld->GetTimerManager().SetTimer(DeferredSetAccountInfoUITH, this, &AWendyCharacter::DeferredSetAccountInfoUI, 0.5f);
	}
}

void AWendyCharacter::UpdateDesktopImageReplication()
{
	if (DesktopImageComponent != nullptr)
	{
		//if (IsLocallyControlled() && ShouldFinallyReplicateCaptauredImage())
		{
			double CurrRT = FPlatformTime::Seconds();

			// @TODO Wendy Network
			// Frequency controlling scheme here is hacky and messy.
			// In fact, I guess it is better be handled by its own networking, instead of Unreal replication.

			// If it is locally controlled.
			// Role could either be ROLE_Authority or ROLE_AutonomousProxy
			if (GetLocalRole() == ROLE_Authority || GetLocalRole() == ROLE_AutonomousProxy 
				|| IsLocallyControlled() //<- This condition might be redundant, but just make things sure.
				)
			{
				// If false, it uses RPC
				// Server if true, Client if false. 
				const bool bUsedReplication= (GetLocalRole() == ROLE_Authority);

				const float RepOrRPCInterval = bUsedReplication ? GetWdDesktopImageRepInterval() : GetWdDesktopImageRPCInterval();
				double& LastRepOrRPCTimeRef = bUsedReplication ? LastDesktopImageRepDirtyTime : LastDesktopImageRPCCallTime;
				bool& bHasProcessedLastTickRef = bUsedReplication ? bDesktopImageRepDirtyLastTick : bDesktopImageRPCCallLastTick;

				if ((CurrRT - LastRepOrRPCTimeRef >= RepOrRPCInterval) && 
					// Don't let it being transferred everyframe anyway if it is considered to be big enough
					// but should we need this condition even thoughimage replication is driven by our own networking?
					// perhaps think about it..
					(GetWdDesktopImageReplicateElemSize() <= GetWdDesktopImageRepMaxSizeForEveryTick() || bHasProcessedLastTickRef == false))
				{
					//if (bUsedReplication)
					{
						{
							// Now we don't do Unreal replication for image data.
							//DesktopImageComponent->ExtractReplicateInfo(ReplicatedDesktopImage);

							// ROLE_Authority won't have to do anymore. It will be replicated thereafter.
						}
						

						// Then now we won't even use Unreal replication for this..
						UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
						if (IsValid(WdGameInst) 
							// UserAccountInfo is being set deferred, should wait until it is set.
							&& (ConnectedUserAccountInfo.UserId.Len() > 0)
							)
						{
							for (int32 RepIdx = 0; RepIdx < CVarWdDesktopImageReplicateBunchNum.GetValueOnGameThread(); ++RepIdx)
							{
								FWendyDesktopImageReplicateInfo LocalReplicateInfo;
								LocalReplicateInfo.ImageData.AddZeroed(GetWdDesktopImageReplicateElemSize());
								DesktopImageComponent->ExtractReplicateInfo(LocalReplicateInfo);
								WdGameInst->SetSendImageInfo(ConnectedUserAccountInfo.UserId, LocalReplicateInfo);
							}
						}
					}

					LastRepOrRPCTimeRef = CurrRT;
					bHasProcessedLastTickRef = true;
				}
				else
				{
					bHasProcessedLastTickRef = false;
				}
			}
		}
		
		if (false == IsLocallyControlled())
		{
			// Consume received image data instead of replication.
			UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this));
			if (IsValid(WdGameInst))
			{
				TArray<FWendyDesktopImageReplicateInfo> ConsumedReplicateInfo;
				WdGameInst->ConsumeImageInfo(ConnectedUserAccountInfo.UserId, ConsumedReplicateInfo);
				for (const FWendyDesktopImageReplicateInfo& ReplicateInfo : ConsumedReplicateInfo)
				{
					DesktopImageComponent->SetFromReplicateInfo(ReplicateInfo);
				}
			}
		}
	}
}

void AWendyCharacter::RemoveSelfFromImageRepNetwork()
{
	// This is needed only for the server side and non locally controlled character
	// Some condition will be checked and filtered inside of mark remove function.

	if (IsLocallyControlled() == false
		
		// This might make the right condition, but hey don't worry here and check it inside of GameInstance.
		//&& (GetLocalRole() == ROLE_Authority)
		)
	{
		// GetWorld might return null at the time it is being called, so try GWorld.
		UWorld* World = GetWorld();
		if (false == IsValid(World))
		{
			World = GWorld;
		}
		if (IsValid(World))
		{
			UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(World));
			if (IsValid(WdGameInst))
			{
				WdGameInst->MarkClientRemoveServerOnly(ConnectedUserAccountInfo.UserId);
			}
		}
	}
}

void AWendyCharacter::OnRep_PickedHomeSeatPosition()
{
	if (IsLocallyControlled() == false)
	{
		// @TODO Wendy HomeSeat : Might better have more robust seat identification.
		// Home seat object selection is not replicated and being searched again in every connected prFindAndCacheHomeSeatocesses.. A bad idea with no doubt, but I haven't had time for figure out better way just yet.

		// And here not even call FindAndCacheHomeSeat right at this moment, because other components might not be ready yet at this point.
		// Replication of other remote characters can happens even before the user choose the home position.
		// While other deferred handling by some sort of delegate call is more desirable, we take a simple approach mostly due to lack of further knowledge and time.

		UWorld* OwnerWorld = GetWorld();
		if (OwnerWorld != nullptr)
		{
			OwnerWorld->GetTimerManager().SetTimer(DeferredFindAndCacheHomeSeatTH, this, &AWendyCharacter::DeferredFindAndCacheHomeSeat, 0.5f);
		}		
	}
}

void AWendyCharacter::ServerSetPickedHomeSeatPosition_Implementation(FVector2D InPickedPos)
{
	PickedHomeSeatPosition = InPickedPos;

	// @TODO Wendy HomeSeat : Might better have more robust seat identification.
	// Home seat object selection is not replicated and being searched again in every connected processes.. A bad idea with no doubt, but I haven't had time for figure out better way just yet.
	FindAndCacheHomeSeat();
}

bool AWendyCharacter::ServerSetPickedHomeSeatPosition_Validate(FVector2D InPickedPos)
{
	return true;
}

void AWendyCharacter::OnRep_ConnectedUserAccountInfo()
{
	if (IsLocallyControlled() == false) // Locally controlled character should already handled this at the very first SetConnectedUserAccountInfo.
	{
		SetupDeferredSetAccountInfoUITimer();
	}
}

void AWendyCharacter::ServerSetConnectedUserAccountInfo_Implementation(FWendyAccountInfo InAccountInfo)
{
	ConnectedUserAccountInfo = InAccountInfo;

	// WidgetComponent might not be ready at this point, so setting up a timer instead of SetAccountInfoUI call right here.
	SetupDeferredSetAccountInfoUITimer();
}

bool AWendyCharacter::ServerSetConnectedUserAccountInfo_Validate(FWendyAccountInfo InAccountInfo)
{
	return true;
}

void AWendyCharacter::OnRep_ChatMessages()
{
	UpdateChatMessageUI();
}

void AWendyCharacter::ServerSetChatMessages_Implementation(const TArray<FString>& InChatMessages)
{
	ChatMessages = InChatMessages;

	UpdateChatMessageUI();
}

bool AWendyCharacter::ServerSetChatMessages_Validate(const TArray<FString>& InChatMessages)
{
	return true;
}

bool AWendyCharacter::ShouldFinallyReplicateCaptauredImage() const
{
	if (GetLocalRole() == ROLE_Authority)
	{
		// @TODO Wendy Replication
		// There might be some case that a character is far away from others so replication is not so needed..
		return true;
	}
	else if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		// AutonomousProxy always send its data to the listen server, that way server is always up to dated
		// Whether to replicate for each client is server's business.

		// In fact, we need some interval check, but it cannot be done here
		// then what is the point of this function?

		// A condition based on distance or relevancy could get into here.


		return true;
	}

	return false;
}

void AWendyCharacter::ZoomCameraView(float InAmount)
{
	if (InAmount != 0.f && IsLocallyControlled())
	{
		if (CameraBoom != nullptr)
		{
			CameraBoom->TargetArmLength = FMath::Clamp(
				CameraBoom->TargetArmLength + (4.0f * InAmount), // Actual scale configuration can be done at DefaultInput.ini
				// Not like infinite..
				CameraBoomTargetArmLengthDefault * 0.1f,
				CameraBoomTargetArmLengthDefault * 2.0f
			);
		}
	}
}

bool AWendyCharacter::SetPickedHomeSeatPosition(FVector2D InPickedPosition)
{
	ensureAlwaysMsgf(IsLocallyControlled(), TEXT("What is happening that non locally controlled character pick its home position directly? for %s, role %d"),
		*GetName(), (int32)GetLocalRole());

	PickedHomeSeatPosition = InPickedPosition;

	// @TODO Wendy HomeSeat : Might better have more robust seat identification.
	// Like all other FindAndCacheHomeSeat call, it might better be done only at the server side with more robust implementation?
	const bool bFoundSeat = FindAndCacheHomeSeat();

	if (bFoundSeat)
	{
		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			// First for for server, then finally for all other clients.
			ServerSetPickedHomeSeatPosition(PickedHomeSeatPosition);
		}
	}
	return bFoundSeat;
}

void AWendyCharacter::SetConnectedUserAccountInfo(FWendyAccountInfo InAccountInfo)
{
	ConnectedUserAccountInfo = InAccountInfo;

	// WidgetComponent might not be ready at this point.
	SetupDeferredSetAccountInfoUITimer();

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		// First for for server, then finally for all other clients.
		ServerSetConnectedUserAccountInfo(ConnectedUserAccountInfo);
	}
}

void AWendyCharacter::AddNewChatMessage(const FString& InNewMessage)
{
	ChatMessages.Insert(InNewMessage, 0);

	const int32 MaxChatMessage = GetWdChatMessageMaxNum();
	if (MaxChatMessage > 0 && ChatMessages.Num() > MaxChatMessage)
	{
		ChatMessages.RemoveAt(MaxChatMessage, ChatMessages.Num() - MaxChatMessage);
	}

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		// First for for server, then finally for all other clients.
		ServerSetChatMessages(ChatMessages);
	}

	// Message has been added in whatever reason, so it has to be counted again.
	SetChatMessagesCirculationTimer();

	UpdateChatMessageUI();
}

void AWendyCharacter::CirculateChatMessages()
{
	// New message goes to the front, so pusing new empty message should do the same.
	AddNewChatMessage(FString(TEXT("")));
}

void AWendyCharacter::SetChatMessagesCirculationTimer()
{
	// Other than local controlled character, chat message will be replicated, even on circulation.
	if (IsLocallyControlled())
	{
		UWorld* OwnerWorld = GetWorld();
		if (IsValid(OwnerWorld))
		{
			OwnerWorld->GetTimerManager().ClearTimer(CirculateChatMessagesTH);
			OwnerWorld->GetTimerManager().SetTimer(CirculateChatMessagesTH, this, &AWendyCharacter::CirculateChatMessages, GetWdChatMessageCirculatePeriod());
		}
	}
}

void AWendyCharacter::UpdateChatMessageUI()
{
	if (IsValid(NameTagComp))
	{
		UWendyUINameTag* InternalWidget = Cast<UWendyUINameTag>(NameTagComp->GetWidget());
		if (IsValid(InternalWidget))
		{
			InternalWidget->SetChatMessage(ChatMessages);
		}
	}

}