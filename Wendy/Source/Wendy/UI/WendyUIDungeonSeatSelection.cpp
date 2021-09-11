// Copyright Working Dungeon Wendy, by DJ Song

#include "WendyUIDungeonSeatSelection.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Engine/UserInterfaceSettings.h"
#include "EngineUtils.h"
#include "WdGameplayStatics.h"
#include "WendyCommon.h"
#include "WendyCharacter.h"
#include "WendyDungeonGameState.h"
#include "WendyDungeonPlayerController.h"
#include "WendyDungeonSeat.h"
#include "WendyExtendedWidgets.h"

/** Not so neat, but let's simply define it in hard way.. Better be scaled according to the whole number. */
const FVector2D GDungeonSeatSelectionUIElemRelSize(0.04f, 0.04f);

UWendyUIDungeonSeatSelection::UWendyUIDungeonSeatSelection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TB_MainTitle = nullptr;
	CP_DungeonTopViewField = nullptr;
	TB_GoToSeatTitle = nullptr;
	BTN_GoToSeat = nullptr;
}

void UWendyUIDungeonSeatSelection::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	GenerateSeatSelectionUIs();
}

void UWendyUIDungeonSeatSelection::StaticWidgetPreparations()
{
	// @TODO Wendy LOCTEXT
	if (TB_MainTitle != nullptr)
	{
		//TB_MainTitle->SetText();
	}
	if (TB_GoToSeatTitle != nullptr)
	{
		//TB_GoToSeatTitle->SetText();
	}

	UpdateGoToSeatBTNEnableState();

	if (BTN_GoToSeat != nullptr)
	{
		BTN_GoToSeat->OnClicked.AddDynamic(this, &UWendyUIDungeonSeatSelection::OnGoToSeatClick);
	}
}

void UWendyUIDungeonSeatSelection::GenerateSeatSelectionUIs()
{
	SelectionUIElems.Empty();

	FVector2D SeatAreaMin(FVector2D::ZeroVector);
	FVector2D SeatAreaMax(FVector2D::ZeroVector);
	
	FVector2D CPAreaMin(FVector2D::ZeroVector);
	FVector2D CPAreaMax(FVector2D::ZeroVector);

	const bool bFoundSeatArea = CalculatesWholeSeatBound(SeatAreaMin, SeatAreaMax);
	const bool bFoundCPArea = GetPanelPlacementBound(CPAreaMin, CPAreaMax);
	
	UWorld* OwnerWorld = GetWorld();
	
	if (bFoundSeatArea && bFoundCPArea && OwnerWorld != nullptr && CP_DungeonTopViewField != nullptr)
	{
		// Before goes on, put some slack on seat area, or it would get too tight.
		{
			FVector2D NonSlackSeatAreaMin(SeatAreaMin);
			FVector2D NonSlackSeatAreaMax(SeatAreaMax);
			SeatAreaMax += (NonSlackSeatAreaMax - NonSlackSeatAreaMin) * 0.1f;
			SeatAreaMin += (NonSlackSeatAreaMin - NonSlackSeatAreaMax) * 0.1f;
		}

		const FVector2D SeatAreaCenter = (SeatAreaMin + SeatAreaMax) * 0.5f;
		const FVector2D CPAreaCenter = (CPAreaMin + CPAreaMax) * 0.5f;

		// The found area should be proper then.

		// Scale and offset to map 3D world object onto cavas panel
		FVector2D SeatToCPScale(
			(CPAreaMax.X - CPAreaMin.X) / (SeatAreaMax.X - SeatAreaMin.X),
			(CPAreaMax.Y - CPAreaMin.Y) / (SeatAreaMax.Y - SeatAreaMin.Y)
		);
		// Offse in CP scaled space.
		FVector2D SeatToCPOffset(
			CPAreaCenter - (SeatAreaCenter * SeatToCPScale)
		);

		const FVector2D SingleSeatElemSize(GDungeonSeatSelectionUIElemRelSize * (CPAreaMax - CPAreaMin));

		for (FActorIterator ItActor(OwnerWorld); ItActor; ++ItActor)
		{
			AWendyDungeonSeat* AsWDS = Cast<AWendyDungeonSeat>(*ItActor);
			if (AsWDS != nullptr)
			{
				// Now dynamically generate selection UI elements.

				// Not the GetActorLocation. There's other location mark for this purpose.
				FVector SeatOriginPos = AsWDS->GetOriginPos();
				FVector2D SeatOriginPosXY(SeatOriginPos.X, SeatOriginPos.Y);

				FVector2D CPSpacePos = (SeatOriginPosXY * SeatToCPScale) + SeatToCPOffset;

				FWdSingleDgSeatSelectionUIElem NewElem;
				NewElem.AssociatedSeat = AsWDS;

				NewElem.OuterPanel = NewObject<UCanvasPanel>(this, NAME_None);
				// Canvas panel in canvas panel.
				UCanvasPanelSlot* OuterPanelPlacedSlot = Cast<UCanvasPanelSlot>(CP_DungeonTopViewField->AddChild(NewElem.OuterPanel));
				if (OuterPanelPlacedSlot != nullptr)
				{
					OuterPanelPlacedSlot->SetPosition(CPSpacePos);
					OuterPanelPlacedSlot->SetSize(SingleSeatElemSize);
				}
				NewElem.SelectedStateCB = NewObject<UCheckBox>(this, NAME_None);
				UCanvasPanelSlot* CheckBoxPlacedSlot = Cast<UCanvasPanelSlot>(NewElem.OuterPanel->AddChild(NewElem.SelectedStateCB));
				if (CheckBoxPlacedSlot != nullptr)
				{
					CheckBoxPlacedSlot->SetPosition(FVector2D::ZeroVector);
					CheckBoxPlacedSlot->SetSize(SingleSeatElemSize);

				}
				// It actually need to set image size of style..
				NewElem.SelectedStateCB->WidgetStyle.UncheckedImage.SetImageSize(SingleSeatElemSize);
				NewElem.SelectedStateCB->WidgetStyle.UncheckedHoveredImage.SetImageSize(SingleSeatElemSize);
				NewElem.SelectedStateCB->WidgetStyle.UncheckedPressedImage.SetImageSize(SingleSeatElemSize);
				NewElem.SelectedStateCB->WidgetStyle.CheckedImage.SetImageSize(SingleSeatElemSize);
				NewElem.SelectedStateCB->WidgetStyle.CheckedHoveredImage.SetImageSize(SingleSeatElemSize);
				NewElem.SelectedStateCB->WidgetStyle.CheckedPressedImage.SetImageSize(SingleSeatElemSize);

				NewElem.SelectedStateCB->OnCheckStateChanged.AddDynamic(this, &UWendyUIDungeonSeatSelection::OnAnySelectionCheckStateChanged);

				SelectionUIElems.Add(NewElem);
			}
		}
	}
}

