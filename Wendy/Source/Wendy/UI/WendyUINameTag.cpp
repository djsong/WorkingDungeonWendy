// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyUINameTag.h"
#include "WendyExtendedWidgets.h"

UWendyUINameTag::UWendyUINameTag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UWendyUINameTag::StaticWidgetPreparations()
{
	if (TB_UserId != nullptr)
	{
		// Put some like this as default?
		//TB_UserId->SetText(FText::FromString(TEXT("Unnamed Guest"));
	}
}

void UWendyUINameTag::SetUserId(const FString& InUserId)
{
	if (TB_UserId != nullptr)
	{
		TB_UserId->SetText(FText::FromString(InUserId));
	}
}

void UWendyUINameTag::SetChatMessage(const TArray<FString>& InChatMessages)
{
	if (TB_ChatMessage != nullptr)
	{
		FString ChatMessageSingleString;
		for (int32 MI = 0; MI < InChatMessages.Num(); ++MI)
		{
			const FString& ChtMsg = InChatMessages[MI];
			ChatMessageSingleString += ChtMsg;
			if (MI < InChatMessages.Num() - 1)
			{
				ChatMessageSingleString += TEXT("\r\n");
			}
		}

		TB_ChatMessage->SetText(FText::FromString(ChatMessageSingleString));
	}
}