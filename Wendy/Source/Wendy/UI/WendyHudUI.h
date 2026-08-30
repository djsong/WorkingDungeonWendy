// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"
#include "WendyUiWidget.h"
#include "WendyHudUI.generated.h"

class UWdButton;
class UWdTextBlock;
class UEditableTextBox;
class UImage;

UCLASS(Blueprintable)
class UWendyHudUI : public UWendyUiWidget
{
	GENERATED_BODY()

protected:

	UPROPERTY(Transient, meta = (BindWidget))
	UEditableTextBox* ET_ChatMessage = nullptr;

	UPROPERTY(Transient, meta = (BindWidget))
	UWdButton* BTN_BackToExploring = nullptr;

	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_FocusModeMessage = nullptr;

	UPROPERTY(Transient, meta = (BindWidget))
	UImage* IMG_MicState = nullptr;

	UPROPERTY(Transient, meta = (BindWidget))
	UImage* IMG_SpeakerState = nullptr;

	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_MicDevice = nullptr;

	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_SpeakerDevice = nullptr;

	UPROPERTY(EditAnywhere, Category="WendyHudUI")
	FColor VoiceChatDeviceColor_On = FColor::Green;

	UPROPERTY(EditAnywhere, Category = "WendyHudUI")
	FColor VoiceChatDeviceColor_Off = FColor::Red;
	
	double LastVoiceChatDeviceStateUpateTime = 0.0;

public:
	UWendyHudUI(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void StaticWidgetPreparations() override;

	UFUNCTION()
	void OnChatMessageCommitted(const FText& InText, ETextCommit::Type InCommitMethod);
	UFUNCTION()
	void OnBackToExploringButtonClicked();

	/** Not the event from this UI, from outside object. */
	UFUNCTION()
	void OnWdPcExploringInputModeEvent();
	UFUNCTION()
	void OnWdPcUIFocusingInputModeEvent();
	UFUNCTION()
	void OnWdDesktopFocusingInputModeEvent();

	void UpdateFocusModeMessage();

	void UpdateVoiceChatDeviceState();
};

