// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyCommon.h"
#include "WendyUiWidget.h"
#include "WendyLobbyUI.generated.h"

class UWdButton;
class UWdTextBlock;
class UEditableTextBox;
class UCheckBox;

UCLASS(Blueprintable)
class UWendyLobbyUI : public UWendyUiWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_UserIdTitle;
	UPROPERTY(Transient, meta = (BindWidget))
	UEditableTextBox* ET_UserId;

	/** Yes, you need to type IP address.. haha */
	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_ServerAddrTitle;
	UPROPERTY(Transient, meta = (BindWidget))
	UEditableTextBox* ET_ServerAddr;

	/** In this application, you should explicitly choose whether to be a server. */
	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_IamServerTitle;
	UPROPERTY(Transient, meta = (BindWidget))
	UCheckBox* CB_IamServer;

	/** Here you are still not in Wendy world. Let's go. */
	UPROPERTY(Transient, meta = (BindWidget))
	UWdButton* BTN_EnterWendy;
	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_EnterWendyTitle;

public:
	UWendyLobbyUI(const FObjectInitializer& ObjectInitializer);

	virtual void NativeDestruct() override;
	virtual void StaticWidgetPreparations() override;

	UFUNCTION()
	void OnEnterWendyClick();

	UFUNCTION()
	void OnUserIdTyped(const FText& InText);

	/** That checkbox won't always be enabled.. */
	void UpdateIamServerEnableState();

	/** Delegate callback to make runtime switching of a console variable to take effect */
	static void OnWdAllowServerSelectionChanged(IConsoleVariable* CVar);

	/** Gets the user edited data in our desired format. */
	FWendyWorldConnectingInfo GetWendyConnectingInfo() const;
};
