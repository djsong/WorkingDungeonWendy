// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyHudUI.h"
#include "Components/EditableTextBox.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyDungeonPlayerController.h"
#include "WendyExtendedWidgets.h"
#include "WendyCharacter.h"

UWendyHudUI::UWendyHudUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UWendyHudUI::NativeConstruct()
{
	Super::NativeConstruct();
	AWendyDungeonPlayerController* LocalWdPC = Cast<AWendyDungeonPlayerController>(UWdGameplayStatics::GetLocalPlayerController(this));
	if (IsValid(LocalWdPC))
	{
		LocalWdPC->OnExploringInputModeEvent.AddDynamic(this, &UWendyHudUI::OnWdPcExploringInputModeEvent);
		LocalWdPC->OnUIFocusingInputModeEvent.AddDynamic(this, &UWendyHudUI::OnWdPcUIFocusingInputModeEvent);
	}
}

void UWendyHudUI::NativeDestruct()
{
	Super::NativeDestruct();

	if (IsValid(ET_ChatMessage))
	{
		ET_ChatMessage->OnTextCommitted.RemoveAll(this);
	}
}

void UWendyHudUI::StaticWidgetPreparations()
{
	if (IsValid(ET_ChatMessage))
	{
		ET_ChatMessage->OnTextCommitted.AddDynamic(this, &UWendyHudUI::OnChatMessageCommitted);
	}

	if (IsValid(BTN_BackToExploring))
	{
		BTN_BackToExploring->OnClicked.AddDynamic(this, &UWendyHudUI::OnBackToExploringButtonClicked);
	}
}

void UWendyHudUI::OnChatMessageCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	if (InCommitMethod == ETextCommit::OnEnter)
	{
		UE_LOG(LogWendy, Log, TEXT("Chat message committed : %s"), *InText.ToString());

		AWendyCharacter* LocalControlledChar = UWdGameplayStatics::GetLocalPlayerCharacter(this);
		if (IsValid(LocalControlledChar))
		{
			LocalControlledChar->AddNewChatMessage(InText.ToString());
		}

		// Sent it then clear..
		if (IsValid(ET_ChatMessage))
		{
			ET_ChatMessage->SetText(FText::FromString(TEXT("")));
		}
	}
}

void UWendyHudUI::OnBackToExploringButtonClicked()
{
	AWendyDungeonPlayerController* LocalWdPC = Cast<AWendyDungeonPlayerController>(UWdGameplayStatics::GetLocalPlayerController(this));
	if (IsValid(LocalWdPC))
	{
		LocalWdPC->ConditionalLeaveFocusMode();
	}
}

void UWendyHudUI::OnWdPcExploringInputModeEvent()
{
	if (IsValid(BTN_BackToExploring))
	{
		BTN_BackToExploring->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UWendyHudUI::OnWdPcUIFocusingInputModeEvent()
{
	if (IsValid(BTN_BackToExploring))
	{
		BTN_BackToExploring->SetVisibility(ESlateVisibility::Visible);
	}
}