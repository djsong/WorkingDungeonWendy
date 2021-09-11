// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"
#include "WendyUiWidget.h"
#include "WendyUINameTag.generated.h"

class UWdTextBlock;

/** Displaying panel that your name above your character's head, you know. */
UCLASS(Blueprintable)
class UWendyUINameTag : public UWendyUiWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient, meta=(BindWidget))
	UWdTextBlock* TB_UserId;


public:
	UWendyUINameTag(const FObjectInitializer& ObjectInitializer);

	virtual void StaticWidgetPreparations() override;

	void SetUserId(const FString& InUserId);
};

