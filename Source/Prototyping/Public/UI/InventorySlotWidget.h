#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Data/DataStructs.h"
#include <Components/SizeBox.h>
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "InventorySlotWidget.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotRightClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotHovered, int32, SlotIndex, bool, bIsHovered);

/**
 * Individual inventory slot widget that represents a single item slot.
 *
 * Chrome model (single input path): one UButton owns every frame state via
 * BP-assigned FButtonStyle presets (Empty/Filled/Hovered/Selected/Highlighted,
 * priority Highlight > Selected > Hover > base — see FInventorySlotStyleRule).
 * No per-state Image overlays: rarity rides the RarityBorder tint, equipped is
 * a single badge, cooldown/quantity/junk logic is untouched.
 * All binds are optional — a BP predating a widget degrades, never crashes.
 */
UCLASS()
class PROTOTYPING_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

	// Initialize the slot with index
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void InitializeSlot(int32 InSlotIndex);

	// Set item data for this slot
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetItemData(const FInventoryItemStruct& Item);

	// Clear the slot
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void ClearSlot();

	// Dim the icon for junk-marked rows (sell-junk helper visual).
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetJunked(bool bJunked);

	// Get current item data
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	FInventoryItemStruct GetItemData() const { return CurrentItem; }

	// Check if slot has item
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	bool HasItem() const { return CurrentItem.itemId > 0; }

	// Get slot index
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	int32 GetSlotIndex() const { return SlotIndex; }

	// Set slot highlight
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetSlotHighlight(bool bHighlighted);

	// Set slot selection
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetSlotSelected(bool bSelected);

	// Selection state (LMB single-select lives in the parent grid).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	bool IsSlotSelected() const { return bIsSelected; }

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Inventory Slot Events")
	FOnSlotClicked OnSlotClicked;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Slot Events")
	FOnSlotRightClicked OnSlotRightClicked;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Slot Events")
	FOnSlotHovered OnSlotHovered;

	UPROPERTY(meta = (BindWidgetOptional)) USizeBox* SlotSizeBox = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", ClampMin = "8"))
	float SlotSide = 64.f;

	UFUNCTION(BlueprintCallable)
	void SetSlotSide(float InSide);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativePreConstruct() override;

	// Update visual appearance
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void UpdateSlotAppearance();

	// Load item icon
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void LoadItemIcon();

	// Update quantity text
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void UpdateQuantityText();

	// Rarity tint (single source: FInventorySlotStyleRule).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	FLinearColor GetRarityColor(const FString& Rarity) const;

protected:
	// UI Components (all optional — absence degrades, never crashes)
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SlotButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ItemIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* QuantityText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* RarityBorder = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ImgEquipped = nullptr;

	// Predictive cooldown overlay (optional in BP: add CooldownOverlay image +
	// CooldownText textblock to WBP to enable; absent = silently disabled).
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* CooldownOverlay = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* CooldownText = nullptr;

	// Button-style presets per frame state (assigned in BP defaults).
	// An unset preset (empty Normal brush) is skipped: the BP-authored
	// button style stays as-is instead of going invisible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot|Style")
	FButtonStyle EmptyStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot|Style")
	FButtonStyle FilledStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot|Style")
	FButtonStyle HoveredStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot|Style")
	FButtonStyle SelectedStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot|Style")
	FButtonStyle HighlightedStyle;

	// Slot data
	UPROPERTY(BlueprintReadOnly, Category = "Inventory Slot")
	FInventoryItemStruct CurrentItem;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory Slot")
	int32 SlotIndex = -1;

	// Predictive cooldown display (world time; server authoritative).
	double CooldownEndTime = 0.0;
	double CooldownTotalTime = 0.0;
	bool bCoolingDown = false;

public:
	/** Predictive cooldown overlay (seconds). Display only; server enforces the real CD. */
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void StartCooldown(float CooldownSeconds);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	bool IsOnCooldown() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateCooldownDisplay();

	// Visual states
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsHighlighted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsHovered = false;

	// Kept for BP compatibility (read in older Blueprint graphs).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsOccupied = false;

	// Junk-mark visual (sell-junk helper). Applied in SetJunked/SetIconTexture.
	bool bIsJunked = false;

private:
	// Handle button events (single input path; right-click stays native below)
	UFUNCTION()
	void OnSlotButtonClicked();

	UFUNCTION()
	void OnSlotButtonHovered();

	UFUNCTION()
	void OnSlotButtonUnhovered();

	/** Preset for the rule-computed frame key, or nullptr when unset. */
	const FButtonStyle* StyleForKey(const FString& Key) const;

	/** True when at least one style preset carries a Normal brush. */
	bool HasAnyPresetStyle() const;

	/** Deferred button sanity (runs once the slot is actually laid out). */
	bool bButtonSanityDone = false;
	double SanityAccum = 0.0;
	void RunButtonSanityCheck();
	bool IsAncestorChainHidden() const;

	/** Handle for async texture loading */
	TSharedPtr<FStreamableHandle> StreamableHandle;

	/** Sets the icon texture */
	void SetIconTexture(UTexture2D* Texture);

	/** Sets default/placeholder icon */
	void SetDefaultIcon();

	/** Helper function for async asset loading */
	void AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback);
};
