// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyDungeonPlayerController.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "WendyDataStore.h"
#include "WendyUIDungeonSeatSelection.h"

AWendyDungeonPlayerController::AWendyDungeonPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AWendyDungeonPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Not setting the input mode at here BeginPlay
	// WendyDungeonGameMode(or state) has its own plan for this.
	//SetInputMode(FInputModeGameAndUI());

}

void AWendyDungeonPlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);

	AWendyCharacter* AsWendyChar = Cast<AWendyCharacter>(aPawn);

	if (IsLocalController() && AsWendyChar != nullptr)
	{
		// Assumes that local DungeonPlayerController is created after logging in from lobby UI and entering the wendy world.
		// This is the first step that AccountInfo goes for replication.. to the server (RPC) and to other client (as remote)
		FWendyDataStore& WendyDataStore = GetGlobalWendyDataStore();
		AsWendyChar->SetConnectedUserAccountInfo(WendyDataStore.GetUserAccountInfo());
	}
}