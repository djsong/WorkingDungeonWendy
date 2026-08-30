// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyHudUI.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyDungeonPlayerController.h"
#include "WendyExtendedWidgets.h"
#include "WendyCharacter.h"
#include "WendyGameInstance.h"
#include "Kismet/GameplayStatics.h"

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
		LocalWdPC->OnDesktopFocusingInputModeEvent.AddDynamic(this, &UWendyHudUI::OnWdDesktopFocusingInputModeEvent);
	}
}

void UWendyHudUI::NativeDestruct()
{
	if (IsValid(ET_ChatMessage))
	{
		ET_ChatMessage->OnTextCommitted.RemoveAll(this);
	}

	AWendyDungeonPlayerController* LocalWdPC = Cast<AWendyDungeonPlayerController>(UWdGameplayStatics::GetLocalPlayerController(this));
	if (IsValid(LocalWdPC))
	{
		LocalWdPC->OnExploringInputModeEvent.RemoveAll(this);
		LocalWdPC->OnUIFocusingInputModeEvent.RemoveAll(this);
		LocalWdPC->OnDesktopFocusingInputModeEvent.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UWendyHudUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Tried to do this on event time, but why bothers? It won't take perfomance that seriously.
	UpdateFocusModeMessage();

	// Same here, let's don't get bothered much. 
	const double CurrTime = FPlatformTime::Seconds();
	if (CurrTime - LastVoiceChatDeviceStateUpateTime > 1.0)
	{
		UpdateVoiceChatDeviceState();
		LastVoiceChatDeviceStateUpateTime = CurrTime;
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

void UWendyHudUI::OnWdDesktopFocusingInputModeEvent()
{
	if (IsValid(BTN_BackToExploring))
	{
		BTN_BackToExploring->SetVisibility(ESlateVisibility::Visible);
	}
}

void UWendyHudUI::UpdateFocusModeMessage()
{
	if (IsValid(TB_FocusModeMessage))
	{
		AWendyDungeonPlayerController* LocalWdPC = Cast<AWendyDungeonPlayerController>(UWdGameplayStatics::GetLocalPlayerController(this));
		if (IsValid(LocalWdPC) && LocalWdPC->HasAnyFocusHoveredSeat())
		{
			TB_FocusModeMessage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			TB_FocusModeMessage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UWendyHudUI::UpdateVoiceChatDeviceState()
{
	// Null whenever voice chat isn't running (failed to start, or no microphone). Everything below then
	// shows the off state rather than going blank, so "voice is dead" is visible rather than ambiguous.
	FWendyVoiceChat* VoiceChat = nullptr;
	if (UWendyGameInstance* WdGameInst = Cast<UWendyGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		VoiceChat = WdGameInst->GetVoiceChat();
	}

	const bool bMicrophoneActive = (VoiceChat != nullptr) && VoiceChat->IsMicrophoneActive();
	const bool bSpeakerActive = (VoiceChat != nullptr) && VoiceChat->IsSpeakerActive();

	if (IsValid(IMG_MicState))
	{
		// Green whenever your voice would actually reach someone: running, not muted, and (in push-to-talk
		// mode) the key held. So in push-to-talk this lights up exactly while you are transmitting.
		IMG_MicState->SetColorAndOpacity(FLinearColor(bMicrophoneActive ? VoiceChatDeviceColor_On : VoiceChatDeviceColor_Off));
	}
	if (IsValid(IMG_SpeakerState))
	{
		IMG_SpeakerState->SetColorAndOpacity(FLinearColor(bSpeakerActive ? VoiceChatDeviceColor_On : VoiceChatDeviceColor_Off));
	}

	if (IsValid(TB_MicDevice))
	{
		// Resolved once when capture opened - see FWendyVoiceChat::QueryMicrophoneDeviceName for why this is
		// the system default rather than a device the capture reports back.
		const FString MicrophoneDeviceName = (VoiceChat != nullptr)
			? VoiceChat->GetMicrophoneDeviceName() : TEXT("(voice chat not running)");
		TB_MicDevice->SetText(FText::FromString(MicrophoneDeviceName));
	}
	if (IsValid(TB_SpeakerDevice))
	{
		// Asked of the engine every update, so swapping headphones mid-session is reflected.
		TB_SpeakerDevice->SetText(FText::FromString(FWendyVoiceChat::GetPlaybackDeviceName(this)));
	}
}