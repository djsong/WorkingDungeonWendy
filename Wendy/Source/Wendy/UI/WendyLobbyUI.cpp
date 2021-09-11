// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyLobbyUI.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyExtendedWidgets.h"
#include "WendyGameSettings.h"


static TAutoConsoleVariable<int32> CVarWdAllowServerSelection(
	TEXT("wd.AllowServerSelection"),
	0,
	TEXT("It allowed to choose being a server by enabling specific checkbox at Lobby UI. Mostly for authoritative."),
	ECVF_Default);

const FString GetFallbackServerIpString()
{
	const UWendyGameSettings* WdGameSettings = GetDefault<UWendyGameSettings>(UWendyGameSettings::StaticClass());
	if (WdGameSettings != nullptr && WdGameSettings->DefaultServerIpAddress.Len() > 0)
	{
		return WdGameSettings->DefaultServerIpAddress;
	}
	return TEXT("127.0.0.1"); // Loopback is a natural choice for fallback..
}

UWendyLobbyUI::UWendyLobbyUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	static bool bCVarCBRegistered = false;
	if (bCVarCBRegistered == false)
	{
		IConsoleVariable* WdAllowServerSelectionPtr = CVarWdAllowServerSelection->AsVariable();
		if (WdAllowServerSelectionPtr != nullptr)
		{
			WdAllowServerSelectionPtr->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&UWendyLobbyUI::OnWdAllowServerSelectionChanged));

			bCVarCBRegistered = true;
		}
	}
}

void UWendyLobbyUI::StaticWidgetPreparations()
{
	// @TODO Wendy LOCTEXT
	if (TB_UserIdTitle != nullptr)
	{
		//TB_UserIdTitle->SetText();
	}
	if (TB_ServerAddrTitle != nullptr)
	{
		//TB_ServerAddrTitle->SetText();
	}
	if (TB_EnterWendyTitle != nullptr)
	{
		//TB_EnterWendyTitle->SetText();
	}
	if (ET_UserId != nullptr)
	{
		//ET_UserId->SetHintText();
	}
	if (ET_ServerAddr != nullptr)
	{
		ET_ServerAddr->SetHintText(FText::FromString(GetFallbackServerIpString()));
	}
	if (TB_IamServerTitle != nullptr)
	{
		//TB_IamServerTitle->SetText();
	}
	UpdateIamServerEnableState();

	if (BTN_EnterWendy != nullptr)
	{
		BTN_EnterWendy->OnClicked.AddDynamic(this, &UWendyLobbyUI::OnEnterWendyClick);
	}
}

void UWendyLobbyUI::OnEnterWendyClick()
{
	FWendyWorldConnectingInfo ConnectingInfo = GetWendyConnectingInfo();

	if (ConnectingInfo.IsValid())
	{
		UWdGameplayStatics::EnterWendyWorld(this, ConnectingInfo);
	}
	else
	{

		// Might pop-up some?

	}
}

void UWendyLobbyUI::UpdateIamServerEnableState()
{
	// Command line argument is also possible.
	const bool bShouldEnable = (CVarWdAllowServerSelection.GetValueOnAnyThread() > 0) || FParse::Param(FCommandLine::Get(), TEXT("AllowServerSelection"));
	if (CB_IamServer != nullptr)
	{
		CB_IamServer->SetIsEnabled(bShouldEnable);
	}
	if (TB_IamServerTitle != nullptr)
	{
		TB_IamServerTitle->SetOpacity(bShouldEnable ? 1.0f : 0.4f);
	}
}

void UWendyLobbyUI::OnWdAllowServerSelectionChanged(IConsoleVariable* CVar)
{
	for (TObjectIterator<UWendyLobbyUI> Itt; Itt; ++Itt)
	{
		UWendyLobbyUI* AsWLUI = Cast<UWendyLobbyUI>(*Itt);
		if (AsWLUI != nullptr && AsWLUI->HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			AsWLUI->UpdateIamServerEnableState();
		}
	}
}

FWendyWorldConnectingInfo UWendyLobbyUI::GetWendyConnectingInfo() const
{
	FWendyWorldConnectingInfo RetInfo;

	// If that checkbox is not enabled, it is not allowed to be a server.
	RetInfo.bMyselfServer = (CB_IamServer != nullptr && CB_IamServer->GetIsEnabled()) ?
		(CB_IamServer->GetCheckedState() == ECheckBoxState::Checked) : false;

	if (ET_UserId != nullptr)
	{
		RetInfo.UserId = ET_UserId->GetText().ToString();
	}
	if (ET_ServerAddr != nullptr && RetInfo.bMyselfServer == false)
	{
		RetInfo.ServerIp = ET_ServerAddr->GetText().ToString();
		if (RetInfo.ServerIp.Len() == 0)
		{
			// If not specified, goes for the default, that being displayed as a hint.
			RetInfo.ServerIp = GetFallbackServerIpString();
		}
	}

	return RetInfo;
}