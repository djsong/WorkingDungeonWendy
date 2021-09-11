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