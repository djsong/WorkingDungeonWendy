// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyHudUI.h"
#include "Components/EditableTextBox.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyExtendedWidgets.h"
#include "WendyCharacter.h"

UWendyHudUI::UWendyHudUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

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

