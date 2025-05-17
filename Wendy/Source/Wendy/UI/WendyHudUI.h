// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"
#include "WendyUiWidget.h"
#include "WendyHudUI.generated.h"

class UWdButton;
class UWdTextBlock;
class UEditableTextBox;

UCLASS(Blueprintable)
class UWendyHudUI : public UWendyUiWidget
{
	GENERATED_BODY()

protected:

	UPROPERTY(Transient, meta = (BindWidget))
	UEditableTextBox* ET_ChatMessage;

public:
	UWendyHudUI(const FObjectInitializer& ObjectInitializer);

	virtual void NativeDestruct() override;
	virtual void StaticWidgetPreparations() override;

	UFUNCTION()
	void OnChatMessageCommitted(const FText& InText, ETextCommit::Type InCommitMethod);
};

