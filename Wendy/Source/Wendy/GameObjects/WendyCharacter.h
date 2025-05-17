// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WendyCommon.h"
#include "WendyCharacter.generated.h"

class AWendyDungeonSeat;
class UWidgetComponent;
class UWendyDesktopImageComponent;


/** Not in byte.. element number. */
int32 GetWdDesktopImageReplicateElemSize();

/** 
 * The actor manifestation of a user in Wendy world.
 * Its specially added feature will be captured desktop image replication,
 * It might further be extended to be a hub to other types of user to user communication.
 *
 * Initially created from ThirdPersion template.
 *	Basic input and movement handling won't be far from the template.
 */
UCLASS(config=Game)
class AWendyCharacter : public ACharacter
{
	GENERATED_BODY()

//=============================================================================================
// Initially given from ThirdPersion template.

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

public:
	AWendyCharacter(const FObjectInitializer& ObjectInitializer);

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

protected:

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

//=============================================================================================
protected:
//=============================================================================================
// Belows are added for Wendy

	/** The on head nametag.. by UWendyUINameTag */
	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* NameTagComp;

	UPROPERTY(VisibleAnywhere)
	UWendyDesktopImageComponent* DesktopImageComponent;
	
	/** To be assigned runtime. No attachment relation with this.
	 * It is where the captured desktop image finally goes. */
	UPROPERTY()
	AWendyDungeonSeat* HomeSeat;

	/** This is actually being set prior to HomeSeat object pointer. 
	 * HomeSeat is searched based on this variable.
	 * @TODO Wendy HomeSeat: Might better have more robust seat identification. */
	UPROPERTY(ReplicatedUsing = OnRep_PickedHomeSeatPosition)
	FVector2D PickedHomeSeatPosition;

	/** AccountInfo of the user represented by this character, that you should type in at lobby to get in the world. 
	 * Only local controlled character has this the same as what DataStore holds. */
	UPROPERTY(ReplicatedUsing = OnRep_ConnectedUserAccountInfo)
	FWendyAccountInfo ConnectedUserAccountInfo;

	/** Chat messages that older message get pushed behind. */
	UPROPERTY(ReplicatedUsing = OnRep_ChatMessages)
	TArray<FString> ChatMessages;

	/** There's always BeginPlay timing issue for dynamically spawned object, 
	 * trying to solve it by some naive measure. */
	FTimerHandle DeferredBeginPlayHandlingTH;

	/** To control the interval that the captured data goes to server. It cannot be every time if single sending bunch is big.. */
	double LastDesktopImageRPCCallTime;
	bool bDesktopImageRPCCallLastTick; // At least not every tick
	/** Pretty much like LastDesktopImageRPCCallTime.. could it be a bit different? */
	double LastDesktopImageRepDirtyTime;
	bool bDesktopImageRepDirtyLastTick; // At least not every tick

	/** Especially when a remote client gets replicated PickedHomeSeatPosition, its actual homeseat finding process need to be deferred 
	 * due to other component and resouce generation process. */
	FTimerHandle DeferredFindAndCacheHomeSeatTH;
	FTimerHandle DeferredSetAccountInfoUITH; // Pretty much the same story here.
	FTimerHandle CirculateChatMessagesTH;

	/////////////////////////
	// Inherited..
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/////////////////////////

	/** There's always BeginPlay timing issue for dynamically spawned object,
	 * trying to solve it by some naive measure.
	 * Put anything that you would want to be done at BeginPlay but need be deferred because of other object's dependency. 
	 * How much it gets deferred? Just some amount.. */
	void DeferredBeginPlayHandling();
		
	/** A character is actually ready to work in this dungeon after gone througg this. */
	bool FindAndCacheHomeSeat();
	/** Sometime the timing for FindAndCacheHomeSeat is too early so make it done later.. */
	void DeferredFindAndCacheHomeSeat();

	/** To show who I am.. haha */
	void SetAccountInfoUI();
	/** Same old same old now.. */
	void DeferredSetAccountInfoUI();
	void SetupDeferredSetAccountInfoUITimer();

	/** Main for overall captured desktop image processing, mostly about replication management */
	void UpdateDesktopImageReplication();

	void RemoveSelfFromImageRepNetwork();

	/////////////////////////
	//>> Replication & RPC..
	UFUNCTION()
	void OnRep_PickedHomeSeatPosition();
	/** It is determined from clinet (autonomous proxy) side then sent to server for replication to other clients. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetPickedHomeSeatPosition(FVector2D InPickedPos);

	UFUNCTION()
	void OnRep_ConnectedUserAccountInfo();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetConnectedUserAccountInfo(FWendyAccountInfo InAccountInfo);

	UFUNCTION()
	void OnRep_ChatMessages();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetChatMessages(const TArray<FString>& InChatMessages);
	//<< Replication & RPC..
	/////////////////////////

	/** 
	 * It is NOT about whether the ReplicatedDesktopImage is ready to be replicated,
	 * even when that is ready for the replication, there could be additional condition 
	 * according to the actor relation or other circumstances like optimization..
	 */
	bool ShouldFinallyReplicateCaptauredImage() const;

	void ZoomCameraView(float InAmount);

public:
	/** Being called for the local user, when the user picked its seat from the UI. 
	 * After this, other remotes get their seat position by replication. */
	bool SetPickedHomeSeatPosition(FVector2D InPickedPosition);
	
	/** Like the story of other replicated data, this is the very first place being set for the local character
	 * before all the replications happen. But this case is simpler. */
	void SetConnectedUserAccountInfo(FWendyAccountInfo InAccountInfo);

	void AddNewChatMessage(const FString& InNewMessage);
	/** Circulate by time.. eventually remove old messages */
	void CirculateChatMessages();
	void SetChatMessagesCirculationTimer();

	/** For final display */
	void UpdateChatMessageUI();

	FORCEINLINE UWendyDesktopImageComponent* GetDesktopImageComponent() const { return DesktopImageComponent; }

	/** To enable going back at any case? */
	const static float CameraBoomTargetArmLengthDefault;
};

