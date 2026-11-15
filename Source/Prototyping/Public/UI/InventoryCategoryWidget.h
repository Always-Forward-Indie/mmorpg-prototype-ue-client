#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryCategoryWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCategorySelectionChanged);

/**
 * Code-built category dropdown popup (no BP setup: rows are UCheckBox
 * created in NativeConstruct, like the skill-bar drag visuals).
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

	// "All" / "Equipment" / "Equipment + Usable" / "Custom (3)".
	FText GetSummaryText() const;

	UPROPERTY(BlueprintAssignable, Category = "Category Filter Events")
	FOnCategorySelectionChanged OnSelectionChanged;

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

	static FText DisplayName(int32 BucketIndex);

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
