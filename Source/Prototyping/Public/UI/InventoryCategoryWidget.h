#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryCategoryWidget.generated.h"

class UBorder;
class UVerticalBox;
class UCheckBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCategorySelectionChanged);

/**
 * One filter row (optional BP replacement for the code-built checkbox row).
 * Subclass, bind a UCheckBox named 'RowCheckBox' + a UTextBlock named
 * 'RowLabel', assign via CategoryRowWidgetClass. Absent = code-built row.
 */
UCLASS()
class PROTOTYPING_API UCategoryRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupRow(const FText& Label, bool bChecked);
	UCheckBox* GetCheckBox() const { return RowCheckBox; }

protected:
	UPROPERTY(meta = (BindWidget))
	class UCheckBox* RowCheckBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* RowLabel = nullptr;
};

/**
 * Category dropdown popup. Two setups, same behavior:
 * - Code-built (default): zero BP setup, dark panel + check rows in code.
 * - BP subclass: provide a UVerticalBox named 'CategoryList' (rows land
 *   there) + optional UBorder 'CategoryFrame' (never restyled by code) +
 *   optional CategoryRowWidgetClass for custom rows (also pickable on the
 *   inventory itself via CategoryRowWidgetClass).
 * Assign the subclass via UInventoryWidget::CategoryFilterWidgetClass.
 *
 * Multi-select with OR semantics: All / Equipment / Usable / Quest / Other.
 * Invariants (mirrored by FInventoryCategoryRule::PassesFilter, so the grid
 * can never be emptied by construction):
 * - checking All checks every bucket; unchecking the last specific bucket
 *   falls back to All instead of showing an empty grid.
 * Owned by UInventoryWidget (viewport Z 1050, below the context menu).
 */
UCLASS()
class PROTOTYPING_API UInventoryCategoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryCategoryWidget(const FObjectInitializer& ObjectInitializer);

	// Show at the given viewport position (already DPI-divided by the caller).
	UFUNCTION(BlueprintCallable, Category = "Category Filter")
	void ShowAt(const FVector2D& ScreenViewportPos);

	UFUNCTION(BlueprintCallable, Category = "Category Filter")
	void HideFilter();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Category Filter")
	bool IsFilterOpen() const;

	// Checked buckets (session state; all-true == All).
	void GetActive(bool& bOutEquipment, bool& bOutUsable, bool& bOutQuest, bool& bOutOther) const;

	// "All" / lone bucket name / "Custom (n)" from two buckets up.
	FText GetSummaryText() const;

	UPROPERTY(BlueprintAssignable, Category = "Category Filter Events")
	FOnCategorySelectionChanged OnSelectionChanged;

	/** Clear + rebuild rows (row-class swap, preserves live selection). */
	void RebuildRows();

	// BP row design (optional): UCategoryRowWidget subclass. Code-built
	// checkbox row when unset (zero BP setup keeps working).
	UPROPERTY(EditAnywhere, Category = "Category Filter")
	TSubclassOf<UCategoryRowWidget> CategoryRowWidgetClass;

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void OnAllChecked(bool bChecked);
	UFUNCTION() void OnEquipmentChecked(bool bChecked);
	UFUNCTION() void OnUsableChecked(bool bChecked);
	UFUNCTION() void OnQuestChecked(bool bChecked);
	UFUNCTION() void OnOtherChecked(bool bChecked);

	/** Programmatic check (never rebroadcasts: SetIsChecked is silent). */
	void ApplyAndBroadcast();

	/** One row into the BP list (or the code-built default list). */
	void AddRow(const FText& Label, bool bInitial, class UCheckBox*& OutBox);

	static FText DisplayName(int32 BucketIndex);

	// BP panel (optional): rows land here when bound, else code-built panel.
	UPROPERTY(meta = (BindWidgetOptional))
	class UVerticalBox* CategoryList = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	class UBorder* CategoryFrame = nullptr;

	// Code-built list (only when no BP 'CategoryList' is bound).
	class UVerticalBox* DefaultList = nullptr;

	class UCheckBox* AllBox = nullptr;
	class UCheckBox* EquipmentBox = nullptr;
	class UCheckBox* UsableBox = nullptr;
	class UCheckBox* QuestBox = nullptr;
	class UCheckBox* OtherBox = nullptr;

	bool bEquip = true;
	bool bUsable = true;
	bool bQuest = true;
	bool bOther = true;
};