bool UWendyUIDungeonSeatSelection::CalculatesWholeSeatBound(FVector2D& OutMin, FVector2D& OutMax) const
{
	UWorld* OwnerWorld = GetWorld();
	if (OwnerWorld == nullptr)
	{
		return false;
	}

	int32 FoundNum = 0;

	FVector2D LocalMin(FLT_MAX, FLT_MAX);
	FVector2D LocalMax(-FLT_MAX, -FLT_MAX);

	for (FActorIterator ItActor(OwnerWorld); ItActor; ++ItActor)
	{
		AWendyDungeonSeat* AsWDS = Cast<AWendyDungeonSeat>(*ItActor);
		if (AsWDS != nullptr)
		{
			++FoundNum;

			// Not the GetActorLocation. There's other location mark for this purpose.
			FVector SeatOriginPos = AsWDS->GetOriginPos();
			FVector2D SeatOriginPosXY(SeatOriginPos.X, SeatOriginPos.Y);
			
			if (SeatOriginPosXY.X < LocalMin.X)
			{
				LocalMin.X = SeatOriginPosXY.X;
			}
			if (SeatOriginPosXY.Y < LocalMin.Y)
			{
				LocalMin.Y = SeatOriginPosXY.Y;
			}

			if (SeatOriginPosXY.X > LocalMax.X)
			{
				LocalMax.X = SeatOriginPosXY.X;
			}
			if (SeatOriginPosXY.Y > LocalMax.Y)
			{
				LocalMax.Y = SeatOriginPosXY.Y;
			}
		}
	}

	if (FoundNum >= 2 &&
		LocalMax.X > LocalMin.X && 
		LocalMax.Y > LocalMin.Y
		)
	{
		OutMin = LocalMin;
		OutMax = LocalMax;
		return true;
	}
	return false;
}

bool UWendyUIDungeonSeatSelection::GetPanelPlacementBound(FVector2D& OutMin, FVector2D& OutMax) const
{
	if (CP_DungeonTopViewField != nullptr)
	{
		//Here, this canvas panel should be in another bigger canvas panel.
		UCanvasPanelSlot* OuterCPS = Cast<UCanvasPanelSlot>(CP_DungeonTopViewField->Slot);
		checkf(OuterCPS != nullptr, TEXT("CP_DungeonTopViewField itself should be in another canvas panel of view size"));

		if (OuterCPS->GetSize().Size() > 0.0f)
		{
			// Assuming left upper anchor..
			OutMin = FVector2D(0.0f, 0.0f);
			OutMax = OuterCPS->GetSize();

			return true;
		}
	}
	return false;
}

