// Copyright Working Dungeon Wendy, by DJ Song

#pragma once

#include "CoreMinimal.h"
#include "WendyUiWidget.h"
#include "WendyUIDungeonSeatSelection.generated.h"

class AWendyDungeonSeat;
class UCanvasPanel;
class UCheckBox;
class UWdButton;
class UWdTextBlock;

/**  
* Bunch of UI elements to represent a seletable dungeon seat.
*/
USTRUCT()
struct FWdSingleDgSeatSelectionUIElem
{
	GENERATED_BODY()

	FWdSingleDgSeatSelectionUIElem()
		: OuterPanel(nullptr)
		, SelectedStateCB(nullptr)
		, AssociatedSeat(nullptr)
	{}

	/** It could be other kind of panel, but CanvasPanel is usually easy to use.. */
	UPROPERTY(Transient)
	UCanvasPanel* OuterPanel;

	/** The shit that make your choice.  */
	UPROPERTY(Transient)
	UCheckBox* SelectedStateCB;

	/** Your character will have this as its home if you checked and click proceed button. */
	TWeakObjectPtr<AWendyDungeonSeat> AssociatedSeat;
};

/** 
 * Being popup right after you enter the dungeon, to let the user choose the seat.
 */
UCLASS(Blueprintable)
class UWendyUIDungeonSeatSelection : public UWendyUiWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_MainTitle;

	/** Buttons will be generated within here.
	 * This canvas panel should be inside of another canvas panel of whole view size. */
	UPROPERTY(Transient, meta = (BindWidget))
	UCanvasPanel* CP_DungeonTopViewField;

	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_GoToSeatTitle;

	/** If you made a choice and cliked it, your selection is finally done. */
	UPROPERTY(Transient, meta = (BindWidget))
	UWdButton* BTN_GoToSeat;

	/** Some guidance to the user */
	UPROPERTY(Transient, meta = (BindWidget))
	UWdTextBlock* TB_ChooseOnlyOneSeatInfo;

	/** Each element represents the seats in wendy world, you choose one of them and click the button to proceed. */
	UPROPERTY(Transient)
	TArray<FWdSingleDgSeatSelectionUIElem> SelectionUIElems;

public:
	UWendyUIDungeonSeatSelection(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void StaticWidgetPreparations() override;
	

private:
	/** Main entry to dynamically generate the main working elements here. */
	void GenerateSeatSelectionUIs();

	/** Returns whole extent bound (like AABB in 2D plane) of all DungeonSeats placed in the world. 
	 * The returned value will be used for mapping dynamically generated UI elements all the main panel here.
	 * Return bool is false when no suitable area was found, i.e. Not enough DungeonSeat objects.
	 */
	bool CalculatesWholeSeatBound(FVector2D& OutMin, FVector2D& OutMax) const;
	/**
	 * The extent of CP_DungeonTopViewField, assuming 1.0 DPI scale?
	 */
	bool GetPanelPlacementBound(FVector2D& OutMin, FVector2D& OutMax) const;

	/** That button won't always be enabled.. */
	void UpdateGoToSeatBTNEnableState();
	
	UFUNCTION()
	void OnGoToSeatClick();

	/** All dyn generated seat selection checkboxes will notify its state change. */
	UFUNCTION()
	void OnAnySelectionCheckStateChanged(bool bChecked);

	void UpdateSeatOccupiedState();

public:
	/** Get's the DPI scale value defined by UserInterfaceSettings. Might be referred from outside..? */
	static float GetCurrViewDPIScale(UObject* WorldContextObject);

private:
};