void UWendyUIDungeonSeatSelection::UpdateGoToSeatBTNEnableState()
{
	int32 CheckedCBCount = 0;

	for (const FWdSingleDgSeatSelectionUIElem& UIElem : SelectionUIElems)
	{
		if (UIElem.SelectedStateCB != nullptr &&
			UIElem.SelectedStateCB->GetCheckedState() == ECheckBoxState::Checked)
		{
			++CheckedCBCount;
		}
	}

	if (BTN_GoToSeat != nullptr)
	{
		BTN_GoToSeat->SetIsEnabled((CheckedCBCount == 1));
	}

	if (CheckedCBCount != 1)
	{
		FString GuideStr;
		if (CheckedCBCount == 0)
		{
			GuideStr = FString::Printf(TEXT("Please choose one seat."));
		}
		else 
		{
			GuideStr = FString::Printf(TEXT("Please choose ONLY one. You have chosen %d."), CheckedCBCount);

		}
		if (TB_ChooseOnlyOneSeatInfo != nullptr)
		{
			TB_ChooseOnlyOneSeatInfo->SetText(FText::FromString(GuideStr));
		}
	}
}

void UWendyUIDungeonSeatSelection::OnGoToSeatClick()
{	
	AWendyDungeonSeat* PickedDungeonSeat = nullptr;
	for (const FWdSingleDgSeatSelectionUIElem& UIElem : SelectionUIElems)
	{
		if (UIElem.SelectedStateCB != nullptr &&
			UIElem.SelectedStateCB->GetCheckedState() == ECheckBoxState::Checked &&
			UIElem.AssociatedSeat.IsValid())
		{
			// We have done some but there might case that more than two checked.
			// Anyway just pick the first one.

			PickedDungeonSeat = UIElem.AssociatedSeat.Get();
			break;
		}
	}

	if (PickedDungeonSeat != nullptr)
	{
		AWendyCharacter* LocalChar = UWdGameplayStatics::GetLocalPlayerCharacter(this);
		if (LocalChar != nullptr)
		{
			// Here sending its position, rather than the object ptr,
			// which is hilarious because DungeonSeat will be searched again based on the information sent here.
			// I know that it is stupid, but there is something that I should learn. It can become better.
			const FVector PickedOrgPos = PickedDungeonSeat->GetOriginPos();
			LocalChar->SetPickedHomeSeatPosition(FVector2D(PickedOrgPos.X, PickedOrgPos.Y));

			// Can we expect some case that game state is other than Wendy dungeon while ther is a local wendy character?
			// In some way, this UI might be at other gamemode like Lobby, but in such case picked information should be cached in other way.
			AWendyDungeonGameState* DungeonGS = Cast<AWendyDungeonGameState>(UWdGameplayStatics::GetGameState(this));
			if (DungeonGS != nullptr)
			{
				checkf(DungeonGS->GetCurrentPhase() == EWendyDungeonPhase::WDP_SeatSelection,
					TEXT("DungeonSeatSelection should exists only in SeatSelection phase, but now %d. What is wrong?"), (int32)DungeonGS->GetCurrentPhase());

				// Advance then this UI will be destroyed..
				DungeonGS->AdvancePhase();
			}
		}
	}
}

void UWendyUIDungeonSeatSelection::OnAnySelectionCheckStateChanged(bool bChecked)
{
	// Might more than one checkboxes selected.
	// If it cannot be prevented, simply don't let it proceeed in such case.

	UpdateGoToSeatBTNEnableState();
}

float UWendyUIDungeonSeatSelection::GetCurrViewDPIScale(UObject* WorldContextObject)
{
	APlayerController* LocalPC = UWdGameplayStatics::GetLocalPlayerController(WorldContextObject);
	const UUserInterfaceSettings* UserInterfaceSettings = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	if (UserInterfaceSettings != nullptr && LocalPC != nullptr)
	{
		int32 ViewSizeX = 1920, ViewSizeY = 1080;
		LocalPC->GetViewportSize(ViewSizeX, ViewSizeY);
		return UserInterfaceSettings->GetDPIScaleBasedOnSize(FIntPoint(ViewSizeX, ViewSizeY));
	}
	return 1.0f;
}